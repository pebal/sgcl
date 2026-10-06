//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "handshake.h"
#include "server_handshake.h"
#include "record.h"
#include "revocation.h"
#include "../error.h"
#include "../../connection.h"
#include "../../../async/coroutine.h"
#include "../../../async/timer.h"
#include "../../../core/function.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

// A TLS 1.3 connection over a transport (a net::connection): the
// transport under net::connection's ConnImpl, beside the sockets. The
// handshake runs in the constructor's caller (client(), before a handle is
// given out); then a read opens records as it needs them and a write seals
// its data into records. The work without I/O — records framed, opened
// and sealed, messages fed to the handshake machine — is done by the
// methods here, and each operation that waits comes as two thin loops
// around them, a thread's (the transport's read and write) and a task's
// (their async_ forms).
//
// Full duplex, as every connection of the module: a read and a write may
// run at once (ConnImpl takes one of each at a time). What both use, the
// keys of the writing direction and the buffer a record is sealed into, is
// held under a mutex of the record: a write takes it for each record it
// sends, and a read that must send something (the answer to a KeyUpdate
// that asks for one, a fatal alert) takes it for that record, so that the
// two interleave by whole records.
//
// A failure is of its direction, each recorded in a field of its own: a
// write's (the transport refused, a write deadline) in _write_broken, under
// the record mutex, which only the writes look at; the read goes on with
// what the transport gives it (the records the framer holds, its end, its
// error). A read's (a record it cannot open, a fatal alert of the peer) in
// _read_broken, written by the read side alone (the reads are one at a
// time, ConnImpl's read lock) and read by it without a lock; it reaches the
// writes through an atomic the read side stores after it: the alert this
// side sends (_want_alert, whose exchange under the record mutex copies the
// read's error into _write_broken), or for an alert of the peer, which is
// answered with nothing, _read_fatal.
//
// The secrets (the keys of both directions, the handshake machine's) and
// every plaintext buffer are in unmanaged blocks, zeroed when done with;
// the object itself is managed (it holds the transport's handle and the
// handshake's result).
namespace sgcl::net::tls::detail {
    using sgcl::io::detail::fail;

    // The bytes of the write queue: a vector whose growth writes nothing
    // into the new room (a record is sealed into it at once; a zeroing
    // std::vector<uint8_t> wrote every byte of a large response's records
    // twice)
    template<class T>
    struct NoZeroAllocator : std::allocator<T> {
        template<class U>
        struct rebind {
            using other = NoZeroAllocator<U>;
        };

        NoZeroAllocator() noexcept = default;

        template<class U>
        SGCL_INLINE_HOT NoZeroAllocator(const NoZeroAllocator<U>&) noexcept {
        }

        template<class U>
        SGCL_INLINE_HOT void construct(U* p) noexcept {
            ::new (static_cast<void*>(p)) U;
        }

        template<class U, class... A>
        SGCL_INLINE_HOT void construct(U* p, A&&... a) noexcept(std::is_nothrow_constructible_v<U, A...>) {
            ::new (static_cast<void*>(p)) U(std::forward<A>(a)...);
        }
    };

    using QueueBytes = std::vector<uint8_t, NoZeroAllocator<uint8_t>>;

    // What a server's first ClientHello asked for, given to the program's
    // choice of an identity (tls::config::identity_for)
    struct HelloInfo {
        string server_name;                     // SNI; empty: none
        vector<string> alpn;                    // offered, in the client's order
    };

    // The identities chosen for one connection, as the machine signs with
    // them, and what keeps their keys
    struct SelectedIdentities {
        vector<ServerIdentity> identities;
        vector<tracked_ptr<const void>> keep;
    };

    using IdentitySelector = function<async::task<expected<SelectedIdentities, io::error>>(HelloInfo)>;

    class TlsImpl final : public net::detail::ConnImpl {
    public:
        SGCL_INLINE_HOT TlsImpl(const net::connection& transport, const ClientSettings& settings) noexcept
        : _transport(transport)
        , _b(new Block()) {
            _hs.emplace(settings, Entropy(), Clock());
        }

        // A client whose identities' keys live in what `keep` holds, and
        // whose NewSessionTickets go to the cache under `key` (none: the
        // tickets passed over)
        SGCL_INLINE_HOT TlsImpl(const net::connection& transport, const ClientSettings& settings, const vector<tracked_ptr<const void>>& keep,
                                const tracked_ptr<SessionCacheState>& cache, const string& key) noexcept
        : _transport(transport)
        , _b(new Block())
        , _keep(keep)
        , _cache(cache)
        , _cache_key(key) {
            _hs.emplace(settings, Entropy(), Clock());
        }

        // The server's side; `keep` holds what the identities' keys live in
        // (the machine signs through pointers into them). With `select`, the
        // identities are its choice for the first ClientHello, made before
        // that hello is fed to the machine
        SGCL_INLINE_HOT TlsImpl(const net::connection& transport, const ServerSettings& settings, const vector<tracked_ptr<const void>>& keep,
                                const tracked_ptr<const IdentitySelector>& select = {}) noexcept
        : _transport(transport)
        , _b(new Block())
        , _keep(keep)
        , _select(select) {
            _server.emplace(settings, Entropy());
        }

        SGCL_INLINE_HOT bool is_server() const noexcept {
            return (bool)_server;
        }

        // --- the handshake --------------------------------------------------

        // Done before the handle is given out; `deadline` bounds the whole
        // of it (the transport's deadlines are set to it, then removed)
        SGCL_INLINE_HOT expected<void, io::error> handshake(time_point deadline) {
            _transport.set_deadline(deadline);
            auto r = _block_handshake();
            _transport.set_deadline(time_point());
            return r;
        }

        async::task<expected<void, io::error>> async_handshake(time_point deadline) noexcept {
            _transport.set_deadline(deadline);
            auto r = co_await _co_handshake();
            _transport.set_deadline(time_point());
            co_return r;
        }

        // The client's result (is_server() false) and the server's
        SGCL_INLINE_HOT const ClientResult& result() const noexcept {
            return _hs->result();
        }

        SGCL_INLINE_HOT const ServerResult& server_result() const noexcept {
            return _server->result();
        }

        // The revocation status of the peer's chain the connection checked
        // after the handshake (tls.h, by the config), and its source;
        // nullopt: not checked
        SGCL_INLINE_HOT void set_revocation(optional<crypto::x509::revocation_status> status, uint8_t source) noexcept {
            _revocation = status;
            _revocation_source = source;
        }

        SGCL_INLINE_HOT optional<crypto::x509::revocation_status> revocation() const noexcept {
            return _revocation;
        }

