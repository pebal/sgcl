//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "cipher.h"
#include "kex.h"
#include "keys.h"
#include "wire.h"
#include "zlib.h"
#include "../../connection.h"
#include "../../error.h"
#include "../../../async/channel.h"
#include "../../../async/coroutine.h"
#include "../../../async/mutex.h"
#include "../../../async/select.h"
#include "../../../async/timer.h"
#include "../../../core/clock.h"
#include "../../../core/deque.h"
#include "../../../core/make_tracked.h"
#include "../../../core/string.h"
#include "../../../core/tracked_ptr.h"
#include "../../../core/vector.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

// An SSH connection over a transport (RFC 4253 and RFC 4254): the version
// exchange, the binary packets, the key exchanges, the channels; either
// side, the client's and the server's particular parts in their classes
// below it (client.h, server.h).
//
// How the work is split, so that nothing waits on what waits on it:
//
//   - One reader, the connection's read loop (a task from the first key
//     exchange's end to the connection's), reads a packet, opens it and
//     acts on it without waiting: channel data goes into the channel's
//     buffer, a reply the reader owes (a channel request's answer, a
//     window, a key exchange's messages) is posted to a queue. The read
//     loop never takes the write side's lock, so a peer that writes while
//     it reads is always read.
//   - Writers take the write lock, seal and write: a task's or a thread's
//     own messages (channel data, requests) directly, and the posted ones
//     through a flusher task that the first post starts. The posts of a
//     key exchange go first; while one runs (from our KEXINIT to our
//     NEWKEYS) no other message is sealed (RFC 4253 §7.1), and the writers
//     wait for its end at a gate.
//   - A key exchange is a machine the read loop drives: the peer's KEXINIT
//     (ours posted if it is not out yet), the method's messages, NEWKEYS
//     both ways; the keys of the writing direction are installed by the
//     writer that seals our NEWKEYS, those of the reading direction by the
//     read loop when the peer's comes. A new exchange starts after 1 GB
//     either way or an hour (configurable), or when the peer asks.
//
// Every key and every packet buffer is unmanaged (the Transport below,
// held by a unique_ptr), the keys zeroed when replaced and when the
// connection goes.
namespace sgcl::net::ssh::detail {
    using sgcl::io::detail::fail;

    class SshConn;
    class ChannelImpl;

    inline constexpr std::string_view SoftwareVersion = "SSH-2.0-SGCL_1.0";

    // The module's own errors, op "ssh"
    inline io::error ssh_error(net::errc code, std::string_view text) noexcept {
        return io::error(net::make_error_code(code), "ssh", string(text));
    }

    // What a connection is made with (the options of the client and the
    // server, copied)
    struct ConnSettings {
        Preferences prefs = default_preferences();
        uint64_t rekey_bytes = uint64_t(1) << 30;
        duration rekey_interval = 3600 * second;
        duration keepalive_interval = duration::zero();
        int keepalive_count_max = 3;
        uint32_t window = 2 * 1024 * 1024;   // a channel's window, as OpenSSH's
        uint32_t max_packet = 32768;         // a channel's largest data packet
        uint32_t max_channels = 64;
    };

    enum class VersionScan : uint8_t {
        more,     // more bytes needed
        done,     // the peer's version line read
        failed,   // why says why
    };

    // The peer's version line out of the bytes come so far (RFC 4253 §4.2):
    // lines before it passed over on the client's side (at most 1024 of
    // them, `lines` counting across calls), none on the server's; a line at
    // most 255 bytes, CR LF or LF at its end, "SSH-2.0-" or "SSH-1.99-" in
    // front, printable ASCII. `used` is what was taken of the bytes
    inline VersionScan scan_version(const uint8_t* p, size_t n, bool client, size_t& lines, size_t& used, std::string& version, const char*& why) {
        used = 0;
        for (;;) {
            size_t nl = SIZE_MAX;
            for (size_t i = used; i < n; ++i) {
                if (p[i] == '\n') {
                    nl = i;
                    break;
                }
            }
            if (nl == SIZE_MAX) {
                if (n - used > 255) {
                    why = "a version line longer than 255 bytes";
                    return VersionScan::failed;
                }
                return VersionScan::more;
            }
            std::string_view v(reinterpret_cast<const char*>(p + used), nl - used);
            used = nl + 1;
            if (v.size() > 255) {
                why = "a version line longer than 255 bytes";
                return VersionScan::failed;
            }
            if (!v.empty() && v.back() == '\r') {
                v.remove_suffix(1);
            }
            if (v.substr(0, 4) == "SSH-") {
                if (v.substr(0, 8) != "SSH-2.0-" && v.substr(0, 9) != "SSH-1.99-") {
                    why = "protocol version not supported";
                    return VersionScan::failed;
                }
                for (char c : v) {
                    if (uint8_t(c) < 0x20 || uint8_t(c) > 0x7E) {
                        why = "a version line with control characters";
                        return VersionScan::failed;
                    }
                }
                version.assign(v);
                return VersionScan::done;
            }
            if (!client || ++lines > 1024) {
                why = "no SSH version line";
                return VersionScan::failed;
            }
        }
    }

    // The unmanaged state of the packets: the keys, the sequence numbers,
    // the buffers, the compression
    struct Transport {
        std::unique_ptr<PacketKeys> in = std::make_unique<PacketKeys>();
        std::unique_ptr<PacketKeys> out = std::make_unique<PacketKeys>();
        std::unique_ptr<PacketKeys> next_in;    // derived, installed at the peer's NEWKEYS
        std::unique_ptr<PacketKeys> next_out;   // derived, installed after our NEWKEYS
        uint32_t seq_in = 0;
        uint32_t seq_out = 0;
        uint64_t packets_in = 0;                // since the connection began
        Bytes rbuf;
        size_t rpos = 0;
        size_t rend = 0;
        Bytes wbuf;
        Bytes zbuf;                             // a payload compressed
        Bytes plain;                            // a payload decompressed
        std::atomic<uint64_t> in_bytes{0};      // since the last key exchange (read by the writers' rekey check)
        uint64_t out_bytes = 0;
        std::unique_ptr<ZlibIn> zin;            // made when compression starts
        std::unique_ptr<ZlibOut> zout;
    };

    // One posted message: a payload and what is done after it is sealed
    struct OutItem {
        Bytes payload;
        uint8_t action = 0;   // 1: the new keys of the writing direction installed (after NEWKEYS)
    };

    // A reply to a global request we sent (RFC 4254 §4), in order
    struct GlobalReply {
        async::detail::ChannelState<pair<bool, string>> done{1};
    };

    // A global request of the peer's waiting for its answer, which go out
    // in the order the requests came
    struct GlobalSlot {
        bool ready = false;
        bool ok = false;
        Bytes extra;
    };

    // The kex machine's step
    enum class KexStep : uint8_t {
        idle,
        await_init,      // the server: the client's KEX_INIT
        await_reply,     // the client: the server's KEX_REPLY
        await_newkeys,   // the peer's NEWKEYS
    };

    // A channel (RFC 4254 §5): its ids and windows both ways, the data
    // come in (the standard stream and the extended one, stderr) waiting
    // to be read, its ends, the replies to its requests; a session's exit
    class ChannelImpl {
    public:
        ChannelImpl(const tracked_ptr<SshConn>& conn, uint32_t local_id, std::string_view type, uint32_t window, uint32_t max_packet) noexcept
        : conn(conn)
        , local_id(local_id)
        , type(type)
        , local_window_size(window)
        , local_window(window)
        , local_max_packet(max_packet)
        , _rearm(make_tracked<async::detail::ChannelState<void>>()) {
        }

        virtual ~ChannelImpl() = default;

        tracked_ptr<SshConn> conn;
        const uint32_t local_id;
        uint32_t remote_id = 0;
        std::string type;

        // what the peer allows us to send
        uint64_t remote_window = 0;
        uint32_t remote_max_packet = 0;
        // what we allow the peer to send
        const uint32_t local_window_size;
        uint32_t local_window;
        uint32_t consumed = 0;   // read by the program since the last adjust
        const uint32_t local_max_packet;

        std::mutex m;
        Bytes in[2];
        size_t in_pos[2] = {0, 0};
        bool eof_in = false;
        bool close_in = false;
        bool eof_out = false;
        bool close_out = false;
        bool conn_gone = false;   // the connection ended
        bool discard = false;     // data that comes is dropped and its window given back (a session waited for)
        bool closing = false;     // our CLOSE is on its way
        bool opened = false;      // the open confirmed
        optional<io::error> open_error;
        time_point deadlines[2] = {};   // read, write

        // a session's end (RFC 4254 §6.10)
        optional<uint32_t> exit_status;
        std::string exit_signal;
        bool core_dumped = false;
        std::string exit_message;

        // our requests waiting for their replies, in order
        vector<tracked_ptr<async::detail::ChannelState<bool>>> pending;

        async::mutex write_lock;   // one write at a time, whole

        // A request of the peer's on this channel, answered (or not) here;
        // the default takes a session's exit and refuses the rest
        virtual void on_request(std::string_view name, bool want_reply, Reader& r);

        // The peer's CLOSE came (the read loop, under no lock)
        virtual void on_peer_close() {
        }

        // The data of the peer's came in (the read loop, under no lock):
        // false when it is past the window
        bool deliver(int stream, const uint8_t* p, size_t n);

