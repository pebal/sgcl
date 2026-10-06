//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
#include "../io/file.h"
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
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>
#if defined(__linux__)
#include <csignal>
#include <pthread.h>
#include <sys/sendfile.h>
#endif

namespace sgcl::net {
    // The connections of the module: a stream of bytes both ways (a TCP or
    // unix socket, a pair of ends in memory, a TLS session over either:
    // tls.h) and a listener that accepts them; a UDP socket, udp::socket,
    // is socket.h's. Each is a handle of one word, a tracked_ptr to the
    // object inside: a copy is the same connection (as a *net.TCPConn in
    // Go is), and a handle passed by value into a task keeps the
    // connection alive for as long as the task runs.
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

        inline io::error closed_error(const char* op, const string& what) noexcept {
            return io::error(io::errc::closed, op, what);
        }

        // A wait that did not end in readiness, as the operation's error
        inline io::error wait_error(WaitResult r, const Descriptor& d, const char* op, const string& what) noexcept {
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
        SGCL_INLINE_HOT int wait_errno(WaitResult r, const Descriptor& d) noexcept {
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
        SGCL_INLINE_HOT void tune_tcp(int fd) noexcept {
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

            SGCL_INLINE_HOT readiness(Descriptor& d, const ConnImpl& c) noexcept
            : _d(&d)
            , _c(&c) {
            }

            SGCL_INLINE_HOT bool supported() const noexcept {
                return _d != nullptr;
            }

            SGCL_INLINE_HOT bool await_ready() {
                _op.emplace(_d->async_wait(Descriptor::Read));
                return _op->await_ready();
            }

            template<class P>
            SGCL_INLINE_HOT bool await_suspend(std::coroutine_handle<P> h) {
                return _op->await_suspend(h);
            }

            expected<void, io::error> await_resume() noexcept;   // below ConnImpl

        private:
            Descriptor* _d = nullptr;
            const ConnImpl* _c = nullptr;
            optional<Descriptor::wait_op> _op;
        };

        // The implementation under a connection handle, for the modules
        // built on net (http: a write begun without a frame), and the
        // handle made over one: a connection is made over a transport only
        // here (the sockets, the pair in memory, TLS), never by a public
        // constructor
        struct ConnectionAccess {
            static ConnImpl& impl(const connection& c) noexcept;
            static connection make(const tracked_ptr<ConnImpl>& impl) noexcept;
        };

        // The raw side of a connection, under the buffer read_line puts in
        // front of it
        class ConnRawReader final {
        public:
            SGCL_INLINE_HOT explicit ConnRawReader(const tracked_ptr<ConnImpl>& c) noexcept
            : _c(c) {
            }

            expected<size_t, io::error> read(const slice<byte>& buffer);
            async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) noexcept;

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

            SGCL_INLINE_HOT expected<size_t, io::error> read(const slice<byte>& buffer) {
                if (buffer.empty()) {
                    return size_t(0);
                }
                std::lock_guard<sgcl::async::mutex> guard(_read_lock);
                return _buffered ? _buffered->read(buffer) : raw_read(buffer);
            }

            async::task<expected<size_t, io::error>> async_read(slice<byte> buffer) noexcept {
                if (buffer.empty()) {
                    co_return size_t(0);
                }
                auto guard = co_await _read_lock.scoped_lock();
                if (_buffered) {
                    co_return co_await _buffered->async_read(buffer);
                }
                co_return co_await awaited_raw_read(buffer);
            }

            SGCL_INLINE_HOT expected<size_t, io::error> write(const slice<const byte>& data) {
                std::lock_guard<sgcl::async::mutex> guard(_write_lock);
                return raw_write(data);
            }

            async::task<expected<size_t, io::error>> async_write(slice<const byte> data) noexcept {
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

            // The same over several pieces in order, as one write: a
            // response's head and the blocks of its body, which a socket
            // takes in one sendmsg and TLS seals into its records straight
            // from the blocks, with no copy of the body into one buffer
            // first. The pieces are held by `parts` (a slice keeps its
            // block) for as long as the rest of the write needs them
            started_write start_write_parts(const vector<slice<const byte>>& parts) {
                started_write s;
                size_t total = 0;
                for (auto& p : parts) {
                    total += p.size();
                }
                if (total == 0) {
                    return s;
                }
                if (!_write_lock.try_lock()) {
                    s.rest.emplace(_write_rest_parts(tracked_ptr<ConnImpl>(this), parts, 0, total, nullopt));
                    return s;
                }
                auto r = try_raw_write_parts(parts.data(), parts.size());
                if (!r || *r == total) {
                    _write_lock.unlock();
                    s.done = std::move(r);
                    return s;
                }
                s.rest.emplace(_write_rest_parts(tracked_ptr<ConnImpl>(this), parts, *r, total, sgcl::async::mutex::guard(_write_lock)));
                return s;
            }

            // The n bytes of the file `fd` from `offset` (read by pread: the
            // file's position is the caller's), as one write under the
            // write lock: the transport's own way (a socket: sendfile, no
            // copy through the process), else blocks read and written as
            // the pieces of one write (TLS seals its records from them).
            // The bytes sent: fewer than n when the file ended first
            SGCL_INLINE_HOT expected<size_t, io::error> send_file(int fd, uint64_t offset, uint64_t n) {
                std::lock_guard<sgcl::async::mutex> guard(_write_lock);
                return raw_send_file(fd, offset, n);
            }

            async::task<expected<size_t, io::error>> async_send_file(int fd, uint64_t offset, uint64_t n) noexcept {
                auto guard = co_await _write_lock.scoped_lock();
                co_return co_await awaited_raw_send_file(fd, offset, n);
            }

            // A line without its "\n" (or "\r\n"), copied out of the
            // buffer; nullopt at the end of the stream
            // `c.read_line()` on this thread, `co_await c.async_read_line()` in a task
            SGCL_INLINE_HOT expected<optional<string>, io::error> read_line() {
                return _block_read_line();
            }

            SGCL_INLINE_HOT async::task<expected<optional<string>, io::error>> async_read_line() noexcept {
                return _co_read_line();
            }

            SGCL_INLINE_HOT expected<optional<string>, io::error> _block_read_line()  {
                std::lock_guard<sgcl::async::mutex> guard(_read_lock);
                return _line_of(_buffer().read_line());
            }

            async::task<expected<optional<string>, io::error>> _co_read_line() noexcept {
                auto guard = co_await _read_lock.scoped_lock();
                co_return _line_of(co_await _buffer().async_read_line());
            }

            // The close in a task, as the handle's async_close: an io::reader
            // or io::writer made of a connection binds this object, and its
            // async_close comes here rather than to the blocking pool
            virtual async::task<expected<void, io::error>> async_close() noexcept {
                co_return close();
            }

            // Takes no lock: a read in progress keeps the bound it started
            // with, the next read_line takes the new one
            SGCL_INLINE_HOT void set_max_line(size_t n) noexcept {
                _max_line.store(n, std::memory_order_relaxed);
            }

            SGCL_INLINE_HOT size_t max_line() const noexcept {
                return _max_line.load(std::memory_order_relaxed);
            }

            virtual expected<size_t, io::error> raw_read(const slice<byte>& buffer) = 0;
            virtual async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> buffer) noexcept = 0;
            virtual expected<size_t, io::error> raw_write(const slice<const byte>& data) = 0;
            virtual async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) noexcept = 0;

            // What the transport has for buffer without waiting: n > 0, 0 at
            // the end of the stream, nullopt when it would wait; or its
            // error. The default has nothing (a transport without it is
            // read the waiting way: raw_readable says it cannot tell)
            virtual expected<optional<size_t>, io::error> try_raw_read(const slice<byte>&) {
                return optional<size_t>();
            }

            // The transport's readiness to be read, as an awaitable without
            // a frame; the default cannot tell (readiness::supported false)
            virtual readiness raw_readable() noexcept {
                return readiness();
            }

            // What the transport takes of data without waiting: all of it,
            // a part, or none (0: it would wait); or its error. Through the
            // same checks as a write that waits (a close, the deadline).
            // The default takes none: the write goes the waiting way
            virtual expected<size_t, io::error> try_raw_write(const slice<const byte>&) {
                return size_t(0);
            }

            // The pieces in order without waiting, as try_raw_write takes
            // one: the bytes taken of all of them together. The default
            // takes them one after another and stops at the first not
            // taken whole
            virtual expected<size_t, io::error> try_raw_write_parts(const slice<const byte>* parts, size_t n) {
                size_t done = 0;
                for (size_t i = 0; i < n; ++i) {
                    auto r = try_raw_write(parts[i]);
                    if (!r) {
                        return r;
                    }
                    done += *r;
                    if (*r < parts[i].size()) {
                        break;
                    }
                }
                return done;
            }

            // The rest of the pieces from byte `done` of them all, waiting:
            // the bytes written from there. The default writes them one
            // after another
            virtual async::task<expected<size_t, io::error>> awaited_raw_write_parts(vector<slice<const byte>> parts, size_t done) noexcept {
                size_t written = 0;
                for (auto& p : parts) {
                    if (done >= p.size()) {
                        done -= p.size();
                        continue;
                    }
                    auto r = co_await awaited_raw_write(slice<const byte>(p.data() + done, p.size() - done));
                    if (!r) {
                        co_return fail(r);
                    }
                    written += *r;
                    done = 0;
                }
                co_return written;
            }

            // A file's bytes (send_file), the transport's way; the default
            // reads them into blocks and writes the blocks as pieces
            virtual expected<size_t, io::error> raw_send_file(int fd, uint64_t offset, uint64_t n) {
                return _send_file_blocks(fd, offset, n);
            }

            virtual async::task<expected<size_t, io::error>> awaited_raw_send_file(int fd, uint64_t offset, uint64_t n) noexcept {
                return _co_send_file_blocks(tracked_ptr<ConnImpl>(this), fd, offset, n);
            }

            // Descriptors passed with bytes over a unix-domain socket
            // (SCM_RIGHTS: connection::send_descriptors and
            // receive_descriptors), the write's and the read's locks taken as
            // a write and a read take them. A transport without a socket of
            // its own (TLS, the pair in memory) refuses: EOPNOTSUPP
            struct RightsRead {
                size_t bytes = 0;
                std::vector<int> fds;   // owned by the receiver from now on, close-on-exec
            };

            expected<size_t, io::error> send_rights(const slice<const byte>& data, const slice<const int>& fds) {
                std::lock_guard<sgcl::async::mutex> guard(_write_lock);
                return raw_send_rights(data, fds);
            }

            async::task<expected<size_t, io::error>> async_send_rights(slice<const byte> data, std::vector<int> fds) noexcept {
                auto guard = co_await _write_lock.scoped_lock();
                co_return co_await awaited_raw_send_rights(data, std::move(fds));
            }

            // A read with descriptors bypasses read_line's buffer: bytes
            // waiting in it would be skipped, and are refused (EBUSY)
            expected<RightsRead, io::error> receive_rights(const slice<byte>& buffer, size_t max) {
                std::lock_guard<sgcl::async::mutex> guard(_read_lock);
                if (_buffered && _buffered->buffered()) {
                    return fail(system_error(EBUSY, "receive_descriptors", describe()));
                }
                return raw_receive_rights(buffer, max);
            }

            async::task<expected<RightsRead, io::error>> async_receive_rights(slice<byte> buffer, size_t max) noexcept {
                auto guard = co_await _read_lock.scoped_lock();
                if (_buffered && _buffered->buffered()) {
                    co_return fail(system_error(EBUSY, "receive_descriptors", describe()));
                }
                co_return co_await awaited_raw_receive_rights(buffer, max);
            }

            virtual expected<size_t, io::error> raw_send_rights(const slice<const byte>&, const slice<const int>&) {
                return fail(system_error(EOPNOTSUPP, "send_descriptors", describe()));
            }

            virtual async::task<expected<size_t, io::error>> awaited_raw_send_rights(slice<const byte> data, std::vector<int> fds) noexcept {
                co_return raw_send_rights(data, slice<const int>(fds));
            }

            virtual expected<RightsRead, io::error> raw_receive_rights(const slice<byte>&, size_t) {
                return fail(system_error(EOPNOTSUPP, "receive_descriptors", describe()));
            }

            virtual async::task<expected<RightsRead, io::error>> awaited_raw_receive_rights(slice<byte> buffer, size_t max) noexcept {
                co_return raw_receive_rights(buffer, max);
            }

            // Ends the connection both ways; the operations in progress end
            // with io::errc::closed. close and set_deadline wake the waits
            // in progress and never wait themselves (noexcept, as the
            // socket's descriptor is); close_write may wait (TLS writes its
            // close_notify through the record's lock)
            virtual expected<void, io::error> close() noexcept = 0;
            virtual bool is_closed() const noexcept = 0;

            // The socket's descriptor, for a call the library does not make
            // (send_descriptors of another connection); -1 for a
            // transport without one of its own (TLS, the pair in memory) or
            // a closed one
            virtual int socket_fd() const noexcept {
                return -1;
            }
            virtual expected<void, io::error> close_write() = 0;
            virtual void set_deadline(int dir, time_point t) noexcept = 0;
            virtual time_point deadline(int dir) const noexcept = 0;   // time_point() for none

            virtual endpoint local_endpoint() const noexcept {
                return endpoint();
            }

            virtual endpoint remote_endpoint() const noexcept {
                return endpoint();
            }

            virtual string path() const noexcept {
                return string();
            }

            virtual expected<void, io::error> set_no_delay(bool) noexcept {
                return fail(system_error(EOPNOTSUPP, "set_no_delay", describe()));
            }

            virtual expected<void, io::error> set_keep_alive(std::chrono::nanoseconds) noexcept {
                return fail(system_error(EOPNOTSUPP, "set_keep_alive", describe()));
            }

            // What an error names the connection by: "tcp 1.2.3.4:5->6.7.8.9:80"
            virtual string describe() const noexcept = 0;

        protected:
            // A file sent through blocks (the default of raw_send_file):
            // rounds of up to FileBlocks blocks of 32 KB read by pread, each
            // round the pieces of one write; the blocks held by the pieces
            // (and by the frame) until the write is done
            static constexpr size_t FileBlocks = 4;
            using FileBlock = io::detail::CopyBlock;

            // One round read into the blocks: the bytes read (0: the file's end)
            static expected<size_t, io::error> _read_round(int fd, uint64_t at, uint64_t left, tracked_ptr<FileBlock>* blocks, vector<slice<const byte>>& parts) noexcept {
                parts.clear();
                size_t round = 0;
                for (size_t k = 0; k < FileBlocks && left > round; ++k) {
                    if (!blocks[k]) {
                        blocks[k] = make_tracked<FileBlock>();
                    }
                    const size_t want = size_t(std::min<uint64_t>(blocks[k]->size(), left - round));
                    ssize_t got;
                    do {
                        got = ::pread(fd, blocks[k]->data(), want, off_t(at + round));
                    } while (got < 0 && errno == EINTR);
                    if (got < 0) {
                        return fail(system_error(errno, "read", string("file")));
                    }
                    if (got == 0) {
                        break;
                    }
                    parts.push_back(slice<const byte>(blocks[k], blocks[k]->data(), size_t(got)));
                    round += size_t(got);
                    if (size_t(got) < want) {
                        break;   // short: most likely the end; the next round tells
                    }
                }
                return round;
            }

            expected<size_t, io::error> _send_file_blocks(int fd, uint64_t offset, uint64_t n) {
                tracked_ptr<FileBlock> blocks[FileBlocks];
                vector<slice<const byte>> parts;
                uint64_t sent = 0;
                while (sent < n) {
                    auto round = _read_round(fd, offset + sent, n - sent, blocks, parts);
                    if (!round) {
                        return fail(round);
                    }
                    if (*round == 0) {
                        break;
                    }
                    auto r = try_raw_write_parts(parts.data(), parts.size());
                    if (!r) {
                        return fail(r);
                    }
                    if (*r < *round) {
                        // the rest of the same write, from where the try
                        // stopped: a transport that kept a batch it took in
                        // part (TLS: records sealed, their tail unsent) goes
                        // on from it, never sealing those bytes again, which
                        // a write of the rest as new data would do
                        auto rest = awaited_raw_write_parts(parts, *r).wait();
                        if (!rest) {
                            return fail(rest);
                        }
                    }
                    sent += *round;
                }
                return size_t(sent);
            }

            static async::task<expected<size_t, io::error>> _co_send_file_blocks(tracked_ptr<ConnImpl> self, int fd, uint64_t offset, uint64_t n) noexcept {
                tracked_ptr<FileBlock> blocks[FileBlocks];
                vector<slice<const byte>> parts;
                uint64_t sent = 0;
                while (sent < n) {
                    auto round = _read_round(fd, offset + sent, n - sent, blocks, parts);
                    if (!round) {
                        co_return fail(round);
                    }
                    if (*round == 0) {
                        break;
                    }
                    auto r = self->try_raw_write_parts(parts.data(), parts.size());
                    if (!r) {
                        co_return fail(r);
                    }
                    if (*r < *round) {
                        auto rest = co_await self->awaited_raw_write_parts(parts, *r);
                        if (!rest) {
                            co_return fail(rest);
                        }
                    }
                    sent += *round;
                }
                co_return size_t(sent);
            }

        private:
            // The rest of a write begun by start_write, from `done` on;
            // `held`: the write lock taken by start_write, a parameter, so
            // that a task dropped without being awaited lets go of it with
            // its frame
            static async::task<expected<size_t, io::error>> _write_rest(tracked_ptr<ConnImpl> self, slice<const byte> data, size_t done, optional<sgcl::async::mutex::guard> held) noexcept {
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

            static async::task<expected<size_t, io::error>> _write_rest_parts(tracked_ptr<ConnImpl> self, vector<slice<const byte>> parts, size_t done, size_t total,
                                                                             optional<sgcl::async::mutex::guard> held) noexcept {
                if (!held) {
                    held.emplace(co_await self->_write_lock.scoped_lock());
                }
                auto r = co_await self->awaited_raw_write_parts(std::move(parts), done);
                held.reset();
                if (!r) {
                    co_return fail(r);
                }
                (void)total;
                co_return done + *r;
            }

            // Under the read lock
            SGCL_INLINE_HOT io::detail::BufferedReaderState& _buffer() noexcept {
                if (!_buffered) {
                    _buffered = make_tracked<io::detail::BufferedReaderState>(io::reader(tracked_ptr<ConnRawReader>(make_tracked<ConnRawReader>(tracked_ptr<ConnImpl>(this)))), io::detail::UnmanagedBlock());
                }
                _buffered->set_max_line(_max_line.load(std::memory_order_relaxed));
                return *_buffered;
            }

            SGCL_INLINE_HOT static expected<optional<string>, io::error> _line_of(const expected<optional<slice<const char>>, io::error>& r) {
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

        SGCL_INLINE_HOT expected<void, io::error> readiness::await_resume() noexcept {
            auto r = _op->await_resume();
            if (r == WaitResult::ready) {
                return expected<void, io::error>();
            }
            return fail(wait_error(r, *_d, "read", _c->describe()));
        }

        SGCL_INLINE_HOT expected<size_t, io::error> ConnRawReader::read(const slice<byte>& buffer) {
            return _c->raw_read(buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> ConnRawReader::async_read(const slice<byte>& buffer) noexcept {
            return _c->awaited_raw_read(buffer);
        }

        // A connection over a socket, TCP or unix
        class SocketConn final : public ConnImpl {
        public:
            SGCL_INLINE_HOT SocketConn(int fd, bool tcp, endpoint local, endpoint remote, const string& path) noexcept
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

            async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> b) noexcept override {
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

            expected<optional<size_t>, io::error> try_raw_read(const slice<byte>& b) noexcept override {
                if (b.empty()) {
                    return optional<size_t>(size_t(0));
                }
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("read", describe()));
                }
                return _recv_now(b);
            }

            readiness raw_readable() noexcept override {
                return readiness(_d, *this);
            }

            // The bytes with the descriptors (SCM_RIGHTS in the first
            // sendmsg; the rest of the bytes, if that one took part of them,
            // as a write sends them), waiting as a write waits. The bytes may
            // not be empty: a stream carries descriptors with bytes only
            expected<size_t, io::error> raw_send_rights(const slice<const byte>& data, const slice<const int>& fds) override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("send_descriptors", describe()));
                }
                if (auto e = _rights_args(data, fds.size())) {
                    return fail(*e);
                }
                size_t written = 0;
                bool look = true;
                for (;;) {
                    auto would_wait = written == 0 ? _sendmsg_now(data, fds, written, look) : _send_now(data, written, look);
                    if (!would_wait) {
                        return fail(would_wait);
                    }
                    if (!*would_wait) {
                        return written;
                    }
                    auto r = _d.wait(Descriptor::Write, look);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "send_descriptors", describe()));
                    }
                    look = !look;
                }
            }

            async::task<expected<size_t, io::error>> awaited_raw_send_rights(slice<const byte> data, std::vector<int> fds) noexcept override {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("send_descriptors", describe()));
                }
                if (auto e = _rights_args(data, fds.size())) {
                    co_return fail(*e);
                }
                size_t written = 0;
                bool look = true;
                for (;;) {
                    auto would_wait = written == 0 ? _sendmsg_now(data, fds, written, look) : _send_now(data, written, look);
                    if (!would_wait) {
                        co_return fail(would_wait);
                    }
                    if (!*would_wait) {
                        co_return written;
                    }
                    auto r = co_await _d.async_wait(Descriptor::Write, look);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "send_descriptors", describe()));
                    }
                    look = !look;
                }
            }

            // Bytes, and the descriptors that came with them (at most `max`),
            // waiting as a read waits
            expected<RightsRead, io::error> raw_receive_rights(const slice<byte>& b, size_t max) override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("receive_descriptors", describe()));
                }
                bool look = true;
                for (;;) {
                    auto n = _recvmsg_now(b, max, look);
                    if (!n) {
                        return fail(n);
                    }
                    if (*n) {
                        return std::move(**n);
                    }
                    auto r = _d.wait(Descriptor::Read, look);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "receive_descriptors", describe()));
                    }
                    look = !look;
                }
            }

            async::task<expected<RightsRead, io::error>> awaited_raw_receive_rights(slice<byte> b, size_t max) noexcept override {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("receive_descriptors", describe()));
                }
                bool look = true;
                for (;;) {
                    auto n = _recvmsg_now(b, max, look);
                    if (!n) {
                        co_return fail(n);
                    }
                    if (*n) {
                        co_return std::move(**n);
                    }
                    auto r = co_await _d.async_wait(Descriptor::Read, look);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "receive_descriptors", describe()));
                    }
                    look = !look;
                }
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

            async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) noexcept override {
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

            expected<size_t, io::error> try_raw_write(const slice<const byte>& data) noexcept override {
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

            // The pieces in one sendmsg (up to 64 of them a call), as long
            // as the socket takes them whole
            expected<size_t, io::error> try_raw_write_parts(const slice<const byte>* parts, size_t n) noexcept override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("write", describe()));
                }
                size_t total = 0;
                for (size_t i = 0; i < n; ++i) {
                    total += parts[i].size();
                }
                size_t written = 0;
                bool deadline = true;
                while (written < total) {
                    if (auto e = _check(Descriptor::Write, "write", deadline)) {
                        return fail(*e);
                    }
                    deadline = true;
                    iovec v[64];
                    int k = 0;
                    size_t skip = written;
                    for (size_t i = 0; i < n && k < 64; ++i) {
                        size_t size = parts[i].size();
                        if (skip >= size) {
                            skip -= size;
                            continue;
                        }
                        v[k].iov_base = const_cast<byte*>(parts[i].data() + skip);
                        v[k].iov_len = size - skip;
                        skip = 0;
                        ++k;
                    }
                    msghdr m{};
                    m.msg_iov = v;
                    m.msg_iovlen = k;
                    _d.prepare(Descriptor::Write);
                    ssize_t sent = ::sendmsg(_d.fd(), &m, SendFlags);
                    if (sent >= 0) {
                        written += size_t(sent);
                        continue;
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        return fail(system_error(e, "write", describe()));
                    }
                    break;
                }
                return written;
            }

            // A file by sendfile(2): the kernel moves its pages to the
            // socket, nothing through the process. Waits as a write does
            // (the reactor, the write deadline); a socket or a file the
            // call does not take (a Unix socket on some systems) goes the
            // blocks' way from where sendfile stopped
            expected<size_t, io::error> raw_send_file(int fd, uint64_t offset, uint64_t n) override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("write", describe()));
                }
                uint64_t sent = 0;
                bool look = true;
                for (;;) {
                    auto s = _send_file_now(fd, offset, n, sent, look);
                    if (!s) {
                        return fail(s);
                    }
                    if (*s == FileSend::done) {
                        return size_t(sent);
                    }
                    if (*s == FileSend::unsupported) {
                        auto r = _send_file_blocks(fd, offset + sent, n - sent);
                        if (!r) {
                            return fail(r);
                        }
                        return size_t(sent + *r);
                    }
                    auto r = _d.wait(Descriptor::Write, look);
                    if (r != WaitResult::ready) {
                        return fail(wait_error(r, _d, "write", describe()));
                    }
                    look = !look;
                }
            }

            async::task<expected<size_t, io::error>> awaited_raw_send_file(int fd, uint64_t offset, uint64_t n) noexcept override {
                Operation op(_d);
                if (!op) {
                    co_return fail(closed_error("write", describe()));
                }
                uint64_t sent = 0;
                bool look = true;
                for (;;) {
                    auto s = _send_file_now(fd, offset, n, sent, look);
                    if (!s) {
                        co_return fail(s);
                    }
                    if (*s == FileSend::done) {
                        co_return size_t(sent);
                    }
                    if (*s == FileSend::unsupported) {
                        auto r = co_await _co_send_file_blocks(tracked_ptr<ConnImpl>(this), fd, offset + sent, n - sent);
                        if (!r) {
                            co_return fail(r);
                        }
                        co_return size_t(sent + *r);
                    }
                    auto r = co_await _d.async_wait(Descriptor::Write, look);
                    if (r != WaitResult::ready) {
                        co_return fail(wait_error(r, _d, "write", describe()));
                    }
                    look = !look;
                }
            }

            expected<void, io::error> close() noexcept override {
                int e = _d.close();
                if (e) {
                    return fail(system_error(e, "close", describe()));
                }
                return {};
            }

            bool is_closed() const noexcept override {
                return _d.closing();
            }

            expected<void, io::error> close_write() noexcept override {
                Operation op(_d);
                if (!op) {
                    return fail(closed_error("close_write", describe()));
                }
                if (::shutdown(_d.fd(), SHUT_WR) != 0) {
                    return fail(system_error(errno, "close_write", describe()));
                }
                return {};
            }

            void set_deadline(int dir, time_point t) noexcept override {
                _d.set_deadline(dir, t);
            }

            time_point deadline(int dir) const noexcept override {
                return _d.deadline(dir);
            }

            endpoint local_endpoint() const noexcept override {
                return _local;
            }

            endpoint remote_endpoint() const noexcept override {
                return _remote;
            }

            string path() const noexcept override {
                return _path;
            }

            expected<void, io::error> set_no_delay(bool on) noexcept override {
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

            expected<void, io::error> set_keep_alive(std::chrono::nanoseconds idle) noexcept override {
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

            string describe() const noexcept override {
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

            async::task<int> _co_connected(async::stop_token stop) noexcept {
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

            SGCL_INLINE_HOT void set_local(endpoint e) noexcept {
                _local = e;
            }

            SGCL_INLINE_HOT void set_remote(endpoint e) noexcept {
                _remote = e;
            }

            // The descriptor, for the setup of a socket not yet shared
            SGCL_INLINE_HOT int fd() const noexcept {
                return _d.fd();
            }

            int socket_fd() const noexcept override {
                return _d.closing() ? -1 : _d.fd();
            }

        private:
            // One receive without waiting, for the three reads (the blocking,
            // the awaited, the try): the descriptor's state looked at first
            // (a close, the read deadline), then the call. The bytes (0 at
            // the end of the stream), nullopt when the socket would wait,
            // or the error
            expected<optional<size_t>, io::error> _recv_now(const slice<byte>& b, bool deadline = true) noexcept {
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
            expected<bool, io::error> _send_now(const slice<const byte>& data, size_t& written, bool deadline = true) noexcept {
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

            // The most descriptors one message carries (SCM_MAX_FD on Linux)
            static constexpr size_t MaxRights = 253;

            // What send_descriptors refuses before a call: a socket that is no
            // unix-domain one (EOPNOTSUPP), no bytes or too many descriptors
            // (EINVAL)
            optional<io::error> _rights_args(const slice<const byte>& data, size_t n) const noexcept {
                if (_tcp) {
                    return system_error(EOPNOTSUPP, "send_descriptors", describe());
                }
                if (data.empty() || n == 0 || n > MaxRights) {
                    return system_error(EINVAL, "send_descriptors", describe());
                }
                return nullopt;
            }

            // The first sendmsg, with the descriptors; as _send_now otherwise
            expected<bool, io::error> _sendmsg_now(const slice<const byte>& data, const slice<const int>& fds, size_t& written, bool deadline) noexcept {
                for (;;) {
                    if (auto e = _check(Descriptor::Write, "send_descriptors", deadline)) {
                        return fail(*e);
                    }
                    _d.prepare(Descriptor::Write);
                    alignas(struct ::cmsghdr) char control[CMSG_SPACE(sizeof(int) * MaxRights)] = {};
                    struct ::iovec iov = {const_cast<byte*>(data.data()), data.size()};
                    struct ::msghdr m = {};
                    m.msg_iov = &iov;
                    m.msg_iovlen = 1;
                    m.msg_control = control;
                    m.msg_controllen = socklen_t(CMSG_SPACE(sizeof(int) * fds.size()));
                    struct ::cmsghdr* c = CMSG_FIRSTHDR(&m);
                    c->cmsg_level = SOL_SOCKET;
                    c->cmsg_type = SCM_RIGHTS;
                    c->cmsg_len = socklen_t(CMSG_LEN(sizeof(int) * fds.size()));
                    sgcl::detail::copy_bytes(CMSG_DATA(c), fds.data(), sizeof(int) * fds.size());
                    ssize_t n = ::sendmsg(_d.fd(), &m, SendFlags);
                    if (n >= 0) {
                        written = size_t(n);
                        return written < data.size() ? expected<bool, io::error>(_send_now(data, written, true)) : expected<bool, io::error>(false);
                    }
                    int e = errno;
                    if (e == EINTR) {
                        continue;
                    }
                    if (e != EAGAIN && e != EWOULDBLOCK) {
                        return fail(system_error(e, "send_descriptors", describe()));
                    }
                    return true;
                }
            }

            // One recvmsg without waiting: the bytes and the descriptors
            // (close-on-exec), nullopt when the socket would wait. Descriptors
            // past `max` the kernel dropped (MSG_CTRUNC): what came is closed
            // and the read is EMSGSIZE
            expected<optional<RightsRead>, io::error> _recvmsg_now(const slice<byte>& b, size_t max, bool deadline) noexcept {
                max = std::min(max, MaxRights);
                for (;;) {
                    if (auto e = _check(Descriptor::Read, "receive_descriptors", deadline)) {
                        return fail(*e);
                    }
                    _d.prepare(Descriptor::Read);
                    alignas(struct ::cmsghdr) char control[CMSG_SPACE(sizeof(int) * MaxRights)] = {};
                    struct ::iovec iov = {b.data(), b.size()};
                    struct ::msghdr m = {};
                    m.msg_iov = &iov;
                    m.msg_iovlen = 1;
                    m.msg_control = control;
                    m.msg_controllen = socklen_t(CMSG_SPACE(sizeof(int) * std::max<size_t>(max, 1)));
#if defined(MSG_CMSG_CLOEXEC)
                    ssize_t n = ::recvmsg(_d.fd(), &m, MSG_CMSG_CLOEXEC);
#else
                    ssize_t n = ::recvmsg(_d.fd(), &m, 0);
#endif
                    if (n < 0) {
                        int e = errno;
                        if (e == EINTR) {
                            continue;
                        }
                        if (e != EAGAIN && e != EWOULDBLOCK) {
                            return fail(system_error(e, "receive_descriptors", describe()));
                        }
                        return optional<RightsRead>();
                    }
                    RightsRead r;
                    r.bytes = size_t(n);
                    for (struct ::cmsghdr* c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c)) {
                        if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS) {
                            continue;
                        }
                        // what the header says came, but no more than the
                        // control block holds: under MSG_CTRUNC macOS keeps
                        // the length of all that was sent, and the ints past
                        // the block's end (zeros) were closed as descriptor 0
                        const unsigned char* at = CMSG_DATA(c);
                        const size_t room = size_t((const unsigned char*)control + m.msg_controllen - at);
                        const size_t count = std::min<size_t>((c->cmsg_len - CMSG_LEN(0)) / sizeof(int), room / sizeof(int));
                        for (size_t i = 0; i < count; ++i) {
                            int fd;
                            std::memcpy(&fd, at + i * sizeof(int), sizeof(int));   // an int of the control block, unaligned as it may be
                            ::fcntl(fd, F_SETFD, FD_CLOEXEC);
                            r.fds.push_back(fd);
                        }
                    }
                    if (m.msg_flags & MSG_CTRUNC) {
                        for (int fd : r.fds) {
                            ::close(fd);
                        }
                        return fail(system_error(EMSGSIZE, "receive_descriptors", describe()));
                    }
                    return optional<RightsRead>(std::move(r));
                }
            }

            enum class FileSend : uint8_t { done, would_wait, unsupported };

            // The part of a file's sending that goes without waiting, from
            // `sent` on: done (all of it, or the file ended), would_wait, or
            // unsupported (sendfile does not take this socket or file: the
            // caller goes on another way from `sent`); or the error. The
            // descriptor's state looked at before every call, as a send
            expected<FileSend, io::error> _send_file_now(int fd, uint64_t offset, uint64_t n, uint64_t& sent, bool deadline = true) noexcept {
                while (sent < n) {
                    if (auto e = _check(Descriptor::Write, "write", deadline)) {
                        return fail(*e);
                    }
                    deadline = true;
                    _d.prepare(Descriptor::Write);
                    const uint64_t want = std::min<uint64_t>(n - sent, uint64_t(1) << 30);
#if defined(__APPLE__)
                    off_t len = off_t(want);
                    const int rc = ::sendfile(fd, _d.fd(), off_t(offset + sent), &len, nullptr, 0);
                    const int e = rc == 0 ? 0 : errno;
                    sent += uint64_t(len);   // what went, also with EAGAIN and EINTR
                    if (rc == 0) {
                        if (len == 0) {
                            return FileSend::done;   // the file ended before n
                        }
                        continue;
                    }
#elif defined(__linux__)
                    off_t at = off_t(offset + sent);
                    const ssize_t k = _linux_sendfile(fd, at, size_t(want));
                    const int e = k < 0 ? errno : 0;
                    if (k > 0) {
                        sent += uint64_t(k);
                        continue;
                    }
                    if (k == 0) {
                        return FileSend::done;   // the file ended before n
                    }
#else
                    const int e = ENOSYS;
#endif
                    if (e == EINTR) {
                        continue;
                    }
                    if (e == EAGAIN || e == EWOULDBLOCK) {
                        return FileSend::would_wait;
                    }
                    if (e == ENOTSUP || e == EOPNOTSUPP || e == ENOTSOCK || e == EINVAL || e == ENOSYS) {
                        return FileSend::unsupported;
                    }
                    return fail(system_error(e, "write", describe()));
                }
                return FileSend::done;
            }

#if defined(__linux__)
            // sendfile has no MSG_NOSIGNAL: SIGPIPE held back for the call
            // on this thread, and one it raised taken off before it is
            // unblocked (a write to a connection the peer closed is EPIPE,
            // as a send's)
            ssize_t _linux_sendfile(int fd, off_t& at, size_t n) noexcept {
                sigset_t pipe_only;
                sigset_t old;
                sigemptyset(&pipe_only);
                sigaddset(&pipe_only, SIGPIPE);
                pthread_sigmask(SIG_BLOCK, &pipe_only, &old);
                const ssize_t k = ::sendfile(_d.fd(), fd, &at, n);
                const int e = errno;
                if (k < 0 && e == EPIPE) {
                    const timespec zero = {0, 0};
                    while (sigtimedwait(&pipe_only, nullptr, &zero) == SIGPIPE) {
                    }
                }
                pthread_sigmask(SIG_SETMASK, &old, nullptr);
                errno = e;
                return k;
            }
#endif

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
                if (errno == ENOTCONN) {
                    return EINPROGRESS;
                }
                // The connect failed between the two calls: macOS then answers
                // getpeername with EINVAL, and the error is in SO_ERROR now
                // (measured: 7135 of 20000 refused loopback connects)
                int failed = errno;
                if (::getsockopt(_d.fd(), SOL_SOCKET, SO_ERROR, &err, &len) == 0 && err) {
                    return err;
                }
                return failed;
            }

            // Before a system call: closing, or the deadline passed (the
            // clock read unless `deadline` is false: the wait just before
            // looked at it)
            SGCL_INLINE_HOT optional<io::error> _check(int dir, const char* op, bool deadline = true) const noexcept {
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
            SGCL_INLINE_HOT MemoryConn(tracked_ptr<MemoryPipe> in, tracked_ptr<MemoryPipe> out) noexcept
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

            async::task<expected<size_t, io::error>> awaited_raw_read(slice<byte> b) noexcept override {
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

            async::task<expected<size_t, io::error>> awaited_raw_write(slice<const byte> data) noexcept override {
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
            expected<void, io::error> close() noexcept override {
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

            void set_deadline(int dir, time_point t) noexcept override {
                {
                    std::lock_guard lock(_m);
                    _deadline[dir] = t;
                }
                _wake();
            }

            time_point deadline(int dir) const noexcept override {
                std::lock_guard lock(_m);
                return _deadline[dir];
            }

            string describe() const noexcept override {
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

            SGCL_INLINE_HOT WaitState _wait_state(int dir) noexcept {
                std::lock_guard lock(_m);
                return WaitState{_deadline[dir], _rearm};
            }

            // The waits in progress woken to look at their state again: the
            // channel they select on closed, a new one for the next waits
            SGCL_INLINE_HOT void _wake() {
                tracked_ptr<async::detail::ChannelState<void>> old;
                {
                    std::lock_guard lock(_m);
                    old = _rearm;
                    _rearm = make_tracked<async::detail::ChannelState<void>>();
                }
                old->close();
            }

            SGCL_INLINE_HOT optional<io::error> _check(int, time_point deadline, const char* op) const noexcept {
                if (is_closed()) {
                    return closed_error(op, describe());
                }
                if (deadline != time_point() && sgcl::clock::now() >= deadline) {
                    return system_error(ETIMEDOUT, op, describe());
                }
                return nullopt;
            }

            SGCL_INLINE_HOT optional<io::error> _check_write(time_point deadline) const noexcept {
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

            SGCL_INLINE_HOT static size_t _take(const slice<const byte>& from, const slice<byte>& to) noexcept {
                size_t n = std::min(from.size(), to.size());
                std::memcpy(to.data(), from.data(), n);
                return n;
            }

            tracked_ptr<MemoryPipe> _in;
            tracked_ptr<MemoryPipe> _out;
            mutable std::mutex _m;                       // the deadlines and the rearm channel
            time_point _deadline[2] = {};
            tracked_ptr<async::detail::ChannelState<void>> _rearm;
            std::atomic<bool> _closed = {false};
        };

        // A regular file's position and the bytes from it to its end (what
        // a connection's read_from and a response's write(file) send);
        // nullopt for any other file (a pipe, a device) or a position that
        // cannot be told
        inline optional<pair<uint64_t, uint64_t>> file_rest(const io::file& f) noexcept {
            const int fd = f.fd();
            struct stat st;
            if (fd < 0 || ::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
                return nullopt;
            }
            const off_t at = ::lseek(fd, 0, SEEK_CUR);
            if (at < 0) {
                return nullopt;
            }
            const uint64_t size = uint64_t(st.st_size);
            return pair<uint64_t, uint64_t>(uint64_t(at), size > uint64_t(at) ? size - uint64_t(at) : 0);
        }

        class ListenerImpl;
        class UdpImpl;
    }

    class connection {
    public:
        connection() noexcept = default;   // no connection; an operation on it is a contract violation

        // At most buffer.size() bytes, as many as have come (at least one);
        // 0 at the end of the stream
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> read(const slice<byte>& buffer) const {
            return _block_read(buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept {
            return _co_read(buffer);
        }

        // The whole buffer; 0 when the stream ends before its first byte,
        // io::errc::unexpected_eof when it ends part way
        // `read_full(...)` on this thread, `co_await async_read_full(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> read_full(const slice<byte>& buffer) const {
            return _get().read_full(buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_read_full(const slice<byte>& buffer) const noexcept {
            return _get().async_read_full(buffer);
        }

        // Everything to the end of the stream
        // `read_all(...)` on this thread, `co_await async_read_all(...)` in a task
        SGCL_INLINE_HOT expected<vector<byte>, io::error> read_all() const {
            return _get().read_all();
        }

        SGCL_INLINE_HOT async::task<expected<vector<byte>, io::error>> async_read_all() const noexcept {
            return _get().async_read_all();
        }

        // `read_all_text(...)` on this thread, `co_await async_read_all_text(...)` in a task
        SGCL_INLINE_HOT expected<string, io::error> read_all_text() const {
            return _get().read_all_text();
        }

        SGCL_INLINE_HOT async::task<expected<string, io::error>> async_read_all_text() const noexcept {
            return _get().async_read_all_text();
        }

        // The next line without its "\n" and "\r\n"; nullopt at the end.
        // The first call puts a buffer of 8 KB in front of the connection,
        // which read() takes from first from then on. A line longer than
        // set_max_line (64 KB by default: the input is the network's) is
        // io::errc::line_too_long.
        // `c.read_line()` on this thread, `co_await c.async_read_line()` in a task
        SGCL_INLINE_HOT expected<optional<string>, io::error> read_line() const {
            return _block_read_line();
        }

        SGCL_INLINE_HOT async::task<expected<optional<string>, io::error>> async_read_line() const noexcept {
            return _co_read_line();
        }

        SGCL_INLINE_HOT void set_max_line(size_t bytes) const noexcept {
            _get().set_max_line(bytes);
        }

        SGCL_INLINE_HOT size_t max_line() const noexcept {
            return _get().max_line();
        }

        // Everything, or the error
        // `write(...)` on this thread, `co_await async_write(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> write(const slice<const byte>& data) const {
            return _block_write(data);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const noexcept {
            return _co_write(data);
        }

        // The text's bytes; the async form holds the string for as long as it runs
        // `write(...)` on this thread, `co_await async_write(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> write(const string& text) const {
            return _block_write(text);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_write(const string& text) const noexcept {
            return _co_write(text);
        }

        // A literal, a character array, a std::string_view: as a string
        // (an exact match, else the conversions to a string and to bytes
        // tie); the async form copies the text, which the task then holds
        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT expected<size_t, io::error> write(const T& text) const {
            return _block_write(slice<const byte>(text));
        }

        template<sgcl::detail::TextArgument T>
        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_write(const T& text) const noexcept {
            return _co_write(string(slice<const byte>(text)));
        }

        // A file from its position to its end, written to the connection:
        // what io::copy(connection, file) calls (Go's ReaderFrom). Over TCP
        // by sendfile, the file's pages going to the socket with no copy
        // through the process; over TLS read in blocks and sealed where
        // they lie. The bytes sent; the file's position moved past them;
        // the write's deadline holds. A file that is not a regular one (a
        // pipe) is copied as io::copy copies any reader
        // `read_from(...)` on this thread, `co_await async_read_from(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> read_from(const io::file& f) const {
            auto span = detail::file_rest(f);
            if (!span) {
                return io::detail::copy_loop(*this, f);
            }
            auto r = _get().send_file(f.fd(), span->first, span->second);
            if (r) {
                ::lseek(f.fd(), off_t(span->first + *r), SEEK_SET);
            }
            return r;
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_read_from(io::file f) const noexcept {
            return _co_read_from(*this, std::move(f));
        }

        // Everything to the end of this stream, written to other (an echo
        // is c.copy_to(c), a proxy two of them): the bytes copied
        // `copy_to(...)` on this thread, `co_await async_copy_to(...)` in a task
        SGCL_INLINE_HOT expected<size_t, io::error> copy_to(const connection& other) const {
            return _get().copy_to(other._get());
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_copy_to(const connection& other) const noexcept {
            return _get().async_copy_to(other._get());
        }

        // The connection ended now, both ways: the reads, writes and
        // waits in progress in other tasks end with io::errc::closed; a
        // second close does nothing. The descriptor goes back to the
        // system when the last operation in progress has let go of it.
        // `close()` on this thread, `co_await async_close()` in a task (a
        // socket's close never waits; a transport over one, TLS, sends its
        // closing record first)
        SGCL_INLINE_HOT expected<void, io::error> close() const noexcept {
            return _get().close();
        }

        SGCL_INLINE_HOT async::task<expected<void, io::error>> async_close() const noexcept {
            return _close(_impl);
        }

        // The writing half ended (shutdown(SHUT_WR)): the peer reads the
        // end of the stream, and this side can still read
        SGCL_INLINE_HOT expected<void, io::error> close_write() const {
            return _get().close_write();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The socket's descriptor, owned by the connection, for a call of
        // the system the library does not make (passing the socket to
        // another process: send_descriptors); -1 for a
        // connection without a socket of its own (TLS, in_memory) or a
        // closed one
        SGCL_INLINE_HOT int fd() const noexcept {
            return _get().socket_fd();
        }

        // What receive_descriptors read: the bytes, and the descriptors that
        // came with them, each a file the program owns from now on (a file,
        // a socket, a pipe: io::file is every descriptor)
        struct received {
            size_t size = 0;
            vector<io::file> files;
        };

        // Descriptors passed to the process at the other end of a
        // unix-domain connection (SCM_RIGHTS, Go's UnixConn.WriteMsgUnix
        // with syscall.UnixRights): the bytes of `data`, not empty, with the
        // descriptors `fds` (1 to 253) in their first message, duplicated
        // into the receiver as it reads them; the sender's stay its own.
        // EOPNOTSUPP for TCP, TLS and the pair in memory. The bytes written.
        // `send_descriptors(...)` on this thread, `co_await
        // async_send_descriptors(...)` in a task (which copies `fds` before
        // it starts)
        SGCL_INLINE_HOT expected<size_t, io::error> send_descriptors(const slice<const byte>& data, const slice<const int>& fds) const {
            return _get().send_rights(data, fds);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_send_descriptors(const slice<const byte>& data, const slice<const int>& fds) const noexcept {
            return _get().async_send_rights(data, std::vector<int>(fds.begin(), fds.end()));
        }

        // A read of the bytes there (at most the buffer's size; 0 at the end
        // of the stream) and of the descriptors that came with them (at
        // most `max`; those past it the kernel drops, and the read is
        // EMSGSIZE with what came closed). Bytes read_line has buffered are
        // refused (EBUSY): descriptors are read beside the connection's
        // buffer, not through it.
        // `receive_descriptors(...)` on this thread, `co_await
        // async_receive_descriptors(...)` in a task
        SGCL_INLINE_HOT expected<received, io::error> receive_descriptors(const slice<byte>& buffer, size_t max = 16) const {
            auto r = _get().receive_rights(buffer, max);
            if (!r) {
                return detail::fail(r);
            }
            return _received(*r);
        }

        SGCL_INLINE_HOT async::task<expected<received, io::error>> async_receive_descriptors(const slice<byte>& buffer, size_t max = 16) const noexcept {
            return _co_receive_descriptors(_impl, buffer, max);
        }

        // The addresses of the two ends; empty for a unix socket and for
        // the pair in memory
        SGCL_INLINE_HOT endpoint local_endpoint() const noexcept {
            return _get().local_endpoint();
        }

        SGCL_INLINE_HOT endpoint remote_endpoint() const noexcept {
            return _get().remote_endpoint();
        }

        // The path of a unix socket, empty for anything else
        SGCL_INLINE_HOT string path() const noexcept {
            return _get().path();
        }

        // Absolute deadlines on the module's clock: a read (write) that
        // starts after the deadline of its direction, or would wait past
        // it, fails with ETIMEDOUT; time_point() removes it. A change
        // applies to the operations in progress too. Where a timeout per
        // operation is wanted, it is c.set_read_deadline(clock::now() + d)
        // before each; a timeout() around a read is not the same (the
        // read goes on after the race is lost, and takes the data).
        SGCL_INLINE_HOT void set_deadline(time_point t) const noexcept {
            _get().set_deadline(detail::Descriptor::Read, t);
            _get().set_deadline(detail::Descriptor::Write, t);
        }

        SGCL_INLINE_HOT void set_read_deadline(time_point t) const noexcept {
            _get().set_deadline(detail::Descriptor::Read, t);
        }

        SGCL_INLINE_HOT void set_write_deadline(time_point t) const noexcept {
            _get().set_deadline(detail::Descriptor::Write, t);
        }

        // The deadline of a direction, time_point() when there is none
        SGCL_INLINE_HOT time_point read_deadline() const noexcept {
            return _get().deadline(detail::Descriptor::Read);
        }

        SGCL_INLINE_HOT time_point write_deadline() const noexcept {
            return _get().deadline(detail::Descriptor::Write);
        }

        // TCP only (EOPNOTSUPP for anything else): Nagle's algorithm off
        // (the default, as in Go) or on; keep-alive probes after `idle`
        // of silence (15 s by default, as in Go), zero turns them off
        SGCL_INLINE_HOT expected<void, io::error> set_no_delay(bool on) const noexcept {
            return _get().set_no_delay(on);
        }

        SGCL_INLINE_HOT expected<void, io::error> set_keep_alive(duration idle) const noexcept {
            return _get().set_keep_alive(std::chrono::nanoseconds(idle));
        }

        // Two connected ends in memory (Go's net.Pipe): what one writes the
        // other reads, a write waiting for the reads that take it, nothing
        // buffered; deadlines, close and close_write as on a socket. For
        // tests without sockets.
        SGCL_INLINE_HOT static pair<connection, connection> in_memory() noexcept {
            tracked_ptr<detail::MemoryPipe> a = make_tracked<detail::MemoryPipe>();
            tracked_ptr<detail::MemoryPipe> b = make_tracked<detail::MemoryPipe>();
            return pair<connection, connection>(connection(tracked_ptr<detail::ConnImpl>(make_tracked<detail::MemoryConn>(a, b))),
                                    connection(tracked_ptr<detail::ConnImpl>(make_tracked<detail::MemoryConn>(b, a))));
        }

        // Whether this handle holds a connection
        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_impl;
        }

        // The same connection
        SGCL_INLINE_HOT friend bool operator==(const connection& a, const connection& b) noexcept {
            return a._impl == b._impl;
        }

    private:
        friend struct detail::ConnectionAccess;
        friend struct io::detail::HandleAccess;

        // A connection over the transport given: the module's own (the
        // sockets, the pair in memory, TLS), through ConnectionAccess
        SGCL_INLINE_HOT explicit connection(const tracked_ptr<detail::ConnImpl>& impl) noexcept
        : _impl(impl) {
        }

        // What an io::reader or io::writer made of the handle binds: the
        // connection itself, not the handle, which may go first
        SGCL_INLINE_HOT const tracked_ptr<detail::ConnImpl>& _stream_state() const noexcept {
            return _impl;
        }

        static async::task<expected<void, io::error>> _close(tracked_ptr<detail::ConnImpl> c) noexcept {
            co_return co_await c->async_close();
        }

        SGCL_INLINE_HOT detail::ConnImpl& _get() const noexcept {
            assert(_impl && "an empty net::connection");
            return *_impl;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT connection(sgcl::detail::FromWord, const tracked_ptr<detail::ConnImpl>& w) noexcept
        : _impl(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ConnImpl>& _handle_word() noexcept {
            return _impl;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ConnImpl>& _handle_word() const noexcept {
            return _impl;
        }

        tracked_ptr<detail::ConnImpl> _impl;

        // the two halves of the operations above: a thread's and a task's
        SGCL_INLINE_HOT expected<size_t, io::error> _block_read(const slice<byte>& buffer) const {
            return _get().read(buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> _co_read(const slice<byte>& buffer) const noexcept {
            return _get().async_read(buffer);
        }

        SGCL_INLINE_HOT expected<optional<string>, io::error> _block_read_line() const {
            return _get()._block_read_line();
        }

        SGCL_INLINE_HOT async::task<expected<optional<string>, io::error>> _co_read_line() const noexcept {
            return _get()._co_read_line();
        }

        SGCL_INLINE_HOT expected<size_t, io::error> _block_write(const slice<const byte>& data) const {
            return _get().write(data);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> _co_write(const slice<const byte>& data) const noexcept {
            return _get().async_write(data);
        }

        SGCL_INLINE_HOT expected<size_t, io::error> _block_write(const string& text) const {
            return _get().write(as_bytes(text.as_slice()));
        }

        SGCL_INLINE_HOT async::task<expected<size_t, io::error>> _co_write(const string& text) const noexcept {
            return _get().async_write(as_bytes(text.as_slice()));
        }

        static received _received(const detail::ConnImpl::RightsRead& r) noexcept {
            received out;
            out.size = r.bytes;
            for (int fd : r.fds) {
                out.files.push_back(io::from_fd(fd, string("descriptor")));
            }
            return out;
        }

        static async::task<expected<received, io::error>> _co_receive_descriptors(tracked_ptr<detail::ConnImpl> c, slice<byte> buffer, size_t max) noexcept {
            auto r = co_await c->async_receive_rights(buffer, max);
            if (!r) {
                co_return detail::fail(r);
            }
            co_return _received(*r);
        }

        static async::task<expected<size_t, io::error>> _co_read_from(connection self, io::file f) noexcept {
            auto span = detail::file_rest(f);
            if (!span) {
                co_return co_await io::detail::async_copy_loop<const connection, io::file>(self, f);
            }
            auto r = co_await self._get().async_send_file(f.fd(), span->first, span->second);
            if (r) {
                ::lseek(f.fd(), off_t(span->first + *r), SEEK_SET);
            }
            co_return r;
        }
    };

    namespace detail {
        SGCL_INLINE_HOT ConnImpl& ConnectionAccess::impl(const connection& c) noexcept {
            return c._get();
        }

        SGCL_INLINE_HOT connection ConnectionAccess::make(const tracked_ptr<ConnImpl>& impl) noexcept {
            return connection(impl);
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
            SGCL_INLINE_HOT ListenerImpl(int fd, bool tcp, endpoint local, const string& path, bool unlink_on_close) noexcept
            : _d(fd)
            , _local(local)
            , _path(path)
            , _tcp(tcp)
            , _unlink(unlink_on_close) {
            }

            virtual ~ListenerImpl() = default;

            // `co_await c.async_accept()` in a task, `c.accept().wait()` on a thread
            SGCL_INLINE_HOT expected<connection, io::error> accept() {
                return _block_accept();
            }

            SGCL_INLINE_HOT async::task<expected<connection, io::error>> async_accept() noexcept {
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

            virtual async::task<expected<connection, io::error>> _co_accept() noexcept {
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
            virtual expected<void, io::error> close() noexcept {
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

            virtual string path() const noexcept {
                return _path;
            }

            virtual string describe() const noexcept {
                return _tcp ? string("tcp ") + _local.to_string() : string("unix ") + _path;
            }

        protected:
            // A listener over another (TLS's): no socket of its own
            SGCL_INLINE_HOT ListenerImpl() noexcept
            : _d(-1, false)
            , _tcp(false)
            , _unlink(false) {
            }

        private:
            // The descriptors (or the kernel's memory) run out: the
            // connection waits in the backlog, the listener stays readable,
            // and an accept in a loop would spin; so a pause, doubled from
            // 5 ms to a second, as Go's server takes
            SGCL_INLINE_HOT static bool _exhausted(int e) noexcept {
                return e == EMFILE || e == ENFILE || e == ENOBUFS || e == ENOMEM;
            }

            SGCL_INLINE_HOT static std::chrono::nanoseconds _next_pause(std::chrono::nanoseconds p) noexcept {
                using namespace std::chrono_literals;
                if (p == std::chrono::nanoseconds::zero()) {
                    return 5ms;
                }
                return std::min<std::chrono::nanoseconds>(p * 2, 1s);
            }

            expected<connection, io::error> _accepted(int s, SockAddr& from) noexcept {
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
                return ConnectionAccess::make(tracked_ptr<ConnImpl>(make_tracked<SocketConn>(s, _tcp, local, remote, _path)));
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

    class listener;

    namespace detail {
        // The module's way to make a listener over its object (a socket's,
        // TLS's), never a public constructor
        struct ListenerAccess {
            static listener make(const tracked_ptr<ListenerImpl>& impl) noexcept;
        };
    }

    // A listening socket (tcp::listen, unix_domain::listen): the
    // connections it accepts
    class listener {
    public:
        listener() noexcept = default;

        // The next connection. A connection aborted before it was taken
        // (ECONNABORTED) is skipped; when the descriptors run out (EMFILE)
        // the accept waits, 5 ms and doubling to a second, and tries again
        // rather than spin. It fails with io::errc::closed after close()
        // (from any task: an accept in progress ends), or on an error that
        // will not pass.
        // `x.accept(...)` on this thread, `co_await x.async_accept(...)` in a task
        SGCL_INLINE_HOT expected<connection, io::error> accept() const {
            return _block_accept();
        }

        SGCL_INLINE_HOT async::task<expected<connection, io::error>> async_accept() const noexcept {
            return _co_accept();
        }

        // No more connections; the accepts in progress end. A unix
        // listener removes its socket's file.
        SGCL_INLINE_HOT expected<void, io::error> close() const noexcept {
            return _get().close();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The address it listens on: ":0" given, the port the system chose
        SGCL_INLINE_HOT endpoint local_endpoint() const noexcept {
            return _get().local_endpoint();
        }

        // The path of a unix listener, empty for TCP
        SGCL_INLINE_HOT string path() const noexcept {
            return _get().path();
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_impl;
        }

        SGCL_INLINE_HOT friend bool operator==(const listener& a, const listener& b) noexcept {
            return a._impl == b._impl;
        }

    private:
        friend struct detail::ListenerAccess;

        SGCL_INLINE_HOT explicit listener(const tracked_ptr<detail::ListenerImpl>& impl) noexcept
        : _impl(impl) {
        }

        SGCL_INLINE_HOT detail::ListenerImpl& _get() const noexcept {
            assert(_impl && "an empty net::listener");
            return *_impl;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT listener(sgcl::detail::FromWord, const tracked_ptr<detail::ListenerImpl>& w) noexcept
        : _impl(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::ListenerImpl>& _handle_word() noexcept {
            return _impl;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::ListenerImpl>& _handle_word() const noexcept {
            return _impl;
        }

        tracked_ptr<detail::ListenerImpl> _impl;

        // the two halves of the operations above: a thread's and a task's
        SGCL_INLINE_HOT expected<connection, io::error> _block_accept() const {
            return _get()._block_accept();
        }

        SGCL_INLINE_HOT async::task<expected<connection, io::error>> _co_accept() const noexcept {
            return _get()._co_accept();
        }
    };

    namespace detail {
        SGCL_INLINE_HOT listener ListenerAccess::make(const tracked_ptr<ListenerImpl>& impl) noexcept {
            return listener(impl);
        }
    }
}
