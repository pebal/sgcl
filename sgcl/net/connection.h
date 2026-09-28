//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "detail/fd.h"
#include "detail/sockaddr.h"
#include "error.h"
#include "ip.h"
#include "../async/channel.h"
#include "../async/mutex.h"
#include "../async/select.h"
#include "../async/stop_token.h"
#include "../async/timer.h"
#include "../core/vector.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../io/buffered.h"
#include "../io/stream.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace sgcl::net {
    // The connections of the module: a stream of bytes both ways (a TCP or
    // unix socket, a pair of ends in memory; a TLS session in the next
    // stage), a listener that accepts them, and a Datagram socket. Each is
    // a handle of one word, a tracked_ptr to the object inside: a copy is
    // the same connection (as a *net.TCPConn in Go is), and a handle
    // passed by value into a task keeps the connection alive for as long
    // as the task runs.
    //
    // Every operation that may wait comes twice, as everywhere in the
    // library: read() takes the thread until the data comes, `co_await
    // async_read()` gives the worker back meanwhile. Both wait on the
    // reactor (reactor.h), so that a close from another task or thread
    // ends either at once, and a deadline on the module's clock ends
    // either when it passes (manual_clock included): the blocking form
    // parks the thread in the descriptor's slot on the reactor, where io::file's
    // polls the descriptor, which neither a close nor the manual clock
    // could interrupt.
    //
    // One read and one write may run at once (full duplex); two reads at
    // once are taken one after the other, as are two writes, so that a
    // write from each of two tasks lands whole. A write writes everything
    // or fails. The deadlines are absolute (set_deadline): a read that
    // starts past its deadline, or waits past it, fails with ETIMEDOUT
    // (e.is_timeout()) and reads nothing. close() from another task ends
    // the reads, writes and accepts in progress with io::errc::closed.
    class connection;

    namespace detail {
        using sgcl::io::detail::fail;

        inline io::error closed_error(const char* op, const string& what) {
            return io::error(io::errc::closed, op, what);
        }

        // A wait that did not end in readiness, as the operation's error
        inline io::error wait_error(WaitResult r, const Descriptor& d, const char* op, const string& what) {
            if (r == WaitResult::closed) {
                return closed_error(op, what);
            }
            if (r == WaitResult::failed) {
                return io::error(d.wait_failure(), op, what);
            }
            return system_error(r == WaitResult::timed_out ? ETIMEDOUT : ECANCELED, op, what);
        }

        // The same as an errno value, for the connect, whose error is one:
        // ENOTSUP for a number the reactor has no slot for
        inline int wait_errno(WaitResult r, const Descriptor& d) noexcept {
            if (r == WaitResult::failed) {
                auto e = d.wait_failure();
                return e.category() == std::system_category() ? e.value() : ENOTSUP;
            }
            return r == WaitResult::timed_out ? ETIMEDOUT : ECANCELED;
        }

        // TCP_KEEPALIVE is macOS's name of what Linux calls TCP_KEEPIDLE
        inline int set_keep_alive(int fd, std::chrono::nanoseconds idle) noexcept {
            int on = idle > std::chrono::nanoseconds::zero() ? 1 : 0;
            if (::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) != 0) {
                return errno;
            }
            if (!on) {
                return 0;
            }
            auto s = std::chrono::ceil<std::chrono::seconds>(idle).count();
            int secs = int(std::clamp<long long>(s, 1, 0x7fffffff));
#if defined(TCP_KEEPALIVE)
            ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &secs, sizeof(secs));
#elif defined(TCP_KEEPIDLE)
            ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &secs, sizeof(secs));
#endif
#if defined(TCP_KEEPINTVL)
            ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &secs, sizeof(secs));
#endif
#if defined(TCP_KEEPCNT)
            int count = 9;
            ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