        bool _deliver_locked(int stream, const uint8_t* p, size_t n, bool& dropped);

        // The waits on this channel woken to look at its state again
        void wake() {
            tracked_ptr<async::detail::ChannelState<void>> old;
            {
                std::lock_guard<std::mutex> g(m);
                old = _rearm;
                _rearm = make_tracked<async::detail::ChannelState<void>>();
            }
            old->close();
        }

        SGCL_INLINE_HOT tracked_ptr<async::detail::ChannelState<void>> rearm_locked() const noexcept {
            return _rearm;
        }

        string describe() const;

    private:
        tracked_ptr<async::detail::ChannelState<void>> _rearm;
    };

    // The connection
    class SshConn {
    public:
        SshConn(const net::connection& transport, bool client, const ConnSettings& settings) noexcept
        : transport(transport)
        , is_client(client)
        , settings(settings)
        , t(std::make_unique<Transport>())
        , auth_inbox(64)
        , ended(make_tracked<async::detail::ChannelState<void>>())
        , _gate(make_tracked<async::detail::ChannelState<void>>()) {
            _gate->close();   // open: no exchange runs
        }

        virtual ~SshConn() = default;

        net::connection transport;
        const bool is_client;
        ConnSettings settings;
        std::unique_ptr<Transport> t;
        std::string local_version{SoftwareVersion};
        std::string peer_version;
        Bytes session_id;
        Bytes host_key_blob;            // the server's, from the first exchange
        std::string host_key_alg;
        std::vector<std::string> server_sig_algs;   // the client's: the server's EXT_INFO
        bool strict = false;            // strict KEX, settled by the first exchange
        std::atomic<bool> authenticated{false};
        std::atomic<bool> failed{false};
        std::atomic<bool> disconnecting{false};   // a DISCONNECT on its way: the end follows it
        optional<io::error> error;      // why the connection ended (under _q)
        async::detail::ChannelState<string> auth_inbox;   // the authentication's messages, during it
        tracked_ptr<async::detail::ChannelState<void>> ended;   // closed when the connection ends
        async::mutex write_lock;
        std::atomic<bool> zlib_pending_in{false};    // compression starts with the next packet read
        std::atomic<int64_t> last_read_ns{0};        // the clock's nanoseconds at the last packet read

        // --- what the client and the server do --------------------------------

        // The client: the server's host key and its signature over H,
        // checked (the signature, then known_hosts or the callback)
        virtual optional<io::error> check_host_key(const Bytes& blob, const AlgInfo& alg, const Bytes& h, const Span& signature) {
            (void)blob;
            (void)alg;
            (void)h;
            (void)signature;
            return ssh_error(net::errc::ssh_handshake, "no host key check");
        }

        // The server: the blob of its host key of an algorithm (a
        // certificate's for a certificate's algorithm); false when it has
        // none
        virtual bool host_key_of(const AlgInfo& alg, Bytes& blob) {
            (void)alg;
            (void)blob;
            return false;
        }

        // The server: the signature blob of H by that key
        virtual bool sign_host(const AlgInfo& alg, const Bytes& h, Bytes& signature) {
            (void)alg;
            (void)h;
            (void)signature;
            return false;
        }

        // A channel the peer opens; the default refuses it
        virtual void on_channel_open(std::string_view type, uint32_t sender, uint32_t window, uint32_t max_packet, Reader& r) {
            (void)type;
            (void)window;
            (void)max_packet;
            (void)r;
            post_open_failure(sender, OpenUnknownChannelType, "unknown channel type");
        }

        // A global request of the peer's (keepalive@openssh.com and the
        // rest refused); slot is the answer's place in order, or null
        virtual void on_global_request(std::string_view name, Reader& r, const tracked_ptr<GlobalSlot>& slot) {
            (void)name;
            (void)r;
            if (slot) {
                answer_global(slot, false, Bytes());
            }
        }

        // The connection ended (the read loop's end): the client's
        // listeners and the server's sessions told
        virtual void on_closed() {
        }

        std::string describe() const {
            return transport ? std::string(transport.remote_endpoint().to_string().view()) : std::string();
        }

        // --- the version exchange and the first key exchange --------------------

        // Our line sent, the peer's read (lines before it passed over on the
        // client's side, RFC 4253 §4.2): within the transport's deadline
        static async::task<expected<void, io::error>> co_version(tracked_ptr<SshConn> self) noexcept {
            std::string line = self->local_version + "\r\n";
            auto w = co_await self->transport.async_write(string(line));
            if (!w) {
                co_return fail(w);
            }
            Transport& t = *self->t;
            size_t lines = 0;
            for (;;) {
                size_t used = 0;
                const char* why = nullptr;
                auto r = scan_version(t.rbuf.data() + t.rpos, t.rend - t.rpos, self->is_client, lines, used, self->peer_version, why);
                t.rpos += used;
                if (r == VersionScan::done) {
                    co_return expected<void, io::error>();
                }
                if (r == VersionScan::failed) {
                    co_return fail(ssh_error(net::errc::ssh_handshake, self->describe() + ": " + why));
                }
                auto f = co_await self->co_fill(t.rend - t.rpos + 1);
                if (!f) {
                    co_return fail(f);
                }
            }
        }

        // The first key exchange: our KEXINIT, the peer's, the method, the
        // NEWKEYS both ways, read here (the read loop has not started). With
        // strict KEX (both sides offer it) the peer's KEXINIT must be its
        // first packet and nothing but the exchange's messages may come
        // before its NEWKEYS; the sequence numbers start again after
        static async::task<expected<void, io::error>> co_first_kex(tracked_ptr<SshConn> self) noexcept {
            self->_post_kexinit(true);
            for (;;) {
                auto p = co_await self->co_read_packet();
                if (!p) {
                    co_return fail(p);
                }
                const uint8_t* data = p->p;
                const size_t n = p->n;
                if (n == 0) {
                    co_return fail(self->protocol_error("an empty packet"));
                }
                const uint8_t msg = data[0];
                if (msg == MsgDisconnect) {
                    co_return fail(self->disconnected(data, n));
                }
                if (msg == MsgKexinit || msg == MsgNewkeys || msg == MsgKexInit || msg == MsgKexReply) {
                    if (msg == MsgKexinit && self->_peer_kexinit_count == 0 && self->t->packets_in != 1 && self->_strict_offered(data, n)) {
                        co_return fail(ssh_error(net::errc::ssh_handshake, self->describe() + ": strict KEX: the KEXINIT is not the first packet"));
                    }
                    if (auto e = self->_kex_message(data, n)) {
                        co_return fail(*e);
                    }
                    if (msg == MsgNewkeys) {
                        break;
                    }
                    continue;
                }
                if (self->_strict_known && self->strict) {
                    co_return fail(ssh_error(net::errc::ssh_handshake, self->describe() + ": strict KEX: a message during the first key exchange"));
                }
                if (msg == MsgIgnore || msg == MsgDebug || msg == MsgUnimplemented) {
                    continue;
                }
                co_return fail(self->protocol_error("an unexpected message during the key exchange"));
            }
            // our NEWKEYS sealed and its keys installed: the gate opens
            tracked_ptr<async::detail::ChannelState<void>> gate;
            {
                std::lock_guard<std::mutex> g(self->_q);
                gate = self->_gate;
            }
            co_await gate->receive();
            if (self->failed.load()) {
                co_return fail(self->current_error());
            }
            co_return expected<void, io::error>();
        }

        // The read loop, from the first key exchange's end to the
        // connection's
        static void start(const tracked_ptr<SshConn>& self) {
            self->last_read_ns.store(sgcl::clock::now().time_since_epoch().count(), std::memory_order_relaxed);
            async::go(_read_loop(self));
            if (self->settings.keepalive_interval > duration::zero()) {
                async::go(_keepalive(self));
            }
        }

        // --- reading packets ----------------------------------------------------

        // At least n unread bytes in the read buffer
        async::task<expected<void, io::error>> co_fill(size_t n) noexcept {
            Transport& tr = *t;
            while (tr.rend - tr.rpos < n) {
                if (tr.rbuf.size() - tr.rpos < n || tr.rend == tr.rbuf.size()) {
                    // the unread bytes to the front, room for n and more
                    size_t have = tr.rend - tr.rpos;
                    if (tr.rpos) {
                        sgcl::detail::move_bytes(tr.rbuf.data(), tr.rbuf.data() + tr.rpos, have);
                    }
                    tr.rpos = 0;
                    tr.rend = have;
                    size_t want = std::max<size_t>(n, 64 * 1024);
                    if (tr.rbuf.size() < want) {
                        tr.rbuf.resize(want);
                    }
                }
                // the transport tried without a frame (ConnImpl::try_read) and
                // its readiness awaited in this frame; the waiting read when
                // it cannot be tried
                const auto room = mutable_bytes_of(tr.rbuf.data() + tr.rend, tr.rbuf.size() - tr.rend);
                auto& impl = net::detail::ConnectionAccess::impl(transport);
                size_t got = 0;
                for (;;) {
                    bool slow = false;
                    auto tried = impl.try_read(room, slow);
                    if (slow) {
                        auto r = co_await transport.async_read(room);
                        if (!r) {
                            co_return fail(r);
                        }
                        got = *r;
                        break;
                    }
                    if (!tried) {
                        co_return fail(tried);
                    }
                    if (*tried) {
                        got = **tried;
                        break;
                    }
                    if (auto ready = co_await impl.raw_readable(); !ready) {
                        co_return fail(ready);
                    }
                }
                if (got == 0) {
                    co_return fail(ssh_error(net::errc::ssh_disconnected, describe() + ": the connection ended"));
                }
                tr.rend += got;
            }
            co_return expected<void, io::error>();
        }