        SGCL_INLINE_HOT uint8_t revocation_source() const noexcept {
            return _revocation_source;
        }

        // A fatal alert after the handshake, before the connection is given
        // out (the peer's chain refused by its revocation check): written
        // without waiting under the traffic keys, as close() writes
        // close_notify, then the transport closed
        void abort(AlertDescription d) noexcept {
            if (_closing.exchange(true)) {
                (void)_transport.close();
                return;
            }
            if (_record.try_lock()) {
                if (_flush_quiet() && !_write_closed && _b->write.installed()) {
                    const uint8_t bytes[2] = {2, uint8_t(d)};
                    _append(ContentType::alert, bytes, 2);
                    _write_closed = true;
                    (void)_flush_quiet();
                }
                _record.unlock();
            }
            (void)_transport.close();
        }

        // --- the transport's operations ---------------------------------------

        expected<size_t, io::error> raw_read(const slice<byte>& buffer) override {
            for (;;) {
                auto s = _read_step(buffer);
                if (s.kind == ReadStep::Kind::bytes) {
                    return s.n;
                }
                if (s.kind == ReadStep::Kind::failed) {
                    return fail(s.error);
                }
                if (s.kind == ReadStep::Kind::reply) {
                    // a failure of this write is the write side's (recorded
                    // there): the read goes on to its own end
                    std::lock_guard<sgcl::async::mutex> g(_record);
                    (void)_queue_wants();
                    (void)_flush_block();
                    continue;
                }
                // more bytes from the transport, its raw read: this object is
                // its one user, and its own mutexes are the direction's
                auto n = _t().raw_read(_b->framer.room());
                if (!n) {
                    return fail(n);
                }
                if (auto e = _took(*n); e) {
                    return fail(*e);
                }
            }
        }

        async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> buffer) noexcept override {
            for (;;) {
                auto s = _read_step(buffer);
                if (s.kind == ReadStep::Kind::bytes) {
                    co_return s.n;
                }
                if (s.kind == ReadStep::Kind::failed) {
                    co_return fail(s.error);
                }
                if (s.kind == ReadStep::Kind::reply) {
                    auto g = co_await _record.scoped_lock();
                    (void)_queue_wants();
                    while (_unsent_size()) {
                        auto w = co_await _t().awaited_raw_write(_unsent());
                        if (!w) {
                            (void)_break(w.error());   // the write side's: the read goes on
                            break;
                        }
                        _sent(*w);
                    }
                    continue;
                }
                auto n = co_await _t().awaited_raw_read(_b->framer.room());
                if (!n) {
                    co_return fail(n);
                }
                if (auto e = _took(*n); e) {
                    co_return fail(*e);
                }
            }
        }

        // A read without waiting, for a reader that waits for readiness
        // itself (the http server's head, without a frame): the plaintext
        // held, else the records the framer holds, else one try of the
        // transport and the records it brought; nullopt when no whole record
        // is there. What the read side must write (a KeyUpdate's answer, an
        // alert) is written when the record is free, else by the next write
        //
        // The transport is tried until it would block: its readiness is
        // edge-triggered (async/reactor.h), so a wait may begin only after a
        // read that found nothing. A read that brought part of a record (a
        // record larger than the small block, the rest already in the
        // socket) and then a wait would wait for an edge that never comes
        expected<optional<size_t>, io::error> try_raw_read(const slice<byte>& buffer) override {
            for (;;) {
                auto s = _read_step(buffer);
                if (s.kind == ReadStep::Kind::bytes) {
                    return optional<size_t>(s.n);
                }
                if (s.kind == ReadStep::Kind::failed) {
                    return fail(s.error);
                }
                if (s.kind == ReadStep::Kind::reply) {
                    _drain_try();
                    continue;
                }
                auto n = _t().try_raw_read(_b->framer.room());
                if (!n) {
                    return fail(n);
                }
                if (!*n) {
                    _b->framer.release();
                    return optional<size_t>();
                }
                if (auto e = _took(**n); e) {
                    return fail(*e);
                }
            }
        }

        // The transport's readiness: asked for only when no whole record is
        // held (try_raw_read gave nullopt)
        net::detail::readiness raw_readable() noexcept override {
            return _t().raw_readable();
        }

        expected<size_t, io::error> raw_write(const slice<const byte>& data) override {
            size_t done = 0;
            bool first = true;
            for (;;) {
                std::lock_guard<sgcl::async::mutex> g(_record);
                if (auto e = _writable(); e) {
                    return fail(*e);
                }
                if (first) {
                    done = _skip_sealed(data);
                    first = false;
                }
                const bool over = _queue_wants();   // the read side's alert: sent, then no data
                if (!over && done < data.size()) {
                    done += _seal_record(data, done);
                }
                if (auto e = _flush_block(); e) {
                    return fail(*e);
                }
                if (over) {
                    return fail(*_write_broken);
                }
                if (done == data.size()) {
                    _trim();
                    break;
                }
            }
            _drain_try();
            return data.size();
        }