#endif
            return 0;
        }

        // What a new TCP connection gets, as in Go: no Nagle, keep-alive
        // probes after fifteen seconds of silence
        inline void tune_tcp(int fd) noexcept {
            int one = 1;
            ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            set_keep_alive(fd, std::chrono::seconds(15));
        }

        class ConnImpl;

        // The readiness of a transport to be read, awaited without a frame
        // of its own (`co_await impl.raw_readable()`: expected<void,
        // io::error>, the error of a close or of the read deadline): a
        // descriptor's wait (io::detail::Descriptor::async_wait) in the
        // awaiting coroutine's frame. A transport with no descriptor to
        // wait on gives none (supported() false), and its reader reads the
        // waiting way
        class readiness {
        public:
            readiness() noexcept = default;

            readiness(Descriptor& d, const ConnImpl& c) noexcept
            : _d(&d)
            , _c(&c) {
            }

            bool supported() const noexcept {
                return _d != nullptr;
            }

            bool await_ready() {
                _op.emplace(_d->async_wait(Descriptor::Read));
                return _op->await_ready();
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                return _op->await_suspend(h);
            }

            expected<void, io::error> await_resume();   // below ConnImpl

        private:
            Descriptor* _d = nullptr;
            const ConnImpl* _c = nullptr;
            optional<Descriptor::wait_op> _op;
        };

        // The implementation under a connection handle, for the modules
        // built on net (http: a write begun without a frame)
        struct ConnectionAccess {
            static ConnImpl& impl(const connection& c) noexcept;
        };

        // The raw side of a connection, under the buffer read_line puts in
        // front of it
        class ConnRawReader final {
        public:
            explicit ConnRawReader(const tracked_ptr<ConnImpl>& c) noexcept
            : _c(c) {
            }

            expected<size_t, io::error> read(const slice<byte>& buffer);
            async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer);

        private:
            tracked_ptr<ConnImpl> _c;
        };

        // A connection, whatever carries it: a stream whose read goes
        // through the line buffer once read_line has made one, and whose
        // reads and writes are each taken one at a time (a mutex of the
        // library per direction, which parks no worker). The transports
        // give the raw operations.
        class ConnImpl
        : public io::mixin::reader<ConnImpl>
        , public io::mixin::writer<ConnImpl> {
        public:
            using io::mixin::writer<ConnImpl>::write;
            using io::mixin::writer<ConnImpl>::async_write;

            virtual ~ConnImpl() = default;

            expected<size_t, io::error> read(const slice<byte>& buffer) {
                if (buffer.empty()) {
                    return size_t(0);
                }
                std::lock_guard<sgcl::async::mutex> guard(_read_lock);
                return _buffered ? _buffered->read(buffer) : raw_read(buffer);
            }

            async::task<expected<size_t, io::error>> async_read(slice<byte> buffer) {
                if (buffer.empty()) {
                    co_return size_t(0);
                }
                auto guard = co_await _read_lock.scoped_lock();
                if (_buffered) {
                    co_return co_await _buffered->async_read(buffer);
                }
                co_return co_await awaited_raw_read(buffer);
            }

            expected<size_t, io::error> write(const slice<const byte>& data) {
                std::lock_guard<sgcl::async::mutex> guard(_write_lock);
                return raw_write(data);
            }

            async::task<expected<size_t, io::error>> async_write(slice<const byte> data) {
                auto guard = co_await _write_lock.scoped_lock();
                co_return co_await awaited_raw_write(data);
            }

            // A read without a frame, for a reader that waits for the
            // transport's readiness itself (raw_readable): the bytes there
            // now (n > 0), 0 at the end of the stream, nothing (nullopt)
            // when the transport would wait; or its error. `slow` set, and
            // nothing read, when this way does not serve: a transport that
            // cannot try, a read in progress, a line buffer that may hold
            // bytes (read_line was used). The caller then reads the waiting
            // way (async_read). Through the same checks as a read that
            // waits (the transport's try_raw_read). A server's head read
            // this way costs one frame, the reader's, where the waiting way
            // was three (the fill's, this class's, the transport's)
            expected<optional<size_t>, io::error> try_read(const slice<byte>& buffer, bool& slow) {
                slow = false;
                if (!raw_readable().supported() || !_read_lock.try_lock()) {
                    slow = true;
                    return optional<size_t>();
                }
                if (_buffered) {
                    _read_lock.unlock();
                    slow = true;
                    return optional<size_t>();
                }
                auto r = try_raw_read(buffer);
                _read_lock.unlock();
                return r;
            }

            // A write begun without a frame: what the transport takes at
            // once, when no other write is in progress (try_raw_write), and
            // a task for the rest only when it would wait. done is the
            // write's result when it ended now (all of it taken, or an
            // error); rest, when set, is the write going on, which holds
            // the write lock from here to its end, so that no other write
            // lands between the two parts. A server's response that the
            // socket takes whole costs no coroutine frame (four of them
            // per response before: the writer's, the send's, this class's
            // and the transport's).
            struct started_write {
                expected<size_t, io::error> done = size_t(0);
                optional<async::task<expected<size_t, io::error>>> rest;
            };

            started_write start_write(const slice<const byte>& data) {
                started_write s;
                if (data.empty()) {
                    return s;
                }
                if (!_write_lock.try_lock()) {   // another write in progress: this one after it, the whole of it
                    s.rest.emplace(_write_rest(tracked_ptr<ConnImpl>(this), data, 0, nullopt));
                    return s;
                }
                auto r = try_raw_write(data);
                if (!r || *r == data.size()) {
                    _write_lock.unlock();
                    s.done = std::move(r);
                    return s;
                }
                s.rest.emplace(_write_rest(tracked_ptr<ConnImpl>(this), data, *r, sgcl::async::mutex::guard(_write_lock)));   // the lock handed to the task
                return s;
            }

            // A line without its "\n" (or "\r\n"), copied out of the
            // buffer; nullopt at the end of the stream
            // `c.read_line()` on this thread, `co_await c.async_read_line()` in a task
            expected<optional<string>, io::error> read_line() {
                return _block_read_line();
            }

            async::task<expected<optional<string>, io::error>> async_read_line() {
                return _co_read_line();
            }

            expected<optional<string>, io::error> _block_read_line()  {
                std::lock_guard<sgcl::async::mutex> guard(_read_lock);
                return _line_of(_buffer().read_line());
            }

            async::task<expected<optional<string>, io::error>> _co_read_line()  {
                auto guard = co_await _read_lock.scoped_lock();
                co_return _line_of(co_await _buffer().async_read_line());
            }

            // The close in a task, as the handle's async_close: an io::reader
            // or io::writer made of a connection binds this object, and its
            // async_close comes here rather than to the blocking pool
            virtual async::task<expected<void, io::error>> async_close() {
                co_return close();
            }

            // Takes no lock: a read in progress keeps the bound it started
            // with, the next read_line takes the new one
            void set_max_line(size_t n) noexcept {
                _max_line.store(n, std::memory_order_relaxed);
            }

            virtual expected<size_t, io::error> raw_read(const slice<byte>& buffer) = 0;
            virtual async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> buffer) = 0;
            virtual expected<size_t, io::error> raw_write(const slice<const byte>& data) = 0;
            virtual async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) = 0;

            // What the transport has for buffer without waiting: n > 0, 0 at
            // the end of the stream, nullopt when it would wait; or its
            // error. The default has nothing (a transport without it is
            // read the waiting way: raw_readable says it cannot tell)
            virtual expected<optional<size_t>, io::error> try_raw_read(const slice<byte>&) {
                return optional<size_t>();
            }

            // The transport's readiness to be read, as an awaitable without
            // a frame; the default cannot tell (readiness::supported false)
            virtual readiness raw_readable() {
                return readiness();
            }

            // What the transport takes of data without waiting: all of it,
            // a part, or none (0: it would wait); or its error. Through the
            // same checks as a write that waits (a close, the deadline).
            // The default takes none: the write goes the waiting way
            virtual expected<size_t, io::error> try_raw_write(const slice<const byte>&) {
                return size_t(0);
            }

            // Ends the connection both ways; the operations in progress end
            // with io::errc::closed
            virtual expected<void, io::error> close() = 0;
            virtual bool is_closed() const noexcept = 0;
            virtual expected<void, io::error> close_write() = 0;
            virtual void set_deadline(int dir, time_point t) = 0;

            virtual endpoint local_endpoint() const {
                return endpoint();
            }

            virtual endpoint remote_endpoint() const {
                return endpoint();
            }

            virtual string path() const {
                return string();
            }

            virtual expected<void, io::error> set_no_delay(bool) {
                return fail(system_error(EOPNOTSUPP, "set_no_delay", describe()));
            }

            virtual expected<void, io::error> set_keep_alive(std::chrono::nanoseconds) {
                return fail(system_error(EOPNOTSUPP, "set_keep_alive", describe()));
            }

            // What an error names the connection by: "tcp 1.2.3.4:5->6.7.8.9:80"
            virtual string describe() const = 0;

        private:
            // The rest of a write begun by start_write, from `done` on;
            // `held`: the write lock taken by start_write, a parameter, so
            // that a task dropped without being awaited lets go of it with
            // its frame
            static async::task<expected<size_t, io::error>> _write_rest(tracked_ptr<ConnImpl> self, slice<const byte> data, size_t done, optional<sgcl::async::mutex::guard> held) {
                if (!held) {
                    held.emplace(co_await self->_write_lock.scoped_lock());
                }
                auto r = co_await self->awaited_raw_write(slice<const byte>(data.data() + done, data.size() - done));
                held.reset();   // at the write's end, not the frame's: the frame lives as long as whoever holds the task
                if (!r) {
                    co_return fail(r);
                }
                co_return done + *r;
            }

            // Under the read lock
            io::detail::BufferedReaderState& _buffer() {
                if (!_buffered) {
                    _buffered = make_tracked<io::detail::BufferedReaderState>(io::reader(tracked_ptr<ConnRawReader>(make_tracked<ConnRawReader>(tracked_ptr<ConnImpl>(this)))), io::detail::UnmanagedBlock());
                }
                _buffered->set_max_line(_max_line.load(std::memory_order_relaxed));
                return *_buffered;
            }

            static expected<optional<string>, io::error> _line_of(const expected<optional<slice<const char>>, io::error>& r) {
                if (!r) {
                    return fail(r);
                }
                if (!*r) {
                    return optional<string>();
                }
                return optional<string>(string(**r));
            }

            sgcl::async::mutex _read_lock;
            sgcl::async::mutex _write_lock;
            tracked_ptr<io::detail::BufferedReaderState> _buffered;   // made by the first read_line; its block unmanaged, every line copied out under the lock
            std::atomic<size_t> _max_line = {64 * 1024};  // a line from the network is bounded
        };

        inline expected<void, io::error> readiness::await_resume() {
            auto r = _op->await_resume();
            if (r == WaitResult::ready) {
                return expected<void, io::error>();
            }
            return fail(wait_error(r, *_d, "read", _c->describe()));
        }

        inline expected<size_t, io::error> ConnRawReader::read(const slice<byte>& buffer) {
            return _c->raw_read(buffer);
        }

        inline async::task<expected<size_t, io::error>> ConnRawReader::async_read(const slice<byte>& buffer) {
            return _c->awaited_raw_read(buffer);
        }

        // A connection over a socket, TCP or unix
        class SocketConn final : public ConnImpl {
        public:
            SocketConn(int fd, bool tcp, endpoint local, endpoint remote, const string& path) noexcept
            : _d(fd)
            , _local(local)
            , _remote(remote)
            , _path(path)
            , _tcp(tcp) {
            }

            expected<size_t, io::error> raw_read(const slice<byte>& b) override {
                if (b.empty()) {
                    return size_t(0);
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("read", describe()));
                }
                bool look = true;   // this try reads the clock for the deadline; the next one not (the wait between them looked: its _result, or its start when this try did not), so one read of the clock a try and a wait instead of two
                for (;;) {
                    auto n = _recv_now(b, look);
                    if (!n) {
                        return fail(n);
                    }
                    if (*n) {
                        return **n;
                    }
                    auto r = _d.wait(Descriptor::Read, look);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "read", describe()));
                    }
                    look = !look;
                }
            }

            async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> b) override {
                if (b.empty()) {
                    co_return size_t(0);
                }
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("read", describe()));
                }
                bool look = true;   // this try reads the clock for the deadline; the next one not (the wait between them looked: its _result, or its start when this try did not), so one read of the clock a try and a wait instead of two
                for (;;) {
                    auto n = _recv_now(b, look);
                    if (!n) {
                        co_return fail(n);
                    }
                    if (*n) {
                        co_return **n;
                    }
                    auto r = co_await _d.async_wait(Descriptor::Read, look);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "read", describe()));
                    }
                    look = !look;
                }
            }

            expected<optional<size_t>, io::error> try_raw_read(const slice<byte>& b) override {
                if (b.empty()) {
                    return optional<size_t>(size_t(0));
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("read", describe()));
                }
                return _recv_now(b);
            }

            readiness raw_readable() override {
                return readiness(_d, *this);
            }

            expected<size_t, io::error> raw_write(const slice<const byte>& data) override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("write", describe()));
                }
                size_t written = 0;
                bool look = true;   // this try reads the clock for the deadline; the next one not (the wait between them looked: its _result, or its start when this try did not), so one read of the clock a try and a wait instead of two
                for (;;) {
                    auto would_wait = _send_now(data, written, look);
                    if (!would_wait) {
                        return fail(would_wait);
                    }
                    if (!*would_wait) {
                        return written;
                    }
                    auto r = _d.wait(Descriptor::Write, look);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "write", describe()));
                    }
                    look = !look;
                }
            }

            async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) override {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("write", describe()));
                }
                size_t written = 0;
                bool look = true;   // this try reads the clock for the deadline; the next one not (the wait between them looked: its _result, or its start when this try did not), so one read of the clock a try and a wait instead of two
                for (;;) {
                    auto would_wait = _send_now(data, written, look);
                    if (!would_wait) {
                        co_return fail(would_wait);
                    }
                    if (!*would_wait) {
                        co_return written;
                    }
                    auto r = co_await _d.async_wait(Descriptor::Write, look);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "write", describe()));
                    }
                    look = !look;
                }
            }

            expected<size_t, io::error> try_raw_write(const slice<const byte>& data) override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("write", describe()));
                }
                size_t written = 0;
                auto would_wait = _send_now(data, written);
                if (!would_wait) {
                    return fail(would_wait);
                }
                return written;
            }

            expected<void, io::error> close() override {
                int e = _d.close();
                if (e) {
                    return fail(system_error(e, "close", describe()));
                }
                return {};
            }

            bool is_closed() const noexcept override {
                return _d.closing();
            }

            expected<void, io::error> close_write() override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("close_write", describe()));
                }
                if (::shutdown(_d.fd(), SHUT_WR) != 0) {
                    return fail(system_error(errno, "close_write", describe()));
                }
                return {};
            }

            void set_deadline(int dir, time_point t) override {
                _d.set_deadline(dir, t);
            }

            endpoint local_endpoint() const override {
                return _local;
            }

            endpoint remote_endpoint() const override {
                return _remote;
            }

            string path() const override {
                return _path;
            }

            expected<void, io::error> set_no_delay(bool on) override {
                if (!_tcp) {
                    return ConnImpl::set_no_delay(on);
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("set_no_delay", describe()));
                }
                int v = on ? 1 : 0;
                if (::setsockopt(_d.fd(), IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) != 0) {
                    return fail(system_error(errno, "set_no_delay", describe()));
                }
                return {};
            }

            expected<void, io::error> set_keep_alive(std::chrono::nanoseconds idle) override {
                if (!_tcp) {
                    return ConnImpl::set_keep_alive(idle);
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("set_keep_alive", describe()));
                }
                if (int e = detail::set_keep_alive(_d.fd(), idle)) {
                    return fail(system_error(e, "set_keep_alive", describe()));
                }
                return {};
            }

            string describe() const override {
                if (!_tcp) {
                    return string("unix ") + _path;
                }
                return string("tcp ") + _local.to_string() + "->" + _remote.to_string();
            }

            // A connect that answered EINPROGRESS, waited for: 0 once
            // connected, or the error (an errno value; ECANCELED for the
            // stop, and for the reactor stopping). The socket is not shared
            // yet, so the local endpoint is set here too.
            int connected() {
                Operation op(_d);
                if (!op) {
                    return ECANCELED;
                }
                for (;;) {
                    int st = _connect_state();
                    if (st != EINPROGRESS) {
                        return st;
                    }
                    auto r = _d.wait(Descriptor::Write);
                    if (r != WaitResult::ready) {
                        return wait_errno(r, _d);
                    }
                }
            }

            async::task<int> _co_connected(async::stop_token stop)  {
                Operation op(_d);
                if (!op) {
                    co_return ECANCELED;
                }
                for (;;) {
                    int st = _connect_state();
                    if (st != EINPROGRESS) {
                        co_return st;
                    }
                    if (stop.stop_requested()) {
                        co_return ECANCELED;
                    }
                    WaitResult r;
                    if (stop.stop_possible()) {   // the wait as a channel, a case of a select beside the stop
                        auto w = _d.begin_wait(Descriptor::Write);
                        r = w.result;
                        if (!w.done) {
                            bool stopped = false;
                            co_await sgcl::async::select(w.channel->on_receive([] {}), stop.on_stop([&] { stopped = true; }));
                            r = _d.end_wait(w);
                            if (stopped) {
                                co_return ECANCELED;
                            }
                        }
                    } else {
                        r = co_await _d.async_wait(Descriptor::Write);
                    }
                    if (r != WaitResult::ready) {
                        co_return wait_errno(r, _d);
                    }
                }
            }

            void set_local(endpoint e) noexcept {
                _local = e;
            }

            // The descriptor, for the setup of a socket not yet shared
            int fd() const noexcept {
                return _d.fd();
            }

        private:
            // One receive without waiting, for the three reads (the blocking,
            // the awaited, the try): the descriptor's state looked at first
            // (a close, the read deadline), then the call. The bytes (0 at
            // the end of the stream), nullopt when the socket would wait,
            // or the error
            expected<optional<size_t>, io::error> _recv_now(const slice<byte>& b, bool deadline = true) {
                for (;;) {
                    if (auto e = _check(Descriptor::Read, "read", deadline)) {
                        return fail(*e);
                    }
                    _d.prepare(Descriptor::Read);
                    ssize_t n = ::recv(_d.fd(), b.data(), b.size(), 0);
                    if (n >= 0) {
                        return optional<size_t>(size_t(n));
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        return fail(system_error(e, "read", describe()));
                    }
                    return optional<size_t>();
                }
            }

            // The part of a write that goes without waiting, from `written`
            // on, for the three writes (the blocking, the awaited, the
            // try): the descriptor's state looked at before every send (a
            // close, the deadline of the direction), then the send. false:
            // everything written; true: the socket would wait; or the error
            expected<bool, io::error> _send_now(const slice<const byte>& data, size_t& written, bool deadline = true) {
                while (written < data.size()) {
                    if (auto e = _check(Descriptor::Write, "write", deadline)) {
                        return fail(*e);
                    }
                    deadline = true;   // a send after a partial one: the clock read as before
                    _d.prepare(Descriptor::Write);
                    ssize_t n = ::send(_d.fd(), data.data() + written, data.size() - written, SendFlags);
                    if (n >= 0) {
                        written += size_t(n);
                        continue;
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        return fail(system_error(e, "write", describe()));
                    }
                    return true;
                }
                return false;
            }

            // 0 connected, EINPROGRESS still connecting, else the error
            int _connect_state() noexcept {
                int err = 0;
                socklen_t len = sizeof(err);
                if (::getsockopt(_d.fd(), SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
                    return errno;
                }
                if (err) {
                    return err;
                }
                SockAddr peer;
                if (::getpeername(_d.fd(), peer.get(), &peer.size) == 0) {
                    return 0;
                }
                return errno == ENOTCONN ? EINPROGRESS : errno;
            }

            // Before a system call: closing, or the deadline passed (the
            // clock read unless `deadline` is false: the wait just before
            // looked at it)
            optional<io::error> _check(int dir, const char* op, bool deadline = true) const {
                if (_d.closing()) {
                    return closed_error(op, describe());
                }
                if (deadline && _d.expired(dir)) {
                    return system_error(ETIMEDOUT, op, describe());
                }
                return nullopt;
            }

            Descriptor _d;
            endpoint _local;
            endpoint _remote;
            string _path;
            bool _tcp;
        };

        // One direction of a pair of ends in memory (Go's net.Pipe): a
        // write hands its bytes over a rendezvous and waits for the reader
        // to say how many it took, so nothing is buffered and nothing is
        // copied twice; the writer's close and the reader's close are
        // channels closed, which every wait of the other side selects on
        struct MemoryPipe {
            async::detail::ChannelState<slice<const byte>> data;   // capacity 0: the writer waits for the reader
            async::detail::ChannelState<size_t> taken;                  // how much of the element the reader took
            async::detail::ChannelState<void> writer_done;              // closed by the writing end: close, close_write
            async::detail::ChannelState<void> reader_done;              // closed by the reading end: close
        };

        class MemoryConn final : public ConnImpl {
        public:
            MemoryConn(tracked_ptr<MemoryPipe> in, tracked_ptr<MemoryPipe> out)
            : _in(std::move(in))
            , _out(std::move(out))
            , _rearm(make_tracked<async::detail::ChannelState<void>>()) {
            }

            expected<size_t, io::error> raw_read(const slice<byte>& b) override {
                if (b.empty()) {
                    return size_t(0);
                }
                for (;;) {
                    auto [deadline, rearm] = _wait_state(Descriptor::Read);
                    if (auto e = _check(Descriptor::Read, deadline, "read")) {
                        return fail(*e);
                    }
                    optional<slice<const byte>> got;
                    Why why = Why::none;
                    if (deadline == time_point()) {
                        (void)sgcl::async::select(_in->data.on_receive([&](optional<slice<const byte>> s) { got = std::move(s); }),
                                     _in->writer_done.on_receive([&] { why = Why::end; }),
                                     _in->reader_done.on_receive([&] { why = Why::closed; }),
                                     rearm->on_receive([&] { why = Why::again; })).wait();
                    } else {
                        (void)sgcl::async::select(_in->data.on_receive([&](optional<slice<const byte>> s) { got = std::move(s); }),
                                     _in->writer_done.on_receive([&] { why = Why::end; }),
                                     _in->reader_done.on_receive([&] { why = Why::closed; }),
                                     rearm->on_receive([&] { why = Why::again; }),
                                     sgcl::async::timeout(deadline, [&] { why = Why::again; })).wait();
                    }
                    if (got) {
                        size_t n = _take(*got, b);
                        (void)_in->taken.send(n).wait();
                        return n;
                    }
                    if (why == Why::end) {
                        return size_t(0);
                    }
                    if (why == Why::closed) {
                        return fail(closed_error("read", describe()));
                    }
                }
            }

            async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> b) override {
                if (b.empty()) {
                    co_return size_t(0);
                }
                for (;;) {
                    auto [deadline, rearm] = _wait_state(Descriptor::Read);
                    if (auto e = _check(Descriptor::Read, deadline, "read")) {
                        co_return fail(*e);
                    }
                    optional<slice<const byte>> got;
                    Why why = Why::none;
                    if (deadline == time_point()) {
                        co_await sgcl::async::select(_in->data.on_receive([&](optional<slice<const byte>> s) { got = std::move(s); }),
                                                    _in->writer_done.on_receive([&] { why = Why::end; }),
                                                    _in->reader_done.on_receive([&] { why = Why::closed; }),
                                                    rearm->on_receive([&] { why = Why::again; }));
                    } else {
                        co_await sgcl::async::select(_in->data.on_receive([&](optional<slice<const byte>> s) { got = std::move(s); }),
                                                    _in->writer_done.on_receive([&] { why = Why::end; }),
                                                    _in->reader_done.on_receive([&] { why = Why::closed; }),
                                                    rearm->on_receive([&] { why = Why::again; }),
                                                    sgcl::async::timeout(deadline, [&] { why = Why::again; }));
                    }
                    if (got) {
                        size_t n = _take(*got, b);
                        co_await _in->taken.send(n);
                        co_return n;
                    }
                    if (why == Why::end) {
                        co_return size_t(0);
                    }
                    if (why == Why::closed) {
                        co_return fail(closed_error("read", describe()));
                    }
                }
            }

            expected<size_t, io::error> raw_write(const slice<const byte>& data) override {
                size_t written = 0;
                while (written < data.size()) {
                    auto [deadline, rearm] = _wait_state(Descriptor::Write);
                    if (auto e = _check_write(deadline)) {
                        return fail(*e);
                    }
                    bool sent = false;
                    Why why = Why::none;
                    auto rest = data.subspan(written);
                    if (deadline == time_point()) {
                        (void)sgcl::async::select(_out->data.on_send(rest, [&] { sent = true; }),
                                     _out->writer_done.on_receive([&] { why = Why::closed; }),
                                     _out->reader_done.on_receive([&] { why = Why::gone; }),
                                     rearm->on_receive([&] { why = Why::again; })).wait();
                    } else {
                        (void)sgcl::async::select(_out->data.on_send(rest, [&] { sent = true; }),
                                     _out->writer_done.on_receive([&] { why = Why::closed; }),
                                     _out->reader_done.on_receive([&] { why = Why::gone; }),
                                     rearm->on_receive([&] { why = Why::again; }),
                                     sgcl::async::timeout(deadline, [&] { why = Why::again; })).wait();
                    }
                    if (sent) {
                        auto n = _out->taken.receive().wait();   // the reader says how much it took, always
                        written += n ? *n : 0;
                    }
                }
                return written;
            }

            async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) override {
                size_t written = 0;
                while (written < data.size()) {
                    auto [deadline, rearm] = _wait_state(Descriptor::Write);
                    if (auto e = _check_write(deadline)) {
                        co_return fail(*e);
                    }
                    bool sent = false;
                    Why why = Why::none;
                    auto rest = data.subspan(written);
                    if (deadline == time_point()) {
                        co_await sgcl::async::select(_out->data.on_send(rest, [&] { sent = true; }),
                                                    _out->writer_done.on_receive([&] { why = Why::closed; }),
                                                    _out->reader_done.on_receive([&] { why = Why::gone; }),
                                                    rearm->on_receive([&] { why = Why::again; }));
                    } else {
                        co_await sgcl::async::select(_out->data.on_send(rest, [&] { sent = true; }),
                                                    _out->writer_done.on_receive([&] { why = Why::closed; }),
                                                    _out->reader_done.on_receive([&] { why = Why::gone; }),
                                                    rearm->on_receive([&] { why = Why::again; }),
                                                    sgcl::async::timeout(deadline, [&] { why = Why::again; }));
                    }
                    if (sent) {
                        auto n = co_await _out->taken.receive();
                        written += n ? *n : 0;
                    }
                }
                co_return written;
            }

            // Both directions ended: the peer reads the end, and a peer's
            // write fails; a wait of this end in progress ends
            expected<void, io::error> close() override {
                if (_closed.exchange(true, std::memory_order_acq_rel)) {
                    return {};
                }
                _in->reader_done.close();
                _out->writer_done.close();
                _wake();
                return {};
            }

            bool is_closed() const noexcept override {
                return _closed.load(std::memory_order_acquire);
            }

            expected<void, io::error> close_write() override {
                if (is_closed()) {
                    return fail(closed_error("close_write", describe()));
                }
                _out->writer_done.close();
                return {};
            }

            void set_deadline(int dir, time_point t) override {
                {
                    std::lock_guard lock(_m);
                    _deadline[dir] = t;
                }
                _wake();
            }

            string describe() const override {
                return string("pipe");
            }

        private:
            enum class Why {
                none,
                end,      // the writer is done: the end of the stream
                closed,   // this end was closed
                gone,     // the reader is gone: a write fails
                again     // a deadline changed or passed: look again
            };

            struct WaitState {
                time_point deadline;
                tracked_ptr<async::detail::ChannelState<void>> rearm;
            };

            WaitState _wait_state(int dir) {
                std::lock_guard lock(_m);
                return WaitState{_deadline[dir], _rearm};
            }

            // The waits in progress woken to look at their state again: the
            // channel they select on closed, a new one for the next waits
            void _wake() {
                tracked_ptr<async::detail::ChannelState<void>> old;
                {
                    std::lock_guard lock(_m);
                    old = _rearm;
                    _rearm = make_tracked<async::detail::ChannelState<void>>();
                }
                old->close();
            }

            optional<io::error> _check(int, time_point deadline, const char* op) const {
                if (is_closed()) {
                    return closed_error(op, describe());
                }
                if (deadline != time_point() && sgcl::clock::now() >= deadline) {
                    return system_error(ETIMEDOUT, op, describe());
                }
                return nullopt;
            }

            optional<io::error> _check_write(time_point deadline) const {
                if (auto e = _check(Descriptor::Write, deadline, "write")) {
                    return e;
                }
                if (_out->writer_done.closed()) {
                    return closed_error("write", describe());   // close_write
                }
                if (_out->reader_done.closed()) {
                    return system_error(EPIPE, "write", describe());
                }
                return nullopt;
            }

            static size_t _take(const slice<const byte>& from, const slice<byte>& to) noexcept {
                size_t n = std::min(from.size(), to.size());
                std::memcpy(to.data(), from.data(), n);
                return n;
            }

            tracked_ptr<MemoryPipe> _in;
            tracked_ptr<MemoryPipe> _out;
            std::mutex _m;                               // the deadlines and the rearm channel
            time_point _deadline[2] = {};
            tracked_ptr<async::detail::ChannelState<void>> _rearm;
            std::atomic<bool> _closed = {false};
        };

        class ListenerImpl;
        class UdpImpl;
    }

    class connection {
    public:
        connection() noexcept = default;   // no connection; an operation on it is a contract violation

        // A connection over the transport given: for the module's own
        // (the sockets, the pair in memory; TLS in the next stage)
        explicit connection(const tracked_ptr<detail::ConnImpl>& impl) noexcept
        : _impl(impl) {
        }

        // At most buffer.size() bytes, as many as have come (at least one);
        // 0 at the end of the stream
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        expected<size_t, io::error> read(const slice<byte>& buffer) const {
            return _block_read(buffer);
        }

        async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const {
            return _co_read(buffer);
        }

        // The whole buffer, or io::errc::unexpected_eof when the stream
        // ends inside it (0 when it ended before the first byte)
        // `read_full(...)` on this thread, `co_await async_read_full(...)` in a task
        expected<size_t, io::error> read_full(const slice<byte>& buffer) const {
            return _get().read_full(buffer);
        }

        async::task<expected<size_t, io::error>> async_read_full(const slice<byte>& buffer) const {
            return _get().async_read_full(buffer);
        }

        // Everything to the end of the stream
        // `read_all(...)` on this thread, `co_await async_read_all(...)` in a task
        expected<vector<byte>, io::error> read_all() const {
            return _get().read_all();
        }

        async::task<expected<vector<byte>, io::error>> async_read_all() const {
            return _get().async_read_all();
        }

        // `read_all_text(...)` on this thread, `co_await async_read_all_text(...)` in a task
        expected<string, io::error> read_all_text() const {
            return _get().read_all_text();
        }

        async::task<expected<string, io::error>> async_read_all_text() const {
            return _get().async_read_all_text();
        }

        // The next line without its "\n" and "\r\n"; nullopt at the end.
        // The first call puts a buffer of 8 KB in front of the connection,
        // which read() takes from first from then on. A line longer than
        // set_max_line (64 KB by default: the input is the network's) is
        // io::errc::line_too_long.
        // `c.read_line()` on this thread, `co_await c.async_read_line()` in a task
        expected<optional<string>, io::error> read_line() const {
            return _block_read_line();
        }

        async::task<expected<optional<string>, io::error>> async_read_line() const {
            return _co_read_line();
        }

        void set_max_line(size_t bytes) const noexcept {
            _get().set_max_line(bytes);
        }

        // Everything, or the error
        // `write(...)` on this thread, `co_await async_write(...)` in a task
        expected<size_t, io::error> write(const slice<const byte>& data) const {
            return _block_write(data);
        }

        async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const {
            return _co_write(data);
        }

        // The text's bytes; the async form holds the string for as long as it runs
        // `write(...)` on this thread, `co_await async_write(...)` in a task
        expected<size_t, io::error> write(const string& text) const {
            return _block_write(text);
        }

        async::task<expected<size_t, io::error>> async_write(const string& text) const {
            return _co_write(text);
        }

        // A literal, a character array, a std::string_view: as a string
        // (an exact match, else the conversions to a string and to bytes
        // tie); the async form copies the text, which the task then holds
        template<sgcl::detail::TextArgument T>
        expected<size_t, io::error> write(const T& text) const {
            return _block_write(slice<const byte>(text));
        }

        template<sgcl::detail::TextArgument T>
        async::task<expected<size_t, io::error>> async_write(const T& text) const {
            return _co_write(string(slice<const byte>(text)));
        }

        // Everything to the end of this stream, written to other (an echo
        // is c.copy_to(c), a proxy two of them): the bytes copied
        // `copy_to(...)` on this thread, `co_await async_copy_to(...)` in a task
        expected<size_t, io::error> copy_to(const connection& other) const {
            return _get().copy_to(other._get());
        }

        async::task<expected<size_t, io::error>> async_copy_to(const connection& other) const {
            return _get().async_copy_to(other._get());
        }

        // The connection ended now, both ways: the reads, writes and
        // waits in progress in other tasks end with io::errc::closed; a
        // second close does nothing. The descriptor goes back to the
        // system when the last operation in progress has let go of it.
        // `close()` on this thread, `co_await async_close()` in a task (a
        // socket's close never waits; a transport over one, TLS, sends its
        // closing record first)
        expected<void, io::error> close() const {
            return _get().close();
        }

        async::task<expected<void, io::error>> async_close() const {
            return _close(_impl);
        }

        // The writing half ended (shutdown(SHUT_WR)): the peer reads the
        // end of the stream, and this side can still read
        expected<void, io::error> close_write() const {
            return _get().close_write();
        }

        bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The addresses of the two ends; empty for a unix socket and for
        // the pair in memory
        endpoint local_endpoint() const {
            return _get().local_endpoint();
        }

        endpoint remote_endpoint() const {
            return _get().remote_endpoint();
        }

        // The path of a unix socket, empty for anything else
        string path() const {
            return _get().path();
        }

        // Absolute deadlines on the module's clock: a read (write) that
        // starts after the deadline of its direction, or would wait past
        // it, fails with ETIMEDOUT; time_point() removes it. A change
        // applies to the operations in progress too. Where a timeout per
        // operation is wanted, it is c.set_read_deadline(clock::now() + d)
        // before each; a timeout() around a read is not the same (the
        // read goes on after the race is lost, and takes the data).
        void set_deadline(time_point t) const {
            _get().set_deadline(detail::Descriptor::Read, t);
            _get().set_deadline(detail::Descriptor::Write, t);
        }

        void set_read_deadline(time_point t) const {
            _get().set_deadline(detail::Descriptor::Read, t);
        }

        void set_write_deadline(time_point t) const {
            _get().set_deadline(detail::Descriptor::Write, t);
        }

        // TCP only (EOPNOTSUPP for anything else): Nagle's algorithm off
        // (the default, as in Go) or on; keep-alive probes after `idle`
        // of silence (15 s by default, as in Go), zero turns them off
        expected<void, io::error> set_no_delay(bool on) const {
            return _get().set_no_delay(on);
        }

        expected<void, io::error> set_keep_alive(duration idle) const {
            return _get().set_keep_alive(std::chrono::nanoseconds(idle));
        }

        // Two connected ends in memory (Go's net.Pipe): what one writes the
        // other reads, a write waiting for the reads that take it, nothing
        // buffered; deadlines, close and close_write as on a socket. For
        // tests without sockets.
        static pair<connection, connection> in_memory() {
            tracked_ptr<detail::MemoryPipe> a = make_tracked<detail::MemoryPipe>();
            tracked_ptr<detail::MemoryPipe> b = make_tracked<detail::MemoryPipe>();
            return pair<connection, connection>(connection(tracked_ptr<detail::ConnImpl>(make_tracked<detail::MemoryConn>(a, b))),
                                    connection(tracked_ptr<detail::ConnImpl>(make_tracked<detail::MemoryConn>(b, a))));
        }

        // Whether this handle holds a connection
        explicit operator bool() const noexcept {
            return (bool)_impl;
        }

        // The same connection
        friend bool operator==(const connection& a, const connection& b) noexcept {
            return a._impl == b._impl;
        }

    private:
        friend struct detail::ConnectionAccess;
        friend struct io::detail::HandleAccess;

        // What an io::reader or io::writer made of the handle binds: the
        // connection itself, not the handle, which may go first
        const tracked_ptr<detail::ConnImpl>& _stream_state() const noexcept {
            return _impl;
        }

        static async::task<expected<void, io::error>> _close(tracked_ptr<detail::ConnImpl> c) {
            co_return co_await c->async_close();
        }

        detail::ConnImpl& _get() const noexcept {
            assert(_impl && "an empty net::connection");
            return *_impl;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        connection(sgcl::detail::FromWord, const tracked_ptr<detail::ConnImpl>& w) noexcept
        : _impl(w) {
        }

        tracked_ptr<detail::ConnImpl>& _handle_word() noexcept {
            return _impl;
        }

        const tracked_ptr<detail::ConnImpl>& _handle_word() const noexcept {
            return _impl;
        }

        tracked_ptr<detail::ConnImpl> _impl;

        // the two halves of the operations above: a thread's and a task's
        expected<size_t, io::error> _block_read(const slice<byte>& buffer) const {
            return _get().read(buffer);
        }

        async::task<expected<size_t, io::error>> _co_read(const slice<byte>& buffer) const {
            return _get().async_read(buffer);
        }

        expected<optional<string>, io::error> _block_read_line() const {
            return _get()._block_read_line();
        }

        async::task<expected<optional<string>, io::error>> _co_read_line() const {
            return _get()._co_read_line();
        }

        expected<size_t, io::error> _block_write(const slice<const byte>& data) const {
            return _get().write(data);
        }

        async::task<expected<size_t, io::error>> _co_write(const slice<const byte>& data) const {
            return _get().async_write(data);
        }

        expected<size_t, io::error> _block_write(const string& text) const {
            return _get().write(as_bytes(text.as_slice()));
        }

        async::task<expected<size_t, io::error>> _co_write(const string& text) const {
            return _get().async_write(as_bytes(text.as_slice()));
        }
    };

    namespace detail {
        inline ConnImpl& ConnectionAccess::impl(const connection& c) noexcept {
            return c._get();
        }
    }
}