        // The next packet's payload, a view into the transport's buffers
        // valid until the next read
        async::task<expected<Span, io::error>> co_read_packet() noexcept {
            Transport& tr = *t;
            const size_t head = tr.in->head_size();
            auto h = co_await co_fill(head);
            if (!h) {
                co_return fail(h);
            }
            uint32_t length;
            size_t total;
            if (tr.in->length(tr.seq_in, tr.rbuf.data() + tr.rpos, length, total) != OpenError::none) {
                co_return fail(protocol_error("a packet length out of range"));
            }
            auto f = co_await co_fill(total);
            if (!f) {
                co_return fail(f);
            }
            size_t at, size;
            OpenError e = tr.in->open(tr.seq_in, tr.rbuf.data() + tr.rpos, length, at, size);
            if (e != OpenError::none) {
                co_return fail(e == OpenError::mac ? ssh_error(net::errc::ssh_protocol, describe() + ": a packet's MAC does not match")
                                                   : protocol_error("a packet's padding out of range"));
            }
            const uint8_t* payload = tr.rbuf.data() + tr.rpos + at;
            tr.rpos += total;
            tr.in_bytes.fetch_add(total, std::memory_order_relaxed);
            ++tr.packets_in;
            if (++tr.seq_in == 0 && strict && session_id.empty()) {
                co_return fail(ssh_error(net::errc::ssh_handshake, describe() + ": strict KEX: the sequence number wrapped in the first exchange"));
            }
            if (zlib_pending_in.exchange(false, std::memory_order_acq_rel)) {
                tr.zin = std::make_unique<ZlibIn>();
            }
            if (tr.zin) {
                if (!tr.zin->decompress(payload, size, tr.plain, MaxPacketLength)) {
                    co_return fail(ssh_error(net::errc::ssh_protocol, describe() + ": a compressed payload that does not decompress"));
                }
                co_return Span{tr.plain.data(), tr.plain.size()};
            }
            co_return Span{payload, size};
        }

        // --- writing ------------------------------------------------------------

        // A message of ours: sealed and written in its turn (after the posted
        // ones), after the key exchange in progress if one is
        static async::task<expected<void, io::error>> co_send(tracked_ptr<SshConn> self, Bytes payload) noexcept {
            co_return co_await co_send_parts(std::move(self), std::move(payload), nullptr, 0);
        }

        // The same with data after the message's fields, sealed from where
        // it lies (a channel's data: its slice is held by the caller's frame
        // until the write is done)
        static async::task<expected<void, io::error>> co_send_parts(tracked_ptr<SshConn> self, Bytes head, const uint8_t* body, size_t body_size) noexcept {
            for (;;) {
                optional<async::mutex::guard> g;
                g.emplace(co_await self->write_lock.scoped_lock());
                if (self->failed.load(std::memory_order_acquire)) {
                    co_return fail(self->current_error());
                }
                tracked_ptr<async::detail::ChannelState<void>> gate;
                {
                    std::lock_guard<std::mutex> q(self->_q);
                    self->_seal_posted_locked(true);
                    if (self->_kex_out) {
                        gate = self->_gate;
                    } else {
                        self->_seal_posted_locked(false);
                        self->_seal(head.data(), head.size(), body, body_size);
                        self->_maybe_rekey_locked();
                        self->_seal_posted_locked(true);
                    }
                }
                auto w = co_await self->_write_out();
                if (!w) {
                    co_return fail(w);
                }
                if (!gate) {
                    co_return expected<void, io::error>();
                }
                g.reset();   // let go, wait for the exchange's end
                co_await gate->receive();
            }
        }

        // A message the read loop owes, posted: sealed by the flusher (or by
        // the next writer) in order, the key exchange's own first
        void post(Bytes payload, bool kex = false, uint8_t action = 0) {
            bool start = false;
            {
                std::lock_guard<std::mutex> g(_q);
                (kex ? _kex_q : _norm_q).push_back(OutItem{std::move(payload), action});
                if (!_flushing) {
                    _flushing = true;
                    start = true;
                }
            }
            if (start) {
                // written here when no write is in progress and the transport
                // takes the bytes at once (no task, no wake of a worker);
                // else by a flusher task
                if (write_lock.try_lock()) {
                    _flush_now(async::mutex::guard(write_lock));
                } else {
                    async::go(_flush(tracked_ptr<SshConn>(this)));
                }
            }
        }

        void post_open_failure(uint32_t recipient, uint32_t reason, std::string_view text) {
            Bytes b;
            Writer w(b);
            w.u8(MsgChannelOpenFailure).u32(recipient).u32(reason).string(text).string("");
            post(std::move(b));
        }

        // A global request of ours with want_reply: its answer (success and
        // the extra data, or failure)
        static async::task<expected<pair<bool, string>, io::error>> co_global_request(tracked_ptr<SshConn> self, Bytes payload) noexcept {
            tracked_ptr reply = make_tracked<GlobalReply>();
            bool gone = false;
            {
                std::lock_guard<std::mutex> g(self->_q);
                gone = self->failed.load();
                if (!gone) {
                    self->_global_replies.push_back(reply);
                }
            }
            if (gone) {
                co_return fail(self->current_error());   // outside _q, which current_error takes
            }
            auto s = co_await co_send(self, std::move(payload));
            if (!s) {
                co_return fail(s);
            }
            auto r = co_await reply->done.receive();
            if (!r) {
                co_return fail(self->current_error());
            }
            co_return std::move(*r);
        }

        // The answer of a peer's global request, sent when those before it
        // are
        void answer_global(const tracked_ptr<GlobalSlot>& slot, bool ok, Bytes extra) {
            std::lock_guard<std::mutex> g(_slots_m);
            slot->ready = true;
            slot->ok = ok;
            slot->extra = std::move(extra);
            while (!_slots.empty() && _slots.front()->ready) {
                tracked_ptr<GlobalSlot> s = _slots.front();
                _slots.pop_front();
                Bytes b;
                Writer w(b);
                w.u8(s->ok ? MsgRequestSuccess : MsgRequestFailure);
                w.raw(s->extra.data(), s->extra.size());
                post(std::move(b));
            }
        }

        // --- channels -----------------------------------------------------------

        // A local id for a new channel, its object made by make(id), kept in
        // the table; nullptr past max_channels
        template<class Make>
        tracked_ptr<ChannelImpl> add_channel(Make make) {
            std::lock_guard<std::mutex> g(_ch_m);
            if (failed.load()) {
                return tracked_ptr<ChannelImpl>();
            }
            size_t live = 0;
            for (auto& c : _channels) {
                live += c ? 1 : 0;
            }
            if (live >= settings.max_channels) {
                return tracked_ptr<ChannelImpl>();
            }
            size_t id = 0;
            while (id < _channels.size() && _channels[id]) {
                ++id;
            }
            if (id == _channels.size()) {
                _channels.push_back(tracked_ptr<ChannelImpl>());
            }
            tracked_ptr<ChannelImpl> c = make(uint32_t(id));
            _channels[id] = c;
            return c;
        }

        tracked_ptr<ChannelImpl> channel(uint32_t id) {
            std::lock_guard<std::mutex> g(_ch_m);
            return id < _channels.size() ? _channels[id] : tracked_ptr<ChannelImpl>();
        }

        void remove_channel(uint32_t id) {
            std::lock_guard<std::mutex> g(_ch_m);
            if (id < _channels.size()) {
                _channels[id] = tracked_ptr<ChannelImpl>();
            }
        }

        size_t channel_count() {
            std::lock_guard<std::mutex> g(_ch_m);
            size_t live = 0;
            for (auto& c : _channels) {
                live += c ? 1 : 0;
            }
            return live;
        }

        // A channel of ours opened (RFC 4254 §5.1): the open sent, the
        // confirmation (or failure) awaited
        static async::task<expected<void, io::error>> co_open_channel(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c, Bytes extra) noexcept {
            Bytes b;
            Writer w(b);
            w.u8(MsgChannelOpen).string(c->type).u32(c->local_id).u32(c->local_window_size).u32(c->local_max_packet);
            w.raw(extra.data(), extra.size());
            auto s = co_await co_send(self, std::move(b));
            if (!s) {
                self->remove_channel(c->local_id);
                co_return fail(s);
            }
            for (;;) {
                tracked_ptr<async::detail::ChannelState<void>> r;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    if (c->opened) {
                        co_return expected<void, io::error>();
                    }
                    if (c->open_error) {
                        auto e = *c->open_error;
                        co_return fail(e);
                    }
                    if (c->conn_gone) {
                        co_return fail(self->current_error());
                    }
                    r = c->rearm_locked();
                }
                co_await r->receive();
            }
        }