        async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) noexcept override {
            size_t done = 0;
            bool first = true;
            for (;;) {
                auto g = co_await _record.scoped_lock();
                if (auto e = _writable(); e) {
                    co_return fail(*e);
                }
                if (first) {
                    done = _skip_sealed(data);
                    first = false;
                }
                const bool over = _queue_wants();
                if (!over && done < data.size()) {
                    done += _seal_record(data, done);
                }
                while (_unsent_size()) {
                    auto w = co_await _t().awaited_raw_write(_unsent());
                    if (!w) {
                        co_return fail(_break(w.error()));
                    }
                    _sent(*w);
                }
                if (over) {
                    co_return fail(*_write_broken);
                }
                if (done == data.size()) {
                    _trim();
                    break;
                }
            }
            _drain_try();
            co_return data.size();
        }

        // A write begun without waiting (ConnImpl::start_write): records
        // sealed and given to the transport as long as it takes them whole.
        // A record it takes in part is kept, its tail sent before anything
        // else, and its plaintext counted as not taken yet: the rest of the
        // write (the same data, from there) finds it sealed and does not
        // seal it again
        expected<size_t, io::error> try_raw_write(const slice<const byte>& data) override {
            if (!_record.try_lock()) {
                return size_t(0);
            }
            size_t taken = 0;
            optional<io::error> error = _writable();
            if (!error) {
                const bool over = _queue_wants();
                auto f = _flush_try();
                if (!f) {
                    error = f.error();
                } else if (over) {
                    error = _write_broken;
                } else if (*f) {
                    while (taken < data.size()) {
                        size_t n = _seal_record(data, taken);
                        auto r = _flush_try();
                        if (!r) {
                            error = r.error();
                            break;
                        }
                        if (!*r) {
                            _sealed_at = data.data() + taken;
                            _sealed_size = n;
                            break;
                        }
                        taken += n;
                    }
                }
            }
            _trim();
            _record.unlock();
            _drain_try();
            if (error) {
                return fail(*error);
            }
            return taken;
        }

        // The pieces of a write (ConnImpl::start_write_parts): each record's
        // plaintext copied from the pieces straight into its place in the
        // queue and sealed there, so the bytes of a response's body go from
        // their blocks into the records with one copy. As try_raw_write
        // otherwise: a record taken in part is kept, its plaintext counted
        // as not taken, and the rest of the write finds it sealed
        expected<size_t, io::error> try_raw_write_parts(const slice<const byte>* parts, size_t n) override {
            if (!_record.try_lock()) {
                return size_t(0);
            }
            size_t total = 0;
            for (size_t i = 0; i < n; ++i) {
                total += parts[i].size();
            }
            size_t taken = 0;
            optional<io::error> error = _writable();
            if (!error) {
                const bool over = _queue_wants();
                auto f = _flush_try();
                if (!f) {
                    error = f.error();
                } else if (over) {
                    error = _write_broken;
                } else if (*f) {
                    // records sealed up to BatchBytes of them, then given
                    // to the transport in one write (a large response in two
                    // or three writes, where a write a record was five for
                    // 64 KB); the plaintext of a batch it does not take whole
                    // is counted as not taken, sealed already
                    while (taken < total) {
                        size_t k = 0;
                        do {
                            k += _seal_parts(parts, n, total, taken + k);
                        } while (taken + k < total && _unsent_size() < BatchBytes);
                        auto r = _flush_try();
                        if (!r) {
                            error = r.error();
                            break;
                        }
                        if (!*r) {
                            _sealed_at = n ? parts[0].data() : nullptr;
                            _sealed_from = taken;
                            _sealed_size = k;
                            break;
                        }
                        taken += k;
                    }
                }
            }
            _trim();
            _record.unlock();
            _drain_try();
            if (error) {
                return fail(*error);
            }
            return taken;
        }

        async::task<expected<size_t, io::error>> awaited_raw_write_parts(vector<slice<const byte>> parts, size_t from) noexcept override {
            size_t total = 0;
            for (auto& p : parts) {
                total += p.size();
            }
            size_t done = from;
            bool first = true;
            for (;;) {
                auto g = co_await _record.scoped_lock();
                if (auto e = _writable(); e) {
                    co_return fail(*e);
                }
                if (first) {
                    done += _skip_sealed_parts(parts.empty() ? nullptr : parts[0].data(), from);
                    first = false;
                }
                const bool over = _queue_wants();
                if (!over && done < total) {
                    done += _seal_parts(parts.data(), parts.size(), total, done);
                }
                while (_unsent_size()) {
                    auto w = co_await _t().awaited_raw_write(_unsent());
                    if (!w) {
                        co_return fail(_break(w.error()));
                    }
                    _sent(*w);
                }
                if (over) {
                    co_return fail(*_write_broken);
                }
                if (done == total) {
                    _trim();
                    break;
                }
            }
            _drain_try();
            co_return total - from;
        }

        // close_notify without waiting (after the tail of a record in part
        // sent, never inside it; none when another write holds the record or
        // the socket does not take it at once), then the transport closed.
        // Nothing of the read side's state is read or written here: close()
        // runs beside a read in progress (a server closing a connection its
        // task reads); a write that fails here (the peer gone) is not
        // recorded (the transport's close ends both directions anyway), and
        // close_notify is not held back by a failure the read side recorded
        expected<void, io::error> close() noexcept override {
            if (_closing.exchange(true)) {
                return _transport.close();
            }
            if (_record.try_lock()) {
                if (_flush_quiet() && !_write_closed && _b->write.installed()) {
                    const uint8_t bytes[2] = {1, 0};
                    _append(ContentType::alert, bytes, 2);
                    _write_closed = true;
                    (void)_flush_quiet();
                }
                _record.unlock();
            }
            return _transport.close();
        }

        // The same in a task: the record waited for, a tail and the
        // close_notify written, 100 ms for all of it, then the transport
        // closed
        async::task<expected<void, io::error>> async_close() noexcept override {
            if (_closing.exchange(true)) {
                co_return _transport.close();
            }
            const time_point until = sgcl::clock::now() + 100 * millisecond;
            bool locked = _record.try_lock();
            while (!locked && sgcl::clock::now() < until) {
                co_await async::sleep(millisecond);
                locked = _record.try_lock();
            }
            if (locked) {
                _queue_close_notify();
                _transport.set_write_deadline(until);
                while (_unsent_size()) {
                    auto w = co_await _t().awaited_raw_write(_unsent());
                    if (!w) {
                        break;
                    }
                    _sent(*w);
                }
                _record.unlock();
            }
            co_return _transport.close();
        }

        bool is_closed() const noexcept override {
            return _transport.is_closed();
        }

        // close_notify, then the transport's writing half ended. It runs
        // beside a read in progress (an HTTP/2 connection's writer ending its
        // half while its reader reads): a write that fails here is the write
        // side's (_write_broken), and the read goes on with what the
        // transport gives it
        expected<void, io::error> close_write() override {
            {
                std::lock_guard<sgcl::async::mutex> g(_record);
                _queue_close_notify();
                if (auto e = _flush_block(); e) {
                    return fail(*e);
                }
                _write_closed = true;
            }
            return _transport.close_write();
        }

        void set_deadline(int dir, time_point t) noexcept override {
            net::detail::ConnectionAccess::impl(_transport).set_deadline(dir, t);
        }

        time_point deadline(int dir) const noexcept override {
            return net::detail::ConnectionAccess::impl(_transport).deadline(dir);
        }

        endpoint local_endpoint() const noexcept override {
            return _transport.local_endpoint();
        }

        endpoint remote_endpoint() const noexcept override {
            return _transport.remote_endpoint();
        }

        string path() const noexcept override {
            return _transport.path();
        }

        expected<void, io::error> set_no_delay(bool on) noexcept override {
            return _transport.set_no_delay(on);
        }

        expected<void, io::error> set_keep_alive(std::chrono::nanoseconds idle) noexcept override {
            return _transport.set_keep_alive(duration(idle));
        }

        string describe() const noexcept override {
            auto t = net::detail::ConnectionAccess::impl(_transport).describe();
            return string("tls ") + t;
        }

    private:
        // What the record layer keeps: unmanaged, zeroed
        struct Block {
            RecordProtection read, write;
            RecordFramer framer;
            HandshakeAssembler assembler;
            Epoch read_epoch = Epoch::initial;
            std::vector<uint8_t> out;               // the handshake's records to send
            std::vector<uint8_t> ticket;            // a server's NewSessionTicket, sent before its first data
            QueueBytes queue;                       // records sealed and not yet taken by the transport (under the record mutex)
            size_t queue_sent = 0;
            uint8_t* plain = nullptr;               // a record's plaintext not yet read: a large block of RecordBlocks while held
            size_t plain_at = 0, plain_end = 0;

            SGCL_INLINE_HOT ~Block() {
                crypto::detail::secure_zero(out.data(), out.size());
                if (plain) {
                    RecordBlocks::give(plain, RecordBlocks::Large, plain_end);
                }
            }
        };

        struct ReadStep {
            enum class Kind { bytes, more, reply, failed, again };
            Kind kind = Kind::more;
            size_t n = 0;
            io::error error;
        };

        net::connection _transport;
        // The block goes after its destructor has zeroed what it held; the
        // zeroing probe (record.h) sees its memory then, before it is freed
        struct BlockRelease {
            SGCL_INLINE_HOT void operator()(Block* b) const noexcept {
                b->~Block();
                ZeroingProbe::on_release(b, sizeof(Block), ZeroingProbe::connection_block);
                ::operator delete(static_cast<void*>(b));
            }
        };

        std::unique_ptr<Block, BlockRelease> _b;
        optional<ClientHandshake> _hs;          // the client's machine, or
        optional<ServerHandshake> _server;      // the server's
        vector<tracked_ptr<const void>> _keep;  // the identities (a server's, a client's), the ticket keys
        tracked_ptr<const IdentitySelector> _select;   // a server's choice of identities per hello, when the config has one
        std::vector<byte> _hello;               // the first ClientHello, held while the identities are chosen
        HelloInfo _hello_info;
        bool _choosing = false;                 // a hello held, the choice to make
        bool _chosen = false;                   // the choice made (or none to make)
        tracked_ptr<SessionCacheState> _cache;  // a client's sessions, when it keeps them
        string _cache_key;
        bool _ccs_on = false;
        sgcl::async::mutex _record;
        std::atomic<bool> _closing = false;
        bool _eof = false;                      // close_notify, or the transport's end at a record's boundary
        bool _write_closed = false;
        // the read side's failure, written once, by it alone (and the
        // handshake's, on its one thread before the handle is given out):
        // the reads after it fail with it
        optional<io::error> _read_broken;
        // the write side's (under the record mutex): its own, or the read's
        // copied when the read's alert is taken or _read_fatal seen; the
        // writes after it fail with it
        optional<io::error> _write_broken;
        std::atomic<bool> _read_fatal = false;  // the peer's fatal alert read: stored after _read_broken
        // what the read side needs written (under the record mutex); an
        // alert is stored after _read_broken
        static constexpr uint8_t NoAlert = 0xFF;
        std::atomic<bool> _want_key_update = false;
        std::atomic<uint8_t> _want_alert = NoAlert;
        optional<crypto::x509::revocation_status> _revocation;   // the peer's chain's, as checked after the handshake
        uint8_t _revocation_source = 0;         // RevocationSource
        const byte* _sealed_at = nullptr;       // a record of a write that tried: its plaintext, sealed, its tail queued
        size_t _sealed_size = 0;
        size_t _sealed_from = 0;                // of a write of pieces: where in them the sealed record starts

        // --- the handshake's two loops ----------------------------------------

        expected<void, io::error> _block_handshake() {
            if (auto e = _start(); e) {
                _drop_out();   // nothing was sent yet: no alert either
                return fail(*e);
            }
            for (;;) {
                if (!_b->out.empty()) {
                    auto w = _t().raw_write(_pending_out());
                    _drop_out();
                    if (!w) {
                        return fail(w);
                    }
                }
                if (_read_broken) {
                    return fail(*_read_broken);
                }
                if (_established()) {
                    _b->read.accept_ccs(false);
                    return {};
                }
                auto n = _t().raw_read(_b->framer.room());
                if (!n) {
                    return fail(n);
                }
                if (*n == 0) {
                    return fail(io::error(io::errc::unexpected_eof, "handshake", describe()));
                }
                _b->framer.commit(*n);
                _handshake_records();
                if (_choosing) {
                    _take_identities((*_select)(std::move(_hello_info)).wait());
                }
            }
        }

        async::task<expected<void, io::error>> _co_handshake() noexcept {
            if (auto e = _start(); e) {
                _drop_out();
                co_return fail(*e);
            }
            for (;;) {
                if (!_b->out.empty()) {
                    auto w = co_await _t().awaited_raw_write(_pending_out());
                    _drop_out();
                    if (!w) {
                        co_return fail(w);
                    }
                }
                if (_read_broken) {
                    co_return fail(*_read_broken);
                }
                if (_established()) {
                    _b->read.accept_ccs(false);
                    co_return expected<void, io::error>();
                }
                auto n = co_await _t().awaited_raw_read(_b->framer.room());
                if (!n) {
                    co_return fail(n);
                }
                if (*n == 0) {
                    co_return fail(io::error(io::errc::unexpected_eof, "handshake", describe()));
                }
                _b->framer.commit(*n);
                _handshake_records();
                if (_choosing) {
                    _take_identities(co_await (*_select)(std::move(_hello_info)));
                }
            }
        }

        // --- the handshake without I/O ------------------------------------------

        // The first ClientHello held for the choice of identities: its server
        // name and protocols read (a hello that does not read goes to the
        // machine as it is, which refuses it with the alert it deserves)
        bool _hold_hello(const slice<const byte>& m) noexcept {
            _chosen = true;
            // the hello answered: an ECH ClientHelloInner when it opens (its
            // name and protocols are the ones asked for), else the hello
            auto h = read_handshake(_server->hello_answered(m));
            if (!h || HandshakeType(h->type) != HandshakeType::client_hello) {
                return false;
            }
            auto ch = read_client_hello(h->body);
            if (!ch) {
                return false;
            }
            HelloInfo info;
            if (auto sn = ch->extensions.find(ExtensionType::server_name)) {
                auto host = read_server_name(*sn);
                if (!host) {
                    return false;
                }
                info.server_name = string(std::string_view(reinterpret_cast<const char*>(host->data()), host->size()));
            }
            if (auto p = ch->extensions.find(ExtensionType::application_layer_protocol_negotiation)) {
                auto offered = read_protocols(*p);
                if (!offered) {
                    return false;
                }
                for (auto name : *offered) {
                    info.alpn.push_back(string(std::string_view(reinterpret_cast<const char*>(name.data()), name.size())));
                }
            }
            _hello.assign(m.data(), m.data() + m.size());
            _hello_info = std::move(info);
            _choosing = true;
            return true;
        }

        // The program's choice made: the hello fed with the identities, or
        // the handshake over (internal_error, as Go's server answers a
        // GetCertificate that fails) with the choice's own error
        void _take_identities(expected<SelectedIdentities, io::error> r) {
            _choosing = false;
            if (!r || r->identities.empty()) {
                _fail_local(Alert{AlertDescription::internal_error, 0, "no identity for the hello"}, "handshake");
                if (!r) {
                    _read_broken = r.error();
                }
                return;
            }
            for (auto& k : r->keep) {
                _keep.push_back(k);
            }
            _server->set_identities(std::move(r->identities));
            std::vector<byte> hello = std::move(_hello);
            _run(_server->feed(bytes_of(hello.data(), hello.size())));
            if (!_ccs_on) {
                _ccs_on = true;   // after the first ClientHello, to the client's Finished (§D.4)
                _b->read.accept_ccs(true);
            }
            _drain_messages();
            _handshake_records();
        }

        // The whole messages the assembler holds, fed in turn
        void _drain_messages() {
            while (!_read_broken) {
                auto m = _b->assembler.next();
                if (!m) {
                    break;
                }
                if (_select && !_chosen && _hold_hello(*m)) {
                    break;   // the identities chosen first (_take_identities feeds it)
                }
                _run(_feed_machine(*m));
                if (_server && !_ccs_on) {
                    _ccs_on = true;   // after the first ClientHello, to the client's Finished (§D.4)
                    _b->read.accept_ccs(true);
                }
                if (_established()) {
                    break;
                }
            }
        }

        // The client's first flight; nothing for a server, which waits for
        // the ClientHello (and takes a change_cipher_spec only after it)
        SGCL_INLINE_HOT optional<io::error> _start() {
            if (_server) {
                return nullopt;
            }
            _b->read.accept_ccs(true);   // from the ClientHello to the server's Finished (§D.4)
            _run(_hs->start());
            return _read_broken;
        }

        SGCL_INLINE_HOT bool _established() const noexcept {
            return _server ? _server->established() : _hs->established();
        }

        SGCL_INLINE_HOT const Step& _feed_machine(const slice<const byte>& m) {
            return _server ? _server->feed(m) : _hs->feed(m);
        }

        SGCL_INLINE_HOT void _on_alert(const Alert& a) noexcept {
            if (_server) {
                _server->on_record_alert(a);
            } else {
                _hs->on_record_alert(a);
            }
        }

        SGCL_INLINE_HOT crypto::x509::reason _verify_reason() const noexcept {
            return _server ? _server->verify_reason() : _hs->verify_reason();
        }

        SGCL_INLINE_HOT slice<const byte> _pending_out() const noexcept {
            return bytes_of(_b->out.data(), _b->out.size());
        }

        SGCL_INLINE_HOT void _drop_out() noexcept {
            crypto::detail::secure_zero(_b->out.data(), _b->out.size());
            std::vector<uint8_t>().swap(_b->out);   // the handshake's flights: nothing kept after them
        }

        // The records of one content into the handshake's output
        void _queue(RecordProtection& w, ContentType type, const uint8_t* p, size_t n) noexcept {
            size_t at = 0;
            do {
                size_t k = std::min(n - at, MaxPlaintext);
                size_t before = _b->out.size();
                _b->out.resize(before + w.sealed_size(type, k));
                w.seal(type, bytes_of(p + at, k), _b->out.data() + before);
                at += k;
            } while (at < n);
        }

        // The machine's actions during the handshake (nothing else writes)
        void _run(const Step& step) noexcept {
            for (auto& a : step.actions) {
                switch (a.kind) {
                case Action::Kind::send: {
                    auto b = step.bytes(a);
                    if (_server && _server->established()) {
                        // the NewSessionTicket goes with the server's first
                        // write: a client that writes and closes without a
                        // read would otherwise close over a ticket unread,
                        // which its kernel answers with a reset, and a ticket
                        // of a connection that never answers is never used
                        _b->ticket.assign(reinterpret_cast<const uint8_t*>(b.data()), reinterpret_cast<const uint8_t*>(b.data()) + b.size());
                        break;
                    }
                    _queue(_b->write, ContentType::handshake, reinterpret_cast<const uint8_t*>(b.data()), b.size());
                    break;
                }
                case Action::Kind::change_cipher_spec: {
                    const uint8_t one = 1;
                    _queue(_b->write, ContentType::change_cipher_spec, &one, 1);
                    break;
                }
                case Action::Kind::install_read:
                    if (a.tls12) {
                        _b->read.install12(a.cipher, a.secret);
                    } else {
                        _b->read.install(a.cipher, a.secret);
                    }
                    _b->read_epoch = a.epoch;
                    if (auto k = _b->assembler.on_key_change(); !k) {
                        _fail_local(k.error(), "handshake");
                        return;
                    }
                    break;
                case Action::Kind::install_write:
                    if (a.tls12) {
                        _b->write.install12(a.cipher, a.secret);
                    } else {
                        _b->write.install(a.cipher, a.secret);
                    }
                    break;
                case Action::Kind::skip_early_data:
                    _b->read.skip_undecryptable(a.size);   // 0-RTT refused (§4.2.10)
                    break;
                case Action::Kind::new_ticket:   // a TLS 1.2 session, at the end of its handshake
                    _keep_session(step, a);
                    break;
                case Action::Kind::update_read:
                case Action::Kind::update_write:
                case Action::Kind::established:
                    break;
                case Action::Kind::alert:
                    _fail_local(Alert{a.alert, 0, a.what}, "handshake");
                    return;
                }
            }
        }

        // The records the framer holds, up to the handshake's end (the
        // records after the server's Finished are the application's: left
        // for the first read)
        void _handshake_records() {
            while (!_read_broken && !_established() && !_choosing) {
                auto rec = _b->framer.next();
                if (!rec) {
                    _fail_local(rec.error(), "handshake");
                    return;
                }
                if (rec->empty()) {
                    return;
                }
                auto o = _b->read.open(reinterpret_cast<uint8_t*>(rec->data()), rec->size());
                if (!o) {
                    _fail_local(o.error(), "handshake");
                    return;
                }
                switch (o->type) {
                case ContentType::invalid:
                    break;
                case ContentType::change_cipher_spec:
                    if (!_server) {
                        _run(_hs->change_cipher_spec());   // TLS 1.2's read keys; 1.3's compatibility record: nothing
                    }
                    break;
                case ContentType::handshake:
                    if (auto p = _b->assembler.push(o->fragment, _b->read_epoch); !p) {
                        _fail_local(p.error(), "handshake");
                        return;
                    }
                    _drain_messages();
                    break;
                case ContentType::alert: {
                    auto a = read_alert(o->fragment);
                    if (!a) {
                        _fail_local(a.error(), "handshake");
                        return;
                    }
                    _on_alert(*a);
                    // before the server's hello (its records still in the
                    // clear): a range of its own, a server without TLS 1.3
                    // among the senders
                    _read_broken = !_server && !_hs->hello_received() ? before_hello_error(a->description, "handshake", describe())
                                                                      : remote_error(a->description, "handshake", describe());
                    break;
                }
                default:
                    _fail_local(Alert{AlertDescription::unexpected_message, 0, "application data before the handshake's end"}, "handshake");
                    return;
                }
                _b->framer.consume();
            }
        }

        // A fatal alert of this side: sent (queued with the handshake's
        // output, or by the read side under the record mutex), and the
        // connection over
        void _fail_local(const Alert& a, const char* op) noexcept {
            if (_read_broken) {
                return;
            }
            if (!_established()) {
                if (_hs && !_hs->failed()) {
                    // a failure of the records, not of the machine (whose own
                    // alert comes here through _run): the machine ends too,
                    // and after the server's hello the keys for the alert go
                    // first, as for its own
                    for (auto& k : _hs->fail(a).actions) {
                        if (k.kind == Action::Kind::change_cipher_spec) {
                            const uint8_t one = 1;
                            _queue(_b->write, ContentType::change_cipher_spec, &one, 1);
                        } else if (k.kind == Action::Kind::install_write) {
                            _b->write.install(k.cipher, k.secret);
                        }
                    }
                }
                const uint8_t bytes[2] = {uint8_t(Alert{a.description}.fatal() ? 2 : 1), uint8_t(a.description)};
                _queue(_b->write, ContentType::alert, bytes, 2);
            }
            if (_verify_reason() != crypto::x509::reason::none) {
                _read_broken = certificate_error(_verify_reason(), op, describe());
            } else {
                _read_broken = local_error(a.description, op, describe());
            }
        }

        // --- reading, without I/O -------------------------------------------------

        SGCL_INLINE_HOT optional<io::error> _took(size_t n) noexcept {
            if (n == 0) {
                if (_b->framer.buffered() > 0) {
                    return io::error(io::errc::unexpected_eof, "read", describe());
                }
                _eof = true;   // no close_notify: the end of the stream, as the transport's
                return nullopt;
            }
            _b->framer.commit(n);
            return nullopt;
        }

        // One step of a read: bytes for the reader, a need of more from
        // the transport, something to write first, or the end
        ReadStep _read_step(const slice<byte>& buffer) noexcept {
            for (;;) {
                auto s = _read_once(buffer);
                if (s.kind != ReadStep::Kind::again) {
                    return s;
                }
            }
        }

        ReadStep _read_once(const slice<byte>& buffer) noexcept {
            ReadStep s;
            Block& b = *_b;
            if (b.plain_at < b.plain_end) {
                size_t n = std::min(buffer.size(), b.plain_end - b.plain_at);
                std::memcpy(buffer.data(), b.plain + b.plain_at, n);
                b.plain_at += n;
                if (b.plain_at == b.plain_end) {   // all of it read: the block back, zeroed
                    RecordBlocks::give(b.plain, RecordBlocks::Large, b.plain_end);
                    b.plain = nullptr;
                    b.plain_at = b.plain_end = 0;
                }
                s.kind = ReadStep::Kind::bytes;
                s.n = n;
                return s;
            }
            if (_read_broken) {   // its own field: a write's failure never ends a read
                s.kind = ReadStep::Kind::failed;
                s.error = *_read_broken;
                return s;
            }
            if (_eof) {
                s.kind = ReadStep::Kind::bytes;
                s.n = 0;
                return s;
            }
            auto rec = b.framer.next();
            if (!rec) {
                _read_failed(rec.error());
                s.kind = ReadStep::Kind::reply;
                return s;
            }
            if (rec->empty()) {
                return s;   // more
            }
            uint8_t* r = reinterpret_cast<uint8_t*>(rec->data());
            const size_t size = rec->size();
            // the plaintext straight into the reader's buffer when all of
            // the record's fits there, else through the unmanaged buffer
            const bool direct = size >= HeaderSize + TagSize && buffer.size() >= size - HeaderSize - TagSize && b.read.installed();
            auto o = direct ? b.read.open(r, size, reinterpret_cast<uint8_t*>(buffer.data())) : b.read.open(r, size);
            if (!o) {
                _read_failed(o.error());
                s.kind = ReadStep::Kind::reply;
                return s;
            }
            const ContentType type = o->type;
            if (type == ContentType::application_data) {
                const size_t n = o->fragment.size();
                if (direct) {
                    b.framer.consume();
                    if (n == 0) {
                        s.kind = ReadStep::Kind::again;   // an empty record: read on
                        return s;
                    }
                    s.kind = ReadStep::Kind::bytes;
                    s.n = n;
                    return s;
                }
                if (n == 0) {
                    b.framer.consume();
                    s.kind = ReadStep::Kind::again;   // an empty record: read on
                    return s;
                }
                b.plain = RecordBlocks::take(RecordBlocks::Large);
                std::memcpy(b.plain, o->fragment.data(), n);
                b.plain_at = 0;
                b.plain_end = n;
                b.framer.consume();
                s.kind = ReadStep::Kind::again;
                return s;
            }
            // control records: their bytes out of the reader's buffer
            // once taken
            if (type == ContentType::handshake) {
                auto p = b.assembler.push(o->fragment, Epoch::application);
                if (direct) {
                    crypto::detail::secure_zero(buffer.data(), size - HeaderSize - TagSize);
                }
                b.framer.consume();
                if (!p) {
                    _read_failed(p.error());
                    s.kind = ReadStep::Kind::reply;
                    return s;
                }
                while (auto m = b.assembler.next()) {
                    if (!_after_handshake(*m)) {
                        break;
                    }
                }
                s.kind = _wants() ? ReadStep::Kind::reply : ReadStep::Kind::again;
                return s;
            }
            if (type == ContentType::alert) {
                auto a = read_alert(o->fragment);
                if (direct) {
                    crypto::detail::secure_zero(buffer.data(), size - HeaderSize - TagSize);
                }
                b.framer.consume();
                if (!a) {
                    _read_failed(a.error());
                    s.kind = ReadStep::Kind::reply;
                    return s;
                }
                if (a->description == AlertDescription::close_notify) {
                    _eof = true;
                    s.kind = ReadStep::Kind::again;
                return s;
                }
                if (a->description == AlertDescription::user_canceled) {
                    s.kind = ReadStep::Kind::again;
                return s;   // a warning; the close_notify follows
                }
                // the peer's fatal alert: nothing sent back, the writes
                // after it refused (the error first, then the flag)
                _read_broken = remote_error(a->description, "read", describe());
                _read_fatal.store(true, std::memory_order_release);
                s.kind = ReadStep::Kind::again;
                return s;
            }
            b.framer.consume();   // a compatibility change_cipher_spec dropped
            s.kind = ReadStep::Kind::again;
            return s;
        }

        // A message after the handshake: a NewSessionTicket's session to
        // the cache, KeyUpdate taken (§4.6); false when it ended the
        // connection
        bool _after_handshake(const slice<const byte>& m) noexcept {
            const Step& step = _feed_machine(m);
            for (auto& a : step.actions) {
                switch (a.kind) {
                case Action::Kind::new_ticket:
                    _keep_session(step, a);
                    break;
                case Action::Kind::update_read:
                    _b->read.update();
                    if (auto k = _b->assembler.on_key_change(); !k) {
                        _read_failed(k.error());
                        return false;
                    }
                    break;
                case Action::Kind::send:
                    _want_key_update.store(true, std::memory_order_release);   // KeyUpdate(update_not_requested), then the write keys updated
                    break;
                case Action::Kind::alert:
                    _read_failed(Alert{a.alert, 0, a.what});
                    return false;
                default:
                    break;
                }
            }
            return true;
        }

        // A ticket's session into the cache: the ticket copied, the PSK (a
        // 1.2 session's master secret) into an unmanaged block of its own
        void _keep_session(const Step& step, const Action& a) noexcept {
            if (!_cache || !_hs) {
                return;
            }
            tracked_ptr<Session> s = make_tracked<Session>();
            s->version = a.tls12 ? Tls12 : Tls13;
            s->cipher = uint16_t(a.cipher);
            sgcl::detail::copy_bytes(s->session_id, a.session_id, a.session_id_size);
            s->session_id_size = a.session_id_size;
            s->group = a.group;
            auto ticket = step.bytes(a);
            s->ticket.assign(ticket.data(), ticket.data() + ticket.size());
            s->psk = std::make_unique<Secret>();
            std::memcpy(s->psk->bytes, a.secret.bytes, sizeof a.secret.bytes);
            s->psk->size = a.secret.size;
            s->received_ms = a.issued_ms;
            s->lifetime = a.lifetime;
            s->age_add = a.age_add;
            s->peer_certificates = _hs->result().peer_certificates;
            _cache->put(_cache_key, s);
        }

        // A failure of the read side, with the alert it sends: the error
        // first, then the alert stored (the write side that takes it reads
        // the error)
        void _read_failed(const Alert& a) noexcept {
            if (!_read_broken) {
                _read_broken = local_error(a.description, "read", describe());
                _want_alert.store(uint8_t(a.description), std::memory_order_release);
            }
        }

        // --- writing, without I/O (under the record mutex) --------------------------
        //
        // Every record sealed goes to the queue of the connection's output
        // (records in their order, ciphertext) and leaves it as the transport
        // takes it: whole in a write that waits, in part in one that tries,
        // the tail then kept for whichever write comes next.

        SGCL_INLINE_HOT net::detail::ConnImpl& _t() const noexcept {
            return net::detail::ConnectionAccess::impl(_transport);
        }

        SGCL_INLINE_HOT bool _wants() const noexcept {
            return _want_key_update.load(std::memory_order_acquire) || _want_alert.load(std::memory_order_acquire) != NoAlert;
        }

        SGCL_INLINE_HOT optional<io::error> _writable() noexcept {
            if (_write_broken) {
                return _write_broken;
            }
            if (_read_fatal.load(std::memory_order_acquire)) {   // the peer's fatal alert read: _read_broken written before
                _write_broken = _read_broken;
                return _write_broken;
            }
            if (_write_closed) {
                return net::detail::closed_error("write", describe());
            }
            return nullopt;
        }

        SGCL_INLINE_HOT size_t _unsent_size() const noexcept {
            return _b->queue.size() - _b->queue_sent;
        }

        SGCL_INLINE_HOT slice<const byte> _unsent() const noexcept {
            return bytes_of(_b->queue.data() + _b->queue_sent, _unsent_size());
        }

        SGCL_INLINE_HOT void _sent(size_t n) noexcept {
            _b->queue_sent += n;
            if (_b->queue_sent == _b->queue.size()) {
                _b->queue.clear();
                _b->queue_sent = 0;
            }
        }

        // At a write's end: a large write's room not kept for the
        // connection's life (the queue holds a small response's records)
        SGCL_INLINE_HOT void _trim() noexcept {
            if (!_unsent_size() && _b->queue.capacity() > RecordBlocks::Small) {
                // the room of a large write back to the thread, for the
                // next large write of any of its connections (a malloc
                // and a free a large response before), none kept by this one
                QueueBytes room;
                room.swap(_b->queue);
                auto& spare = _spare();
                if (room.capacity() <= SpareLimit && room.capacity() > spare.capacity()) {
                    room.clear();
                    spare.swap(room);
                }
            }
        }

        // Room for `more` bytes past what the queue holds, and for `want`
        // when it is empty (a batch of records: taken at once, not grown
        // into by doubling): the thread's spare room when it has enough
        SGCL_INLINE_HOT void _room(size_t more, size_t want = 0) noexcept {
            auto& q = _b->queue;
            if (q.empty() && q.capacity() < more) {
                want = std::max(want, more);
                auto& spare = _spare();
                if (spare.capacity() >= want) {
                    q.swap(spare);
                } else {
                    q.reserve(want);
                }
            }
        }

        static constexpr size_t SpareLimit = size_t(128) << 10;
        static constexpr size_t BatchBytes = size_t(48) << 10;   // the records of a write of pieces sealed before one write to the transport

        static QueueBytes& _spare() noexcept {
            thread_local QueueBytes spare;
            return spare;
        }

        SGCL_INLINE_HOT void _append(ContentType type, const uint8_t* p, size_t n) noexcept {
            _room(_b->write.sealed_size(type, n));
            auto& q = _b->queue;
            size_t at = q.size();
            q.resize(at + _b->write.sealed_size(type, n));
            _b->write.seal(type, bytes_of(p, n), q.data() + at);
        }

        SGCL_INLINE_HOT void _append_key_update() noexcept {
            const uint8_t ku[5] = {uint8_t(HandshakeType::key_update), 0, 0, 1, 0};
            _append(ContentType::handshake, ku, 5);
            _b->write.update();
        }

        // What the read side asked for: the KeyUpdate answer, the fatal
        // alert. True when it was the alert: the read side has failed, its
        // error (written before the alert was stored) now the writes' too,
        // and nothing may follow the alert
        bool _queue_wants() noexcept {
            const bool key_update = _want_key_update.exchange(false, std::memory_order_acq_rel);
            const uint8_t alert = _want_alert.exchange(NoAlert, std::memory_order_acq_rel);
            if (alert != NoAlert && !_write_broken) {
                _write_broken = _read_broken;
            }
            if (_write_closed) {
                return alert != NoAlert;
            }
            if (!_b->ticket.empty()) {
                for (size_t at = 0; at < _b->ticket.size(); at += MaxPlaintext) {
                    _append(ContentType::handshake, _b->ticket.data() + at, std::min(MaxPlaintext, _b->ticket.size() - at));
                }
                std::vector<uint8_t>().swap(_b->ticket);
            }
            if (key_update) {
                _append_key_update();
            }
            if (alert != NoAlert) {
                const uint8_t bytes[2] = {uint8_t(Alert{AlertDescription(alert)}.fatal() ? 2 : 1), alert};
                _append(ContentType::alert, bytes, 2);
            }
            return alert != NoAlert;
        }

        SGCL_INLINE_HOT size_t _record_limit() const noexcept {
            uint16_t l = _server ? 0 : _hs->result().record_size_limit;   // a server answers no record_size_limit in v1
            return l ? std::min(MaxPlaintext, size_t(l) - 1) : MaxPlaintext;
        }

        // One record of data from `from` (a KeyUpdate first when the keys
        // near their limit, §5.5): the plaintext taken
        SGCL_INLINE_HOT size_t _seal_record(const slice<const byte>& data, size_t from) noexcept {
            if (_b->write.needs_update()) {
                _append_key_update();
            }
            size_t n = std::min(data.size() - from, _record_limit());
            _append(ContentType::application_data, reinterpret_cast<const uint8_t*>(data.data()) + from, n);
            return n;
        }

        // One record of the pieces from byte `from` of them all: its
        // plaintext copied into its place in the queue, sealed there (a
        // KeyUpdate first when the keys near their limit, §5.5); the
        // plaintext taken
        size_t _seal_parts(const slice<const byte>* parts, size_t n, size_t total, size_t from) noexcept {
            if (_b->write.needs_update()) {
                _append_key_update();
            }
            const size_t k = std::min(total - from, _record_limit());
            const size_t record = _b->write.sealed_size(ContentType::application_data, k);
            _room(record, total - from > k ? BatchBytes + MaxRecord : 0);
            auto& q = _b->queue;
            const size_t at = q.size();
            q.resize(at + _b->write.sealed_size(ContentType::application_data, k));
            uint8_t* body = q.data() + at + HeaderSize;
            size_t skip = from;
            size_t w = 0;
            for (size_t i = 0; i < n && w < k; ++i) {
                const size_t size = parts[i].size();
                if (skip >= size) {
                    skip -= size;
                    continue;
                }
                const size_t take = std::min(size - skip, k - w);
                sgcl::detail::copy_bytes(body + w, parts[i].data() + skip, take);
                w += take;
                skip = 0;
            }
            _b->write.seal(ContentType::application_data, bytes_of(body, k), q.data() + at);
            return k;
        }

        // The plaintext of a write of pieces a write that tried has sealed
        // already: its record starts at `from` of the same pieces
        SGCL_INLINE_HOT size_t _skip_sealed_parts(const byte* first, size_t from) noexcept {
            size_t n = _sealed_size && first == _sealed_at && from == _sealed_from ? _sealed_size : 0;
            _sealed_at = nullptr;
            _sealed_size = 0;
            _sealed_from = 0;
            return n;
        }

        // The plaintext of this data a write that tried has sealed already
        SGCL_INLINE_HOT size_t _skip_sealed(const slice<const byte>& data) noexcept {
            size_t n = _sealed_size && data.data() == _sealed_at && data.size() >= _sealed_size ? _sealed_size : 0;
            _sealed_at = nullptr;
            _sealed_size = 0;
            return n;
        }

        // None after a failure of either side: the writes' own, the peer's
        // fatal alert, the read side's alert (taken, or still to be taken)
        SGCL_INLINE_HOT bool _queue_close_notify() noexcept {
            if (_write_closed || _write_broken || _read_fatal.load(std::memory_order_acquire) || _want_alert.load(std::memory_order_acquire) != NoAlert ||
                !_b->write.installed()) {
                return false;
            }
            const uint8_t bytes[2] = {1, 0};
            _append(ContentType::alert, bytes, 2);
            _write_closed = true;
            return true;
        }

        optional<io::error> _flush_block() {
            while (_unsent_size()) {
                auto w = _t().raw_write(_unsent());
                if (!w) {
                    return _break(w.error());
                }
                _sent(*w);
            }
            return nullopt;
        }

        // What the transport takes now; true when nothing is left
        SGCL_INLINE_HOT expected<bool, io::error> _flush_try() {
            if (!_unsent_size()) {
                return true;
            }
            auto w = _t().try_raw_write(_unsent());
            if (!w) {
                return unexpected(_break(w.error()));
            }
            _sent(*w);
            return _unsent_size() == 0;
        }

        // What the transport takes now, a failure not recorded (close());
        // true when nothing is left
        bool _flush_quiet() {
            while (_unsent_size()) {
                auto w = _t().try_raw_write(_unsent());
                if (!w || *w == 0) {
                    return false;
                }
                _sent(*w);
            }
            return true;
        }

        // The read side's wants written when the record is free; a writer
        // that held it checks again after letting go, so none is left behind
        void _drain_try() {
            while (_wants() && _record.try_lock()) {
                (void)_queue_wants();
                (void)_flush_try();
                _record.unlock();
            }
        }

        // A failure of the transport's write, the write side's alone (under
        // the record mutex)
        io::error _break(const io::error& e) noexcept {
            if (!_write_broken) {
                _write_broken = e;
            }
            return e;
        }
    };
}