namespace sgcl::io::detail {
    // A connection is a stream handle (io/stream.h): a stream made of one
    // binds its ConnImpl, as one made of an io::file binds the file's state
    template<>
    inline constexpr bool IsStreamHandle<net::connection> = true;
}

namespace sgcl::net {

    namespace detail {
        // A listening socket, TCP or unix
        class ListenerImpl {
        public:
            ListenerImpl(int fd, bool tcp, endpoint local, const string& path, bool unlink_on_close)
            : _d(fd)
            , _local(local)
            , _path(path)
            , _tcp(tcp)
            , _unlink(unlink_on_close) {
            }

            virtual ~ListenerImpl() = default;

            // `co_await c.async_accept()` in a task, `c.accept().wait()` on a thread
            expected<connection, io::error> accept() {
                return _block_accept();
            }

            async::task<expected<connection, io::error>> async_accept() {
                return _co_accept();
            }

            virtual expected<connection, io::error> _block_accept()  {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("accept", describe()));
                }
                std::chrono::nanoseconds pause(0);
                for (;;) {
                    if (_d.closing()) {
                        return fail(closed_error("accept", describe()));
                    }
                    SockAddr from;
                    _d.prepare(Descriptor::Read);
                    int s = ::accept(_d.fd(), from.get(), &from.size);
                    if (s >= 0) {
                        return _accepted(s, from);
                    }
                    int e = errno;
                    if (e == EINTR || e == ECONNABORTED) {
                        continue;
                    }
                    if (e == EAGAIN || e == EWOULDBLOCK) {
                        auto r = _d.wait(Descriptor::Read);
                        if (r != WaitResult::ready) {
                            return fail(wait_error(r, _d, "accept", describe()));
                        }
                        continue;
                    }
                    if (_exhausted(e)) {
                        pause = _next_pause(pause);
                        (void)sgcl::async::select(_closed.on_receive([] {}), sgcl::async::timeout(sgcl::clock::now() + pause, [] {})).wait();
                        continue;
                    }
                    return fail(system_error(e, "accept", describe()));
                }
            }