        // A request on a channel (RFC 4254 §5.4): with want_reply, its
        // answer awaited (ssh_request_refused for a failure)
        static async::task<expected<void, io::error>> co_channel_request(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c, std::string name, bool want_reply,
                                                                         Bytes extra) noexcept {
            tracked_ptr<async::detail::ChannelState<bool>> reply;
            Bytes b;
            optional<async::mutex::guard> lock;
            lock.emplace(co_await c->write_lock.scoped_lock());   // never after our CLOSE, which goes out under it
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->conn_gone) {
                    co_return fail(self->current_error());
                }
                if (c->close_out || c->close_in || c->closing) {
                    co_return fail(io::error(io::errc::closed, string(name), c->describe()));
                }
                Writer w(b);
                w.u8(MsgChannelRequest).u32(c->remote_id).string(name).boolean(want_reply);
                w.raw(extra.data(), extra.size());
                if (want_reply) {
                    reply = make_tracked<async::detail::ChannelState<bool>>(1);
                    c->pending.push_back(reply);
                }
            }
            auto s = co_await co_send(self, std::move(b));
            lock.reset();
            if (!s) {
                co_return fail(s);
            }
            if (!want_reply) {
                co_return expected<void, io::error>();
            }
            auto r = co_await reply->receive();
            if (!r) {
                co_return fail(c->conn_gone ? self->current_error() : ssh_error(net::errc::ssh_request_refused, self->describe() + ": " + name + " (the channel closed)"));
            }
            if (!*r) {
                co_return fail(ssh_error(net::errc::ssh_request_refused, self->describe() + ": " + name));
            }
            co_return expected<void, io::error>();
        }

        // A request with want_reply and our EOF behind it, sent together, then
        // the request's answer awaited: what a command run in one line does,
        // a round trip the fewer than the request, its answer and the EOF in
        // turn
        static async::task<expected<void, io::error>> co_request_then_eof(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c, std::string name, Bytes extra) noexcept {
            auto reply = co_await co_start_request_then_eof(self, c, name, std::move(extra));
            if (!reply) {
                co_return fail(reply);
            }
            auto r = co_await (*reply)->receive();
            if (!r) {
                co_return fail(c->conn_gone ? self->current_error() : ssh_error(net::errc::ssh_request_refused, self->describe() + ": " + name + " (the channel closed)"));
            }
            if (!*r) {
                co_return fail(ssh_error(net::errc::ssh_request_refused, self->describe() + ": " + name));
            }
            co_return expected<void, io::error>();
        }

        // The same sent, its answer left to the caller: the channel the
        // answer comes on (its arrival wakes the channel's waits too)
        static async::task<expected<tracked_ptr<async::detail::ChannelState<bool>>, io::error>> co_start_request_then_eof(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c,
                                                                                                                          std::string name, Bytes extra) noexcept {
            tracked_ptr<async::detail::ChannelState<bool>> reply = make_tracked<async::detail::ChannelState<bool>>(1);
            {
                auto lock = co_await c->write_lock.scoped_lock();
                std::lock_guard<std::mutex> g(c->m);
                if (c->conn_gone) {
                    co_return fail(self->current_error());
                }
                if (c->close_out || c->close_in || c->closing || c->eof_out) {
                    co_return fail(io::error(io::errc::closed, string(name), c->describe()));
                }
                Bytes b;
                Writer w(b);
                w.u8(MsgChannelRequest).u32(c->remote_id).string(name).boolean(true);
                w.raw(extra.data(), extra.size());
                c->pending.push_back(reply);
                self->post(std::move(b));
                c->eof_out = true;
                Bytes e;
                Writer we(e);
                we.u8(MsgChannelEof).u32(c->remote_id);
                self->post(std::move(e));
            }
            co_return reply;
        }

        // Up to buffer.size() bytes of a channel's stream (0: its end), the
        // read deadline holding; the window given back once half of it is read
        static async::task<expected<size_t, io::error>> co_channel_read(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c, int stream, slice<byte> buffer) noexcept {
            if (buffer.empty()) {
                co_return size_t(0);
            }
            for (;;) {
                tracked_ptr<async::detail::ChannelState<void>> r;
                time_point deadline;
                size_t n = 0;
                uint32_t adjust = 0;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    Bytes& b = c->in[stream];
                    size_t avail = b.size() - c->in_pos[stream];
                    if (avail) {
                        n = std::min(avail, buffer.size());
                        sgcl::detail::copy_bytes(buffer.data(), b.data() + c->in_pos[stream], n);
                        c->in_pos[stream] += n;
                        c->consumed += uint32_t(n);
                        if (c->consumed >= c->local_window_size / 2 && !c->close_in && !c->close_out && !c->eof_in) {
                            adjust = c->consumed;
                            c->local_window += adjust;
                            c->consumed = 0;
                            // posted under the channel's lock: never after our CLOSE
                            Bytes a;
                            Writer w(a);
                            w.u8(MsgChannelWindowAdjust).u32(c->remote_id).u32(adjust);
                            self->post(std::move(a));
                        }
                    } else if (c->eof_in || c->close_in) {
                        co_return size_t(0);
                    } else if (c->conn_gone) {
                        co_return fail(self->current_error());
                    } else if (c->close_out) {
                        co_return fail(io::error(io::errc::closed, "read", c->describe()));
                    } else {
                        r = c->rearm_locked();
                        deadline = c->deadlines[0];
                    }
                }
                if (n) {
                    (void)adjust;
                    co_return n;
                }
                if (deadline != time_point() && sgcl::clock::now() >= deadline) {
                    co_return fail(net::detail::system_error(ETIMEDOUT, "read", c->describe()));
                }
                if (deadline == time_point()) {
                    co_await r->receive();
                } else {
                    co_await sgcl::async::select(r->on_receive([] {}), sgcl::async::timeout(deadline, [] {}));
                }
            }
        }

        // All of data written to a channel's stream (0 the data, 1 the
        // extended data of stderr) within the peer's window and packet size,
        // the write deadline holding
        static async::task<expected<size_t, io::error>> co_channel_write(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c, int stream, slice<const byte> data) noexcept {
            auto lock = co_await c->write_lock.scoped_lock();
            size_t done = 0;
            while (done < data.size()) {
                tracked_ptr<async::detail::ChannelState<void>> r;
                time_point deadline;
                size_t chunk = 0;
                uint32_t remote = 0;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    if (c->conn_gone) {
                        co_return fail(self->current_error());
                    }
                    if (c->eof_out || c->close_out || c->closing) {
                        co_return fail(io::error(io::errc::closed, "write", c->describe()));
                    }
                    if (c->close_in) {
                        co_return fail(net::detail::system_error(EPIPE, "write", c->describe()));
                    }
                    if (c->remote_window > 0) {
                        chunk = size_t(std::min<uint64_t>({c->remote_window, uint64_t(c->remote_max_packet), uint64_t(data.size() - done)}));
                        c->remote_window -= chunk;
                        remote = c->remote_id;
                    } else {
                        r = c->rearm_locked();
                        deadline = c->deadlines[1];
                    }
                }
                if (chunk) {
                    Bytes head;
                    Writer w(head);
                    if (stream == 0) {
                        w.u8(MsgChannelData).u32(remote).u32(uint32_t(chunk));
                    } else {
                        w.u8(MsgChannelExtendedData).u32(remote).u32(1).u32(uint32_t(chunk));
                    }
                    auto s = co_await co_send_parts(self, std::move(head), reinterpret_cast<const uint8_t*>(data.data()) + done, chunk);
                    if (!s) {
                        co_return fail(s);
                    }
                    done += chunk;
                    continue;
                }
                if (deadline != time_point() && sgcl::clock::now() >= deadline) {
                    co_return fail(net::detail::system_error(ETIMEDOUT, "write", c->describe()));
                }
                if (deadline == time_point()) {
                    co_await r->receive();
                } else {
                    co_await sgcl::async::select(r->on_receive([] {}), sgcl::async::timeout(deadline, [] {}));
                }
            }
            co_return done;
        }

        // Our EOF (RFC 4254 §5.3): the peer reads the end; once
        static async::task<expected<void, io::error>> co_channel_eof(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c) noexcept {
            auto lock = co_await c->write_lock.scoped_lock();   // after the writes in progress
            uint32_t remote;
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->eof_out || c->close_out || c->close_in || c->conn_gone) {
                    co_return expected<void, io::error>();
                }
                c->eof_out = true;
                remote = c->remote_id;
            }
            c->wake();
            Bytes b;
            Writer w(b);
            w.u8(MsgChannelEof).u32(remote);
            co_return co_await co_send(self, std::move(b));
        }

        // A channel ended by our side in one go, after its writes in
        // progress: a request without a reply first (a session's
        // exit-status, when `request` names one), our EOF, our CLOSE — posted
        // together, so that they leave in one write
        static async::task<void> co_finish_channel(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c, std::string request, Bytes extra) noexcept {
            auto lock = co_await c->write_lock.scoped_lock();
            bool free_now = false;
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->close_out || c->conn_gone || !c->opened) {
                    co_return;
                }
                if (!c->close_in) {
                    if (!request.empty()) {
                        Bytes b;
                        Writer w(b);
                        w.u8(MsgChannelRequest).u32(c->remote_id).string(request).boolean(false);
                        w.raw(extra.data(), extra.size());
                        self->post(std::move(b));
                    }
                    if (!c->eof_out) {
                        c->eof_out = true;
                        Bytes b;
                        Writer w(b);
                        w.u8(MsgChannelEof).u32(c->remote_id);
                        self->post(std::move(b));
                    }
                }
                c->close_out = true;
                Bytes b;
                Writer w(b);
                w.u8(MsgChannelClose).u32(c->remote_id);
                self->post(std::move(b));
                free_now = c->close_in;
            }
            c->wake();
            if (free_now) {
                self->remove_channel(c->local_id);
            }
        }

        // Our EOF without waiting (a connection's close_write, which may be
        // called in a task): posted after the write in progress, if any
        void eof_soon(const tracked_ptr<ChannelImpl>& c) {
            if (c->write_lock.try_lock()) {
                _post_eof(c);
                c->write_lock.unlock();
                return;
            }
            async::go(_eof_task(tracked_ptr<SshConn>(this), c));
        }

        static async::task<void> _eof_task(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c) noexcept {
            auto lock = co_await c->write_lock.scoped_lock();
            self->_post_eof(c);
        }

        void _post_eof(const tracked_ptr<ChannelImpl>& c) {
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->eof_out || c->close_out || c->close_in || c->conn_gone || c->closing) {
                    return;
                }
                c->eof_out = true;
                Bytes b;
                Writer w(b);
                w.u8(MsgChannelEof).u32(c->remote_id);
                post(std::move(b));
            }
            c->wake();
        }

        // Our CLOSE: the channel ends both ways; its id is freed when the
        // peer's CLOSE comes (or now, when it came first). Never waits for
        // the write lock: posted
        void close_channel(const tracked_ptr<ChannelImpl>& c) {
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->close_out || c->conn_gone) {
                    return;
                }
                if (!c->opened) {
                    c->close_out = true;   // sent once the open is confirmed
                    return;
                }
                c->closing = true;
            }
            c->wake();   // a write waiting for the window ends: its channel is closing
            async::go(_close_task(tracked_ptr<SshConn>(this), c));
        }

        // Our CLOSE, after the channel's writes in progress (their data must
        // not follow it: RFC 4254 §5.3), posted under the channel's lock so
        // that no window adjust follows it either; the id freed once both
        // CLOSEs are through
        static async::task<void> _close_task(tracked_ptr<SshConn> self, tracked_ptr<ChannelImpl> c) noexcept {
            auto lock = co_await c->write_lock.scoped_lock();
            bool free_now = false;
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->close_out || c->conn_gone) {
                    co_return;
                }
                c->close_out = true;
                Bytes b;
                Writer w(b);
                w.u8(MsgChannelClose).u32(c->remote_id);
                self->post(std::move(b));
                free_now = c->close_in;
            }
            c->wake();
            if (free_now) {
                self->remove_channel(c->local_id);
            }
        }

        // --- the end ------------------------------------------------------------

        // The connection ended with an error (the first one kept): the
        // transport closed, every wait woken
        void fail_with(const io::error& e) {
            {
                std::lock_guard<std::mutex> g(_q);
                if (!error) {
                    error = e;
                }
            }
            if (failed.exchange(true)) {
                return;
            }
            (void)transport.close();
            _gate->close();
            auth_inbox.close();
            ended->close();
            vector<tracked_ptr<ChannelImpl>> all;
            {
                std::lock_guard<std::mutex> g(_ch_m);
                for (auto& c : _channels) {
                    if (c) {
                        all.push_back(c);
                    }
                }
            }
            for (auto& c : all) {
                vector<tracked_ptr<async::detail::ChannelState<bool>>> pend;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    c->conn_gone = true;
                    pend = c->pending;
                    c->pending.clear();
                }
                for (auto& p : pend) {
                    p->close();
                }
                c->wake();
            }
            vector<tracked_ptr<GlobalReply>> replies;
            {
                std::lock_guard<std::mutex> g(_q);
                while (!_global_replies.empty()) {
                    replies.push_back(_global_replies.front());
                    _global_replies.pop_front();
                }
            }
            for (auto& r : replies) {
                r->done.close();
            }
            on_closed();
        }

        // A DISCONNECT sent (best effort, a second at most), then the end
        void disconnect(uint32_t reason, std::string_view text, const io::error& e) {
            if (failed.load() || disconnecting.exchange(true)) {
                return;
            }
            Bytes b;
            Writer w(b);
            w.u8(MsgDisconnect).u32(reason).string(text).string("");
            async::go(_disconnect(tracked_ptr<SshConn>(this), std::move(b), e));
        }

        io::error current_error() {
            std::lock_guard<std::mutex> g(_q);
            return error ? *error : ssh_error(net::errc::ssh_disconnected, describe() + ": the connection ended");
        }

        io::error protocol_error(std::string_view what) const {
            return ssh_error(net::errc::ssh_protocol, describe() + ": " + std::string(what));
        }

        // The peer's DISCONNECT as an error: its reason and text
        io::error disconnected(const uint8_t* p, size_t n) const {
            Reader r(p + 1, n - 1);
            uint32_t reason = r.u32();
            Span text = r.string();
            std::string m = describe() + ": disconnected by the peer: " + disconnect_reason(reason);
            if (text.n) {
                m += " (" + printable(text.view()) + ")";
            }
            return ssh_error(reason == DisconnectNoMoreAuthMethods ? net::errc::ssh_auth_failed : net::errc::ssh_disconnected, m);
        }

        // Compression of our writing direction from the next packet sealed
        // (zlib@openssh.com, after the authentication), when negotiated
        void start_compression_out() {
            std::lock_guard<std::mutex> g(_q);
            _comp_out = true;
            if (_zlib_out_wanted && !t->zout) {
                t->zout = std::make_unique<ZlibOut>();
            }
        }

        void start_compression_in() {
            bool want;
            {
                std::lock_guard<std::mutex> g(_q);
                want = _zlib_in_wanted;
            }
            _comp_in.store(true);
            if (want) {
                zlib_pending_in.store(true, std::memory_order_release);
            }
        }

        // The server's host key changed in a later exchange: refused
        bool same_host_key(const Bytes& blob) const noexcept {
            return host_key_blob == blob;
        }

        // One packet acted on as the read loop acts on it (what the fuzz
        // harnesses drive): an error ends the connection there
        SGCL_INLINE_HOT optional<io::error> dispatch(const uint8_t* p, size_t n) {
            return _dispatch(p, n);
        }

        // A packet written for the tests: the sealing and the write as any
        // other message, the key exchange's gate passed by (a message
        // injected where the protocol forbids it)
        static async::task<expected<void, io::error>> co_send_raw(tracked_ptr<SshConn> self, Bytes payload) noexcept {
            auto g = co_await self->write_lock.scoped_lock();
            {
                std::lock_guard<std::mutex> q(self->_q);
                self->_seal(payload.data(), payload.size(), nullptr, 0);
            }
            co_return co_await self->_write_out();
        }

        // The key exchanges completed (the first one included), for tests
        SGCL_INLINE_HOT uint64_t kex_count() const noexcept {
            return _kex_count.load(std::memory_order_relaxed);
        }

    protected:
        // --- the read loop --------------------------------------------------------

        static async::task<void> _read_loop(tracked_ptr<SshConn> self) noexcept {
            for (;;) {
                auto p = co_await self->co_read_packet();
                if (!p) {
                    self->fail_with(p.error());
                    break;
                }
                self->last_read_ns.store(sgcl::clock::now().time_since_epoch().count(), std::memory_order_relaxed);
                if (auto e = self->_dispatch(p->p, p->n)) {
                    if (e->code() == net::errc::ssh_protocol || e->code() == net::errc::ssh_handshake) {
                        self->disconnect(DisconnectProtocolError, e->path().view(), *e);
                    } else {
                        self->fail_with(*e);
                    }
                    break;
                }
                self->_check_rekey_in();
            }
        }

        // One packet acted on, never waiting; an error ends the connection
        optional<io::error> _dispatch(const uint8_t* p, size_t n) {
            if (n == 0) {
                return protocol_error("an empty packet");
            }
            const uint8_t msg = p[0];
            if (_skip_guess) {   // the peer's wrong guess (RFC 4253 §7)
                _skip_guess = false;
                if (msg >= 30 && msg <= 49) {
                    return nullopt;
                }
            }
            switch (msg) {
                case MsgDisconnect:
                    return disconnected(p, n);
                case MsgIgnore:
                case MsgDebug:
                case MsgUnimplemented:
                    return nullopt;
                case MsgKexinit:
                case MsgNewkeys:
                case MsgKexInit:
                case MsgKexReply:
                    return _kex_message(p, n);
                case MsgExtInfo:
                    _ext_info(p, n);
                    return nullopt;
                default:
                    break;
            }
            if (_step != KexStep::idle && msg >= 50) {
                return protocol_error("a message during the key exchange");
            }
            if (msg == MsgServiceRequest || msg == MsgServiceAccept || (msg >= 50 && msg <= 79)) {
                if (authenticated.load()) {
                    return nullopt;   // a late banner, an extra answer: passed over
                }
                if (is_client && msg == MsgUserauthSuccess) {
                    // compression starts with the next packet, both ways, and
                    // the server's connection messages may follow at once
                    // (OpenSSH's hostkeys-00@openssh.com)
                    start_compression_in();
                    start_compression_out();
                    authenticated.store(true);
                }
                if (!auth_inbox.try_send(string(std::string_view(reinterpret_cast<const char*>(p), n)))) {
                    return protocol_error("too many authentication messages");
                }
                return nullopt;
            }
            if (msg >= 80 && msg <= 127) {
                if (!authenticated.load()) {
                    return protocol_error("a connection message before the authentication");
                }
                return _connection_message(p, n);
            }
            // a message number of no layer here: unimplemented (RFC 4253 §11.4)
            Bytes b;
            Writer w(b);
            w.u8(MsgUnimplemented).u32(t->seq_in - 1);
            post(std::move(b));
            return nullopt;
        }

        optional<io::error> _connection_message(const uint8_t* p, size_t n) {
            Reader r(p + 1, n - 1);
            const uint8_t msg = p[0];
            switch (msg) {
                case MsgGlobalRequest: {
                    Span name = r.string();
                    bool want = r.boolean();
                    if (!r.ok()) {
                        return protocol_error("a malformed global request");
                    }
                    tracked_ptr<GlobalSlot> slot;
                    if (want) {
                        slot = make_tracked<GlobalSlot>();
                        std::lock_guard<std::mutex> g(_slots_m);
                        _slots.push_back(slot);
                    }
                    on_global_request(name.view(), r, slot);
                    return nullopt;
                }
                case MsgRequestSuccess:
                case MsgRequestFailure: {
                    tracked_ptr<GlobalReply> reply;
                    {
                        std::lock_guard<std::mutex> g(_q);
                        if (_global_replies.empty()) {
                            return protocol_error("a global reply with no request");
                        }
                        reply = _global_replies.front();
                        _global_replies.pop_front();
                    }
                    Span rest = r.rest();
                    reply->done.try_send(pair<bool, string>(msg == MsgRequestSuccess, string(rest.view())));
                    return nullopt;
                }
                case MsgChannelOpen: {
                    Span type = r.string();
                    uint32_t sender = r.u32();
                    uint32_t window = r.u32();
                    uint32_t max_packet = r.u32();
                    if (!r.ok()) {
                        return protocol_error("a malformed channel open");
                    }
                    on_channel_open(type.view(), sender, window, max_packet, r);
                    return nullopt;
                }
                default:
                    break;
            }
            uint32_t id = r.u32();
            if (!r.ok()) {
                return protocol_error("a malformed channel message");
            }
            tracked_ptr<ChannelImpl> c = channel(id);
            if (!c) {
                return protocol_error("a message for a channel that is not open");
            }
            switch (msg) {
                case MsgChannelOpenConfirmation: {
                    uint32_t sender = r.u32();
                    uint32_t window = r.u32();
                    uint32_t max_packet = r.u32();
                    if (!r.ok()) {
                        return protocol_error("a malformed open confirmation");
                    }
                    bool close_now = false;
                    {
                        std::lock_guard<std::mutex> g(c->m);
                        if (c->opened || c->open_error) {
                            return protocol_error("a second open confirmation");
                        }
                        c->remote_id = sender;
                        c->remote_window = window;
                        c->remote_max_packet = std::max<uint32_t>(1, std::min<uint32_t>(max_packet, 256 * 1024 - 1024));
                        c->opened = true;
                        close_now = c->close_out;   // closed by the program while it opened
                        c->close_out = false;
                    }
                    c->wake();
                    if (close_now) {
                        close_channel(c);
                    }
                    return nullopt;
                }
                case MsgChannelOpenFailure: {
                    uint32_t reason = r.u32();
                    Span text = r.string();
                    {
                        std::lock_guard<std::mutex> g(c->m);
                        if (c->opened || c->open_error) {
                            return protocol_error("an open failure for an open channel");
                        }
                        std::string m = describe() + ": " + c->type + ": ";
                        switch (reason) {
                            case OpenAdministrativelyProhibited: m += "administratively prohibited"; break;
                            case OpenConnectFailed: m += "connect failed"; break;
                            case OpenUnknownChannelType: m += "unknown channel type"; break;
                            case OpenResourceShortage: m += "resource shortage"; break;
                            default: m += "refused"; break;
                        }
                        if (text.n) {
                            m += " (" + printable(text.view()) + ")";
                        }
                        c->open_error = ssh_error(net::errc::ssh_channel_refused, m);
                    }
                    remove_channel(id);
                    c->wake();
                    return nullopt;
                }
                case MsgChannelWindowAdjust: {
                    uint32_t add = r.u32();
                    if (!r.ok()) {
                        return protocol_error("a malformed window adjust");
                    }
                    {
                        std::lock_guard<std::mutex> g(c->m);
                        // a window past 2^32 - 1 is held there (RFC 4254 §5.2)
                        c->remote_window = std::min<uint64_t>(c->remote_window + add, 0xFFFFFFFFu);
                    }
                    c->wake();
                    return nullopt;
                }
                case MsgChannelData:
                case MsgChannelExtendedData: {
                    int stream = 0;
                    if (msg == MsgChannelExtendedData) {
                        uint32_t code = r.u32();
                        if (code != 1) {   // only SSH_EXTENDED_DATA_STDERR is defined
                            Span skip = r.string();
                            (void)skip;
                            return r.ok() ? nullopt : optional<io::error>(protocol_error("malformed extended data"));
                        }
                        stream = 1;
                    }
                    Span data = r.string();
                    if (!r.done()) {
                        return protocol_error("malformed channel data");
                    }
                    if (data.n > c->local_max_packet + 1024 || !c->deliver(stream, data.p, data.n)) {
                        return protocol_error("channel data past the window or after the end");
                    }
                    return nullopt;
                }
                case MsgChannelEof: {
                    {
                        std::lock_guard<std::mutex> g(c->m);
                        c->eof_in = true;
                    }
                    c->wake();
                    return nullopt;
                }
                case MsgChannelClose: {
                    bool free_now = false;
                    vector<tracked_ptr<async::detail::ChannelState<bool>>> pend;
                    {
                        std::lock_guard<std::mutex> g(c->m);
                        if (c->close_in) {
                            return protocol_error("a second channel close");
                        }
                        c->close_in = true;
                        free_now = c->close_out;
                        pend = c->pending;
                        c->pending.clear();
                    }
                    for (auto& x : pend) {
                        x->close();
                    }
                    c->wake();
                    c->on_peer_close();
                    if (free_now) {
                        remove_channel(id);
                    } else {
                        close_channel(c);   // ours in answer, after the writes in progress
                    }
                    return nullopt;
                }
                case MsgChannelRequest: {
                    Span name = r.string();
                    bool want = r.boolean();
                    if (!r.ok()) {
                        return protocol_error("a malformed channel request");
                    }
                    c->on_request(name.view(), want, r);
                    return nullopt;
                }
                case MsgChannelSuccess:
                case MsgChannelFailure: {
                    tracked_ptr<async::detail::ChannelState<bool>> reply;
                    {
                        std::lock_guard<std::mutex> g(c->m);
                        if (c->pending.empty()) {
                            return protocol_error("a channel reply with no request");
                        }
                        reply = c->pending.front();
                        c->pending.erase(c->pending.begin());
                    }
                    reply->try_send(msg == MsgChannelSuccess);
                    c->wake();   // a wait that watches the answer beside the data (client::run)
                    return nullopt;
                }
                default:
                    return protocol_error("an unknown connection message");
            }
        }

        // RFC 8308: the server's algorithms of user signatures
        void _ext_info(const uint8_t* p, size_t n) {
            Reader r(p + 1, n - 1);
            uint32_t count = r.u32();
            for (uint32_t i = 0; i < count && r.ok(); ++i) {
                Span name = r.string();
                Span value = r.string();
                if (r.ok() && name.view() == "server-sig-algs" && is_client) {
                    server_sig_algs.clear();
                    Bytes b;
                    Writer w(b);
                    w.string(value);
                    Reader lr(b.data(), b.size());
                    for (auto a : lr.name_list()) {
                        server_sig_algs.emplace_back(a);
                    }
                }
            }
        }

        // --- the key exchange machine ----------------------------------------------

        // Whether a KEXINIT payload offers the peer's strict-KEX marker
        bool _strict_offered(const uint8_t* p, size_t n) const {
            Kexinit k;
            if (!read_kexinit(p, n, k)) {
                return false;
            }
            return contains_name(k.lists[ListKex], is_client ? StrictServer : StrictClient);
        }

        // Our KEXINIT queued when it is not out yet (under _q): the gate shut
        void _post_kexinit_locked(bool first) {
            if (_kex_out) {
                return;
            }
            _our_kexinit = make_kexinit(settings.prefs, is_client, first);
            _kex_q.push_back(OutItem{_our_kexinit, 0});
            _kex_out = true;
            _gate = make_tracked<async::detail::ChannelState<void>>();
        }

        void _post_kexinit(bool first) {
            bool start = false;
            {
                std::lock_guard<std::mutex> g(_q);
                _post_kexinit_locked(first);
                if (!_flushing) {
                    _flushing = true;
                    start = true;
                }
            }
            if (start) {
                async::go(_flush(tracked_ptr<SshConn>(this)));
            }
        }

        optional<io::error> _kex_message(const uint8_t* p, size_t n) {
            const uint8_t msg = p[0];
            if (msg == MsgKexinit) {
                if (_step != KexStep::idle) {
                    return protocol_error("a KEXINIT during a key exchange");
                }
                Kexinit peer;
                if (!read_kexinit(p, n, peer)) {
                    return protocol_error("a malformed KEXINIT");
                }
                Bytes ours;
                {
                    std::lock_guard<std::mutex> g(_q);
                    _post_kexinit_locked(session_id.empty());
                    ours = _our_kexinit;
                }
                _flush_soon();
                _peer_kexinit.assign(p, p + n);
                Kexinit mine;
                read_kexinit(ours.data(), ours.size(), mine);
                Negotiated neg;
                const char* e = is_client ? negotiate(mine, peer, true, neg) : negotiate(peer, mine, false, neg);
                if (e) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": " + e);
                }
                if (session_id.empty()) {
                    strict = neg.strict;
                    _strict_known = true;
                    if (strict && _peer_kexinit_count == 0 && t->packets_in != 1) {
                        return ssh_error(net::errc::ssh_handshake, describe() + ": strict KEX: the KEXINIT is not the first packet");
                    }
                    _peer_ext_info = neg.ext_info;
                }
                {
                    // the compression of this exchange's keys (zlib@openssh.com
                    // starts anew at each NEWKEYS once it has started, as
                    // OpenSSH's streams do)
                    std::lock_guard<std::mutex> g(_q);
                    _zlib_out_wanted = neg.zlib[is_client ? 0 : 1];
                    _zlib_in_wanted = neg.zlib[is_client ? 1 : 0];
                }
                ++_peer_kexinit_count;
                _neg = neg;
                _skip_guess = neg.guess_wrong;
                _exchange = std::make_unique<KexExchange>(*neg.kex);
                if (is_client) {
                    post(_exchange->client_init(), true);
                    _step = KexStep::await_reply;
                } else {
                    _step = KexStep::await_init;
                }
                return nullopt;
            }
            if (msg == MsgKexReply && is_client && _step == KexStep::await_reply) {
                Span host_key, signature;
                HashInputs in{local_version, peer_version, &_our_kexinit, &_peer_kexinit};
                if (const char* e = _exchange->client_reply(p, n, in, host_key, signature)) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": " + e);
                }
                Bytes blob(host_key.p, host_key.p + host_key.n);
                if (session_id.empty()) {
                    if (auto e = check_host_key(blob, *_neg.host_key, _exchange->h(), signature)) {
                        return e;
                    }
                    host_key_blob = blob;
                    host_key_alg.assign(_neg.host_key->name);
                } else {
                    ParsedKey pk;
                    if (!same_host_key(blob) || !parse_key(blob.data(), blob.size(), pk) ||
                        !verify(pk.key, _neg.host_key->signature, _exchange->h().data(), _exchange->h().size(), signature.p, signature.n)) {
                        return ssh_error(net::errc::ssh_handshake, describe() + ": the host key changed or its signature failed in a later key exchange");
                    }
                }
                _keys_ready();
                return nullopt;
            }
            if (msg == MsgKexInit && !is_client && _step == KexStep::await_init) {
                Bytes blob, sig;
                if (!host_key_of(*_neg.host_key, blob)) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": no host key of the algorithm negotiated");
                }
                HashInputs in{peer_version, local_version, &_peer_kexinit, &_our_kexinit};
                Bytes reply;
                if (const char* e = _exchange->server_init(p, n, in, blob, reply)) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": " + e);
                }
                if (!sign_host(*_neg.host_key, _exchange->h(), sig)) {
                    return ssh_error(net::errc::ssh_handshake, describe() + ": the host key did not sign");
                }
                Writer w(reply);
                w.string(sig);
                post(std::move(reply), true);
                _keys_ready();
                return nullopt;
            }
            if (msg == MsgNewkeys && _step == KexStep::await_newkeys && n == 1) {
                // the reading direction's new keys from the next packet on
                t->in = std::move(t->next_in);
                if (strict) {
                    t->seq_in = 0;
                }
                if (_comp_in.load()) {
                    bool want;
                    {
                        std::lock_guard<std::mutex> g(_q);
                        want = _zlib_in_wanted;
                    }
                    zlib_pending_in.store(false);
                    if (want) {
                        t->zin = std::make_unique<ZlibIn>();
                    } else {
                        t->zin.reset();
                    }
                }
                t->in_bytes.store(0, std::memory_order_relaxed);
                _step = KexStep::idle;
                return nullopt;
            }
            return protocol_error("an unexpected key exchange message");
        }

        // K and H known: the session id kept (the first H), the keys of both
        // directions derived, our NEWKEYS posted (its keys installed after it
        // is sealed), and an EXT_INFO after it when the server offers one
        void _keys_ready() {
            const bool first = session_id.empty();
            if (first) {
                session_id = _exchange->h();
                _have_session.store(true, std::memory_order_release);
            }
            const int out_dir = is_client ? 0 : 1;
            t->next_out = std::make_unique<PacketKeys>();
            t->next_in = std::make_unique<PacketKeys>();
            install_keys(*t->next_out, _neg, out_dir, *_exchange, session_id);
            install_keys(*t->next_in, _neg, 1 - out_dir, *_exchange, session_id);
            Bytes nk{MsgNewkeys};
            post(std::move(nk), true, 1);
            if (first && !is_client && _peer_ext_info) {
                Bytes b;
                Writer w(b);
                w.u8(MsgExtInfo).u32(1).string("server-sig-algs");
                std::vector<std::string_view> algs = {"ssh-ed25519", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp384", "ecdsa-sha2-nistp521", "rsa-sha2-512", "rsa-sha2-256"};
                w.name_list(algs);
                post(std::move(b), true);
            }
            _step = KexStep::await_newkeys;
        }

        // A key exchange from our side after the byte or time limit, when
        // none runs (under _q)
        void _maybe_rekey_locked() {
            if (_kex_out || !_have_session.load(std::memory_order_acquire) || _step.load(std::memory_order_acquire) != KexStep::idle) {
                return;
            }
            const bool due = t->out_bytes >= settings.rekey_bytes || t->in_bytes.load(std::memory_order_relaxed) >= settings.rekey_bytes ||
                             (settings.rekey_interval > duration::zero() && sgcl::clock::now() - _kex_time >= settings.rekey_interval);
            if (due) {
                _post_kexinit_locked(false);
            }
        }

        void _check_rekey_in() {
            bool start = false;
            {
                std::lock_guard<std::mutex> g(_q);
                bool was = _kex_out;
                _maybe_rekey_locked();
                if (!was && _kex_out && !_flushing) {
                    _flushing = true;
                    start = true;
                }
            }
            if (start) {
                async::go(_flush(tracked_ptr<SshConn>(this)));
            }
        }

        void _flush_soon() {
            bool start = false;
            {
                std::lock_guard<std::mutex> g(_q);
                if (!_flushing && (!_kex_q.empty() || (!_kex_out && !_norm_q.empty()))) {
                    _flushing = true;
                    start = true;
                }
            }
            if (start) {
                async::go(_flush(tracked_ptr<SshConn>(this)));
            }
        }

        // --- sealing (under the write lock and _q) -----------------------------------

        void _seal(const uint8_t* head, size_t head_size, const uint8_t* body, size_t body_size) {
            Transport& tr = *t;
            const size_t before = tr.wbuf.size();
            if (tr.zout) {
                Bytes joined;
                const uint8_t* p = head;
                size_t n = head_size;
                if (body_size) {
                    joined.reserve(head_size + body_size);
                    joined.insert(joined.end(), head, head + head_size);
                    joined.insert(joined.end(), body, body + body_size);
                    p = joined.data();
                    n = joined.size();
                }
                tr.zout->compress(p, n, tr.zbuf);
                tr.out->seal(tr.seq_out, tr.zbuf.data(), tr.zbuf.size(), tr.wbuf);
            } else {
                tr.out->seal(tr.seq_out, head, head_size, body, body_size, tr.wbuf);
            }
            ++tr.seq_out;
            tr.out_bytes += tr.wbuf.size() - before;
        }

        // The posted messages sealed: the key exchange's, or (kex false) the
        // rest, which wait while an exchange runs
        void _seal_posted_locked(bool kex) {
            auto& q = kex ? _kex_q : _norm_q;
            while (!q.empty()) {
                if (!kex && _kex_out) {
                    return;
                }
                OutItem item = std::move(q.front());
                q.pop_front();
                _seal(item.payload.data(), item.payload.size(), nullptr, 0);
                if (item.action == 1) {
                    _install_out_locked();
                }
            }
        }

        // After our NEWKEYS: the writing direction's new keys, the sequence
        // number from 0 with strict KEX, the gate opened when the reading
        // direction is done too or later (the writers may go on as soon as
        // our NEWKEYS is out: RFC 4253 §7.3)
        void _install_out_locked() {
            Transport& tr = *t;
            tr.out = std::move(tr.next_out);
            if (strict) {
                tr.seq_out = 0;
            }
            if (_comp_out) {
                if (_zlib_out_wanted) {
                    tr.zout = std::make_unique<ZlibOut>();
                } else {
                    tr.zout.reset();
                }
            }
            tr.out_bytes = 0;
            _kex_out = false;
            _kex_time = sgcl::clock::now();
            _gate->close();
            _kex_count.fetch_add(1, std::memory_order_relaxed);
        }

        // The sealed bytes written (under the write lock, outside _q)
        // The sealed bytes begun on the transport without a frame
        // (ConnImpl::start_write): true when they are all written (or the
        // write failed, which ends the connection), false when a task writes
        // the rest (`rest`; the buffer is held until it ends)
        bool _start_out(optional<async::task<expected<size_t, io::error>>>& rest) {
            Transport& tr = *t;
            if (tr.wbuf.empty()) {
                return true;
            }
            auto st = net::detail::ConnectionAccess::impl(transport).start_write(slice<const byte>(reinterpret_cast<const byte*>(tr.wbuf.data()), tr.wbuf.size()));
            if (st.rest) {
                rest = std::move(st.rest);
                return false;
            }
            _out_done(st.done);
            return true;
        }

        void _out_done(const expected<size_t, io::error>& w) {
            Transport& tr = *t;
            tr.wbuf.clear();
            if (tr.wbuf.capacity() > 1024 * 1024) {
                Bytes().swap(tr.wbuf);
            }
            if (!w) {
                fail_with(w.error());
            }
        }

        // The sealed bytes written (under the write lock, outside _q)
        async::task<expected<void, io::error>> _write_out() noexcept {
            optional<async::task<expected<size_t, io::error>>> rest;
            if (!_start_out(rest)) {
                auto w = co_await std::move(*rest);
                _out_done(w);
            }
            if (failed.load(std::memory_order_acquire)) {
                co_return fail(current_error());
            }
            co_return expected<void, io::error>();
        }

        // The posted messages sealed and written here, the write lock held
        // (taken by post's try): a write the transport does not take whole
        // goes on in a task, which flushes what comes meanwhile
        void _flush_now(async::mutex::guard g) {
            for (;;) {
                {
                    std::lock_guard<std::mutex> q(_q);
                    if (failed.load()) {
                        _flushing = false;
                        return;
                    }
                    _seal_posted_locked(true);
                    _seal_posted_locked(false);
                    _seal_posted_locked(true);
                    if (t->wbuf.empty()) {
                        _flushing = false;
                        return;
                    }
                }
                optional<async::task<expected<size_t, io::error>>> rest;
                if (!_start_out(rest)) {
                    async::go(_flush_rest(tracked_ptr<SshConn>(this), std::move(*rest), std::move(g)));
                    return;
                }
            }
        }

        static async::task<void> _flush_rest(tracked_ptr<SshConn> self, async::task<expected<size_t, io::error>> rest, async::mutex::guard g) noexcept {
            auto w = co_await std::move(rest);
            self->_out_done(w);
            co_await _flush_locked(self);
        }

        static async::task<void> _flush(tracked_ptr<SshConn> self) noexcept {
            auto g = co_await self->write_lock.scoped_lock();
            co_await _flush_locked(self);
        }

        // The flusher's loop, the write lock held by the caller
        static async::task<void> _flush_locked(tracked_ptr<SshConn> self) noexcept {
            for (;;) {
                {
                    std::lock_guard<std::mutex> q(self->_q);
                    if (self->failed.load()) {
                        self->_flushing = false;
                        co_return;
                    }
                    self->_seal_posted_locked(true);
                    self->_seal_posted_locked(false);
                    self->_seal_posted_locked(true);
                    if (self->t->wbuf.empty()) {
                        self->_flushing = false;
                        co_return;
                    }
                }
                auto w = co_await self->_write_out();
                if (!w) {
                    std::lock_guard<std::mutex> q(self->_q);
                    self->_flushing = false;
                    co_return;
                }
            }
        }

        static async::task<void> _disconnect(tracked_ptr<SshConn> self, Bytes payload, io::error e) noexcept {
            self->transport.set_write_deadline(sgcl::clock::now() + second);
            {
                std::lock_guard<std::mutex> q(self->_q);
                if (!self->error) {
                    self->error = e;
                }
            }
            (void)co_await co_send_raw(self, std::move(payload));
            self->fail_with(e);
        }

        // keepalive@openssh.com with want_reply while the peer is silent;
        // count_max unanswered in a row end the connection
        static async::task<void> _keepalive(tracked_ptr<SshConn> self) noexcept {
            const duration every = self->settings.keepalive_interval;
            int missed = 0;
            bool probing = false;
            int64_t probed_at = 0;
            while (!self->failed.load()) {
                co_await async::sleep(every);
                if (self->failed.load()) {
                    break;
                }
                const int64_t last = self->last_read_ns.load(std::memory_order_relaxed);
                const int64_t now = sgcl::clock::now().time_since_epoch().count();
                if (probing && last >= probed_at) {   // something came since the last probe
                    probing = false;
                    missed = 0;
                }
                if (now - last < every.nanoseconds()) {
                    continue;
                }
                if (probing && ++missed >= self->settings.keepalive_count_max) {
                    self->fail_with(ssh_error(net::errc::ssh_disconnected, self->describe() + ": the peer stopped answering keepalives"));
                    break;
                }
                probing = true;
                probed_at = now;
                Bytes b;
                Writer w(b);
                w.u8(MsgGlobalRequest).string("keepalive@openssh.com").boolean(true);
                async::go(_keepalive_one(self, std::move(b)));
            }
        }

        static async::task<void> _keepalive_one(tracked_ptr<SshConn> self, Bytes b) noexcept {
            (void)co_await co_global_request(self, std::move(b));
        }

        std::mutex _q;                       // the queues, the kex flags, the error
        std::deque<OutItem> _kex_q;
        std::deque<OutItem> _norm_q;
        bool _flushing = false;
        bool _kex_out = false;               // our KEXINIT queued or sent, our NEWKEYS not yet sealed
        tracked_ptr<async::detail::ChannelState<void>> _gate;   // closed when no exchange shuts the writers out
        Bytes _our_kexinit;
        Bytes _peer_kexinit;
        std::atomic<KexStep> _step{KexStep::idle};   // the read loop's; read by the writers' rekey check
        std::atomic<bool> _have_session{false};      // the first exchange's H kept
        Negotiated _neg;
        std::unique_ptr<KexExchange> _exchange;
        bool _skip_guess = false;
        bool _strict_known = false;
        bool _peer_ext_info = false;
        bool _zlib_out_wanted = false;       // under _q
        bool _zlib_in_wanted = false;        // under _q
        bool _comp_out = false;              // delayed compression began (under _q)
        std::atomic<bool> _comp_in{false};
        uint64_t _peer_kexinit_count = 0;
        time_point _kex_time = sgcl::clock::now();
        std::atomic<uint64_t> _kex_count{0};
        deque<tracked_ptr<GlobalReply>> _global_replies;   // under _q
        std::mutex _slots_m;
        deque<tracked_ptr<GlobalSlot>> _slots;
        std::mutex _ch_m;
        vector<tracked_ptr<ChannelImpl>> _channels;
    };

    inline string ChannelImpl::describe() const {
        return string("ssh " + conn->describe() + " channel " + std::to_string(local_id));
    }

inline bool ChannelImpl::_deliver_locked(int stream, const uint8_t* p, size_t n, bool& dropped) {
        {
            std::lock_guard<std::mutex> g(m);
            if (n > local_window || eof_in || close_in) {
                return false;
            }
            if (discard) {   // the window back at once: nobody reads
                dropped = true;
                if (n && !close_out) {
                    Bytes a;
                    Writer w(a);
                    w.u8(MsgChannelWindowAdjust).u32(remote_id).u32(uint32_t(n));
                    conn->post(std::move(a));
                }
                return true;
            }
            local_window -= uint32_t(n);
            Bytes& b = in[stream];
            if (in_pos[stream] == b.size()) {
                b.clear();
                in_pos[stream] = 0;
            } else if (in_pos[stream] > 65536 && in_pos[stream] * 2 > b.size()) {
                b.erase(b.begin(), b.begin() + ptrdiff_t(in_pos[stream]));
                in_pos[stream] = 0;
            }
            b.insert(b.end(), p, p + n);
        }
        wake();
        return true;
    }

    inline bool ChannelImpl::deliver(int stream, const uint8_t* p, size_t n) {
        bool dropped = false;
        return _deliver_locked(stream, p, n, dropped);
    }

    // A session's end as the peer reports it (RFC 4254 §6.10); everything
    // else refused when a reply is wanted
    inline void ChannelImpl::on_request(std::string_view name, bool want_reply, Reader& r) {
        bool ok = false;
        if (name == "exit-status") {
            uint32_t code = r.u32();
            if (r.ok()) {
                std::lock_guard<std::mutex> g(m);
                exit_status = code;
                ok = true;
            }
        } else if (name == "exit-signal") {
            Span sig = r.string();
            bool core = r.boolean();
            Span msg = r.string();
            (void)r.string();
            if (r.ok()) {
                std::lock_guard<std::mutex> g(m);
                exit_signal.assign(printable(sig.view()));
                core_dumped = core;
                exit_message.assign(printable(msg.view()));
                ok = true;
            }
        }
        if (ok) {
            wake();
        }
        if (want_reply) {
            Bytes b;
            Writer w(b);
            w.u8(ok ? MsgChannelSuccess : MsgChannelFailure).u32(remote_id);
            conn->post(std::move(b));
        }
    }
}