            virtual async::task<expected<connection, io::error>> _co_accept()  {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("accept", describe()));
                }
                std::chrono::nanoseconds pause(0);
                for (;;) {
                    if (_d.closing()) {
                        co_return fail(closed_error("accept", describe()));
                    }
                    SockAddr from;
                    _d.prepare(Descriptor::Read);
                    int s = ::accept(_d.fd(), from.get(), &from.size);
                    if (s >= 0) {
                        co_return _accepted(s, from);
                    }
                    int e = errno;
                    if (e == EINTR || e == ECONNABORTED) {
                        continue;
                    }
                    if (e == EAGAIN || e == EWOULDBLOCK) {
                        auto r = co_await _d.async_wait(Descriptor::Read);
                        if (r != WaitResult::ready) {
                            co_return fail(wait_error(r, _d, "accept", describe()));
                        }
                        continue;
                    }
                    if (_exhausted(e)) {
                        pause = _next_pause(pause);
                        co_await sgcl::async::select(_closed.on_receive([] {}), sgcl::async::timeout(sgcl::clock::now() + pause, [] {}));
                        continue;
                    }
                    co_return fail(system_error(e, "accept", describe()));
                }
            }

            // The descriptor first, then the channel of the pause: an accept
            // woken from its pause finds the closing bit set
            virtual expected<void, io::error> close() {
                int e = _d.close();
                _closed.close();
                if (_unlink && !_unlinked.exchange(true, std::memory_order_acq_rel)) {
                    ::unlink(_path.c_str());
                }
                if (e) {
                    return fail(system_error(e, "close", describe()));
                }
                return {};
            }

            virtual bool is_closed() const noexcept {
                return _d.closing();
            }

            virtual endpoint local_endpoint() const noexcept {
                return _local;
            }

            virtual string path() const {
                return _path;
            }

            virtual string describe() const {
                return _tcp ? string("tcp ") + _local.to_string() : string("unix ") + _path;
            }

        protected:
            // A listener over another (TLS's): no socket of its own
            ListenerImpl() noexcept
            : _d(-1, false)
            , _tcp(false)
            , _unlink(false) {
            }

        private:
            // The descriptors (or the kernel's memory) run out: the
            // connection waits in the backlog, the listener stays readable,
            // and an accept in a loop would spin; so a pause, doubled from
            // 5 ms to a second, as Go's server takes
            static bool _exhausted(int e) noexcept {
                return e == EMFILE || e == ENFILE || e == ENOBUFS || e == ENOMEM;
            }

            static std::chrono::nanoseconds _next_pause(std::chrono::nanoseconds p) noexcept {
                using namespace std::chrono_literals;
                if (p == std::chrono::nanoseconds::zero()) {
                    return 5ms;
                }
                return std::min<std::chrono::nanoseconds>(p * 2, 1s);
            }

            expected<connection, io::error> _accepted(int s, SockAddr& from) {
                if (!prepare_socket(s)) {
                    int e = errno;
                    ::close(s);
                    return fail(system_error(e, "accept", describe()));
                }
                endpoint local, remote;
                if (_tcp) {
                    tune_tcp(s);
                    remote = from_sockaddr(from.get());
                    local = local_of(s);
                }
                return connection(tracked_ptr<ConnImpl>(make_tracked<SocketConn>(s, _tcp, local, remote, _path)));
            }

            Descriptor _d;
            async::detail::ChannelState<void> _closed;   // closed by close(): ends the pause after EMFILE
            endpoint _local;
            string _path;
            bool _tcp;
            bool _unlink;
            std::atomic<bool> _unlinked = {false};
        };
    }

    // A listening socket (tcp::listen, unix_domain::listen): the
    // connections it accepts
    class listener {
    public:
        listener() noexcept = default;

        explicit listener(const tracked_ptr<detail::ListenerImpl>& impl) noexcept
        : _impl(impl) {
        }

        // The next connection. A connection aborted before it was taken
        // (ECONNABORTED) is skipped; when the descriptors run out (EMFILE)
        // the accept waits, 5 ms and doubling to a second, and tries again
        // rather than spin. It fails with io::errc::closed after close()
        // (from any task: an accept in progress ends), or on an error that
        // will not pass.
        // `x.accept(...)` on this thread, `co_await x.async_accept(...)` in a task
        expected<connection, io::error> accept() const {
            return _block_accept();
        }

        async::task<expected<connection, io::error>> async_accept() const {
            return _co_accept();
        }

        // No more connections; the accepts in progress end. A unix
        // listener removes its socket's file.
        expected<void, io::error> close() const {
            return _get().close();
        }

        bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The address it listens on: ":0" given, the port the system chose
        endpoint local_endpoint() const {
            return _get().local_endpoint();
        }

        // The path of a unix listener, empty for TCP
        string path() const {
            return _get().path();
        }

        explicit operator bool() const noexcept {
            return (bool)_impl;
        }

        friend bool operator==(const listener& a, const listener& b) noexcept {
            return a._impl == b._impl;
        }

    private:
        detail::ListenerImpl& _get() const noexcept {
            assert(_impl && "an empty net::listener");
            return *_impl;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        listener(sgcl::detail::FromWord, const tracked_ptr<detail::ListenerImpl>& w) noexcept
        : _impl(w) {
        }

        tracked_ptr<detail::ListenerImpl>& _handle_word() noexcept {
            return _impl;
        }

        const tracked_ptr<detail::ListenerImpl>& _handle_word() const noexcept {
            return _impl;
        }

        tracked_ptr<detail::ListenerImpl> _impl;

        // the two halves of the operations above: a thread's and a task's
        expected<connection, io::error> _block_accept() const {
            return _get()._block_accept();
        }

        async::task<expected<connection, io::error>> _co_accept() const {
            return _get()._co_accept();
        }
    };

    // A Datagram received: how many bytes the buffer got, from whom, and
    // whether the Datagram was longer than the buffer and cut (MSG_TRUNC)
    namespace detail {
        struct Datagram {
            size_t size = 0;
            endpoint from;
            bool truncated = false;
        };
    }

    namespace detail {
        class UdpImpl {
        public:
            UdpImpl(int fd, int family, endpoint local, endpoint remote) noexcept
            : _d(fd)
            , _local(local)
            , _remote(remote)
            , _family(family) {
            }

            // One datagram: its size in the buffer and its sender
            // `receive(...)` on this thread, `co_await async_receive(...)` in a task
            expected<Datagram, io::error> receive(const slice<byte>& b) {
                return _block_receive(b);
            }

            async::task<expected<Datagram, io::error>> async_receive(const slice<byte>& b) {
                return _co_receive(b);
            }

            expected<Datagram, io::error> _block_receive(const slice<byte>& b)  {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("read", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Read, "read")) {
                        return fail(*e);
                    }
                    Datagram d;
                    int e = _recv(b, d);
                    if (e == 0) {
                        return d;
                    }
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        return fail(system_error(e, "read", describe()));
                    }
                    auto r = _d.wait(Descriptor::Read);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "read", describe()));
                    }
                }
            }

            async::task<expected<Datagram, io::error>> _co_receive(slice<byte> b)  {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("read", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Read, "read")) {
                        co_return fail(*e);
                    }
                    Datagram d;
                    int e = _recv(b, d);
                    if (e == 0) {
                        co_return d;
                    }
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        co_return fail(system_error(e, "read", describe()));
                    }
                    auto r = co_await _d.async_wait(Descriptor::Read);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "read", describe()));
                    }
                }
            }

            // One Datagram to `to`, or to the connected peer when to is empty
            expected<size_t, io::error> send(const slice<const byte>& data, const endpoint& to) {
                SockAddr sa;
                if (to.is_valid() && !to_sockaddr(to, _family, sa)) {
                    return fail(system_error(EAFNOSUPPORT, "write", to.to_string()));
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("write", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Write, "write")) {
                        return fail(*e);
                    }
                    _d.prepare(Descriptor::Write);
                    ssize_t n = to.is_valid() ? ::sendto(_d.fd(), data.data(), data.size(), SendFlags, sa.get(), sa.size) : ::send(_d.fd(), data.data(), data.size(), SendFlags);
                    if (n >= 0) {
                        return size_t(n);
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {   // ENOBUFS an error, as in Go: the socket has room, so a wait for writability would spin
                        return fail(system_error(e, "write", describe()));
                    }
                    auto r = _d.wait(Descriptor::Write);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "write", describe()));
                    }
                }
            }

            async::task<expected<size_t, io::error>> _co_send(slice<const byte> data, endpoint to)  {
                SockAddr sa;
                if (to.is_valid() && !to_sockaddr(to, _family, sa)) {
                    co_return fail(system_error(EAFNOSUPPORT, "write", to.to_string()));
                }
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("write", describe()));
                }
                for (;;) {
                    if (auto e = _check(Descriptor::Write, "write")) {
                        co_return fail(*e);
                    }
                    _d.prepare(Descriptor::Write);
                    ssize_t n = to.is_valid() ? ::sendto(_d.fd(), data.data(), data.size(), SendFlags, sa.get(), sa.size) : ::send(_d.fd(), data.data(), data.size(), SendFlags);
                    if (n >= 0) {
                        co_return size_t(n);
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {   // ENOBUFS an error, as in Go: the socket has room, so a wait for writability would spin
                        co_return fail(system_error(e, "write", describe()));
                    }
                    auto r = co_await _d.async_wait(Descriptor::Write);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "write", describe()));
                    }
                }
            }

            expected<void, io::error> close() {
                int e = _d.close();
                if (e) {
                    return fail(system_error(e, "close", describe()));
                }
                return {};
            }

            bool is_closed() const noexcept {
                return _d.closing();
            }

            void set_deadline(int dir, time_point t) {
                _d.set_deadline(dir, t);
            }

            endpoint local_endpoint() const noexcept {
                return _local;
            }

            endpoint remote_endpoint() const noexcept {
                return _remote;
            }

            string describe() const {
                return _remote.is_valid() ? string("udp ") + _local.to_string() + "->" + _remote.to_string() : string("udp ") + _local.to_string();
            }

        private:
            // recvmsg, for the flag that says the Datagram was cut: 0 or
            // errno. An empty buffer reads into a byte of its own: macOS
            // answers an empty one with 0 and leaves the Datagram queued, a
            // Datagram of nothing that is not there; so the Datagram is
            // taken, its size in the buffer 0, truncated when it had bytes
            int _recv(const slice<byte>& b, Datagram& d) {
                SockAddr from;
                byte spare[1];
                iovec iov;
                iov.iov_base = b.empty() ? spare : b.data();
                iov.iov_len = b.empty() ? 1 : b.size();
                msghdr m = {};
                m.msg_name = &from.storage;
                m.msg_namelen = sizeof(from.storage);
                m.msg_iov = &iov;
                m.msg_iovlen = 1;
                _d.prepare(Descriptor::Read);
                ssize_t n = ::recvmsg(_d.fd(), &m, 0);
                if (n < 0) {
                    return errno;
                }
                d.size = b.empty() ? 0 : size_t(n);
                d.truncated = (m.msg_flags & MSG_TRUNC) != 0 || (b.empty() && n > 0);
                d.from = m.msg_namelen ? from_sockaddr(from.get()) : _remote;
                return 0;
            }

            optional<io::error> _check(int dir, const char* op) const {
                if (_d.closing()) {
                    return closed_error(op, describe());
                }
                if (_d.expired(dir)) {
                    return system_error(ETIMEDOUT, op, describe());
                }
                return nullopt;
            }

            Descriptor _d;
            endpoint _local;
            endpoint _remote;
            int _family;
        };
    }

    // A UDP socket (udp::bind, udp::connect): datagrams to and from any
    // address, or, connected, to and from one. A Datagram is sent whole or
    // not at all; one longer than the buffer is cut, and says so.
    namespace detail {
        class UdpSocket {
        public:
            UdpSocket() noexcept = default;

            explicit UdpSocket(const tracked_ptr<detail::UdpImpl>& impl) noexcept
            : _impl(impl) {
            }

            // The next Datagram into the buffer: its size, its sender, and
            // whether it was cut to fit
            // `receive_from(...)` on this thread, `co_await async_receive_from(...)` in a task
            expected<Datagram, io::error> receive_from(const slice<byte>& buffer) const {
                return _block_receive_from(buffer);
            }

            async::task<expected<Datagram, io::error>> async_receive_from(const slice<byte>& buffer) const {
                return _co_receive_from(buffer);
            }

            // `send_to(...)` on this thread, `co_await async_send_to(...)` in a task
            expected<size_t, io::error> send_to(const slice<const byte>& data, const endpoint& to) const {
                return _block_send_to(data, to);
            }

            async::task<expected<size_t, io::error>> async_send_to(const slice<const byte>& data, const endpoint& to) const {
                return _co_send_to(data, to);
            }

            // A socket from udp::connect: to and from its one peer
            // `receive(...)` on this thread, `co_await async_receive(...)` in a task
            expected<size_t, io::error> receive(const slice<byte>& buffer) const {
                return _block_receive(buffer);
            }

            async::task<expected<size_t, io::error>> async_receive(const slice<byte>& buffer) const {
                return _co_receive(buffer);
            }

            // `send(...)` on this thread, `co_await async_send(...)` in a task
            expected<size_t, io::error> send(const slice<const byte>& data) const {
                return _block_send(data);
            }

            async::task<expected<size_t, io::error>> async_send(const slice<const byte>& data) const {
                return _co_send(data);
            }

            expected<void, io::error> close() const {
                return _get().close();
            }

            bool is_closed() const noexcept {
                return _get().is_closed();
            }

            endpoint local_endpoint() const {
                return _get().local_endpoint();
            }

            // The peer of a connected socket, empty otherwise
            endpoint remote_endpoint() const {
                return _get().remote_endpoint();
            }

            void set_deadline(time_point t) const {
                _get().set_deadline(detail::Descriptor::Read, t);
                _get().set_deadline(detail::Descriptor::Write, t);
            }

            void set_read_deadline(time_point t) const {
                _get().set_deadline(detail::Descriptor::Read, t);
            }

            void set_write_deadline(time_point t) const {
                _get().set_deadline(detail::Descriptor::Write, t);
            }

            explicit operator bool() const noexcept {
                return (bool)_impl;
            }

            friend bool operator==(const UdpSocket& a, const UdpSocket& b) noexcept {
                return a._impl == b._impl;
            }

        private:
            detail::UdpImpl& _get() const noexcept {
                assert(_impl && "an empty net::udp::socket");
                return *_impl;
            }

            static async::task<expected<size_t, io::error>> _receive_size(tracked_ptr<detail::UdpImpl> impl, slice<byte> buffer) {
                auto d = co_await impl->async_receive(buffer);
                if (!d) {
                    co_return detail::fail(d);
                }
                co_return d->size;
            }

            tracked_ptr<detail::UdpImpl> _impl;

            // the two halves of the operations above: a thread's and a task's
            expected<Datagram, io::error> _block_receive_from(const slice<byte>& buffer) const {
                return _get()._block_receive(buffer);
            }

            async::task<expected<Datagram, io::error>> _co_receive_from(const slice<byte>& buffer) const {
                return _get()._co_receive(buffer);
            }

            expected<size_t, io::error> _block_send_to(const slice<const byte>& data, const endpoint& to) const {
                return _get().send(data, to);
            }

            async::task<expected<size_t, io::error>> _co_send_to(const slice<const byte>& data, const endpoint& to) const {
                return _get()._co_send(data, to);
            }

            expected<size_t, io::error> _block_receive(const slice<byte>& buffer) const {
                auto d = _get()._block_receive(buffer);
                if (!d) {
                    return detail::fail(d);
                }
                return d->size;
            }

            async::task<expected<size_t, io::error>> _co_receive(const slice<byte>& buffer) const {
                return _receive_size(_impl, buffer);
            }

            expected<size_t, io::error> _block_send(const slice<const byte>& data) const {
                return _get().send(data, endpoint());
            }

            async::task<expected<size_t, io::error>> _co_send(const slice<const byte>& data) const {
                return _get()._co_send(data, endpoint());
            }
        };
    }
}
