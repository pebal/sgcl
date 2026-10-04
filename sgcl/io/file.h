//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "buffered.h"
#include "fs.h"
#include "stream.h"
#include "../async/blocking.h"
#include "../async/reactor.h"
#include "detail/descriptor.h"

#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <pthread.h>
#include <random>
#include <sys/stat.h>
#include <unistd.h>

namespace sgcl::io {
    // How a file is opened: a set of flags, combined with |. read is the
    // default; write alone truncates nothing and creates nothing (an
    // error when the file is not there), so a file to write is opened
    // with write | create | truncate, which `create(path)` spells, or
    // write | create | append for a log. exclusive with create fails
    // when the file exists (O_EXCL); sync makes every write reach the
    // disk before it returns (O_SYNC).
    enum class open_flags : unsigned {
        read = 1,
        write = 2,
        create = 4,
        truncate = 8,
        append = 16,
        exclusive = 32,
        sync = 64
    };

    SGCL_INLINE_HOT constexpr open_flags operator|(open_flags a, open_flags b) noexcept {
        return static_cast<open_flags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    SGCL_INLINE_HOT constexpr bool operator&(open_flags a, open_flags b) noexcept {
        return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
    }

    class file;

    namespace detail {
        class FileState;
        struct FileAccess;
    }

    // A file: one class for every descriptor (a regular file, a pipe, a
    // terminal, later a socket), a stream with a position. A read or
    // write is the system call on the descriptor. The async form of an
    // operation goes one of two ways, chosen when the file is made: a
    // regular file's (and a terminal's) runs the call on the blocking
    // pool (blocking.h: a disk has no readiness to wait for), a
    // non-blocking descriptor's (a pipe from pipe(), a socket) waits for
    // readiness on the reactor (reactor.h) and makes the call when it
    // will not block; the synchronous form on such a descriptor polls
    // instead. read_at and write_at are pread and pwrite: the position
    // given, the file's own untouched, so that tasks share a file
    // without a seek between them. The descriptor is released by
    // close() or, failing that, by the destructor on the collector's
    // thread after the sweep that finds the file dead.
    //
    // The state of a file, the object its handles share (io::file below):
    // the descriptor and what the operations need. Managed; the handle is
    // one tracked word to it.
    //
    // A write to a pipe whose read end is closed is the error EPIPE, never
    // the signal SIGPIPE, which would end the process (net's sockets the
    // same). Where the descriptor has a flag for it (F_SETNOSIGPIPE: macOS,
    // the BSDs), the flag is set when io makes the file and a write costs
    // nothing more; elsewhere (Linux) a write to a pipe or a socket holds
    // the signal back on its thread and takes one it raised off before
    // letting it go, as net's sendfile does. The standard streams keep the
    // signal, as Go's do: `prog | head` ends prog as a shell expects.
    namespace detail {
        // A descriptor io owns from now on: true when its writes must hold
        // the signal back themselves
        SGCL_INLINE_HOT bool no_sigpipe(int fd) noexcept {
#if defined(F_SETNOSIGPIPE)
            (void)::fcntl(fd, F_SETNOSIGPIPE, 1);
            return false;
#else
            struct ::stat st;
            return ::fstat(fd, &st) == 0 && (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode));
#endif
        }

#if !defined(F_SETNOSIGPIPE)
        inline ssize_t write_holding_sigpipe(int fd, const void* data, size_t n) noexcept {
            sigset_t pipe_only;
            sigset_t old;
            sigemptyset(&pipe_only);
            sigaddset(&pipe_only, SIGPIPE);
            pthread_sigmask(SIG_BLOCK, &pipe_only, &old);
            const ssize_t k = ::write(fd, data, n);
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
    }

    namespace detail {
    class FileState final {
    public:
        // `hold_sigpipe`: what no_sigpipe answered, for a descriptor io owns
        SGCL_INLINE_HOT FileState(int fd, const string& name, bool reactor, bool owns, bool hold_sigpipe = false) noexcept
        : _d(fd, owns), _path(std::move(name)), _reactor(reactor) {
#if defined(F_SETNOSIGPIPE)
            (void)hold_sigpipe;
#else
            _hold_sigpipe = hold_sigpipe;
#endif
        }

        expected<size_t, error> read(const slice<byte>& buffer) {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "read", _name()));
            }
            if (buffer.empty()) {
                return 0;
            }
            for (;;) {
                _d.prepare(detail::Descriptor::Read);
                ssize_t n = ::read(_d.fd(), buffer.data(), buffer.size());
                if (n >= 0) {
                    return static_cast<size_t>(n);
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN && _reactor) {
                    if (auto e = _waited(_d.wait(detail::Descriptor::Read), "read")) {
                        return detail::fail(*e);
                    }
                    continue;
                }
                return detail::fail(last_error("read", _name()));
            }
        }

        async::task<expected<size_t, error>> async_read(slice<byte> buffer) noexcept {
            if (!_reactor) {
                co_return co_await detail::read_via_pool(buffer, [self = tracked_ptr<FileState>(this)](const slice<byte>& b) { return self->read(b); });
            }
            detail::Operation op(_d);
            if (!op) {
                co_return detail::fail(error(errc::closed, "read", _name()));
            }
            if (buffer.empty()) {
                co_return 0;
            }
            for (;;) {
                _d.prepare(detail::Descriptor::Read);
                ssize_t n = ::read(_d.fd(), buffer.data(), buffer.size());
                if (n >= 0) {
                    co_return static_cast<size_t>(n);
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    auto r = co_await _d.async_wait(detail::Descriptor::Read);
                    if (auto e = _waited(r, "read")) {
                        co_return detail::fail(*e);
                    }
                    continue;
                }
                co_return detail::fail(last_error("read", _name()));
            }
        }

        expected<size_t, error> write(const slice<const byte>& data) {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "write", _name()));
            }
            size_t written = 0;
            while (written < data.size()) {
                _d.prepare(detail::Descriptor::Write);
                ssize_t n = _write(data.data() + written, data.size() - written);
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                    continue;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN && _reactor) {
                    if (auto e = _waited(_d.wait(detail::Descriptor::Write), "write")) {
                        return detail::fail(*e);
                    }
                    continue;
                }
                return detail::fail(last_error("write", _name()));
            }
            return written;
        }

        async::task<expected<size_t, error>> async_write(slice<const byte> data) noexcept {
            if (!_reactor) {
                data = detail::owned_for_pool(data);   // never plain memory without its owner on the pool
                co_return co_await async::spawn_blocking([self = tracked_ptr<FileState>(this), data] { return self->write(data); });
            }
            detail::Operation op(_d);
            if (!op) {
                co_return detail::fail(error(errc::closed, "write", _name()));
            }
            size_t written = 0;
            while (written < data.size()) {
                _d.prepare(detail::Descriptor::Write);
                ssize_t n = _write(data.data() + written, data.size() - written);
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                    continue;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    auto r = co_await _d.async_wait(detail::Descriptor::Write);
                    if (auto e = _waited(r, "write")) {
                        co_return detail::fail(*e);
                    }
                    continue;
                }
                co_return detail::fail(last_error("write", _name()));
            }
            co_return written;
        }

        expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "seek", _name()));
            }
            int whence = from == seek_from::begin ? SEEK_SET : from == seek_from::current ? SEEK_CUR : SEEK_END;
            off_t r = ::lseek(_d.fd(), static_cast<off_t>(offset), whence);
            if (r < 0) {
                return detail::fail(last_error("seek", _name()));
            }
            return static_cast<uint64_t>(r);
        }

        // Ends the file: no operation starts after it, the waits in
        // progress end with errc::closed, and the descriptor is given back
        // to the kernel by whoever lets go of it last, this call or the
        // last operation in progress, so that none of them lands on the
        // number the kernel gives to the next file opened
        SGCL_INLINE_HOT expected<void, error> close() noexcept {
            if (int e = _d.close()) {
                errno = e;
                return detail::fail(last_error("close", _name()));
            }
            return {};
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _d.closing();
        }

        // `read_at(...)` on this thread, `co_await async_read_at(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> read_at(const slice<byte>& buffer, uint64_t offset) noexcept {
            return _block_read_at(buffer, offset);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_read_at(const slice<byte>& buffer, uint64_t offset) noexcept {
            return _co_read_at(buffer, offset);
        }

        // `write_at(...)` on this thread, `co_await async_write_at(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> write_at(const slice<const byte>& data, uint64_t offset) noexcept {
            return _block_write_at(data, offset);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_write_at(const slice<const byte>& data, uint64_t offset) noexcept {
            return _co_write_at(data, offset);
        }

        // fsync; ftruncate; fstat; fchmod
        SGCL_INLINE_HOT expected<void, error> sync() noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "sync", _name()));
            }
            if (::fsync(_d.fd()) != 0) {
                return detail::fail(last_error("sync", _name()));
            }
            return {};
        }

        SGCL_INLINE_HOT expected<void, error> truncate(uint64_t size) noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "truncate", _name()));
            }
            if (::ftruncate(_d.fd(), static_cast<off_t>(size)) != 0) {
                return detail::fail(last_error("truncate", _name()));
            }
            return {};
        }

        // `sync()` on this thread, `co_await async_sync()` in a task, on the
        // blocking pool (an fsync waits for the disk); truncate likewise
        async::task<expected<void, error>> async_sync() noexcept {
            co_return co_await async::spawn_blocking([self = tracked_ptr<FileState>(this)] { return self->sync(); });
        }

        async::task<expected<void, error>> async_truncate(uint64_t size) noexcept {
            co_return co_await async::spawn_blocking([self = tracked_ptr<FileState>(this), size] { return self->truncate(size); });
        }

        SGCL_INLINE_HOT expected<file_info, error> stat() const noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "stat", _name()));
            }
            struct ::stat st;
            if (::fstat(_d.fd(), &st) != 0) {
                return detail::fail(last_error("stat", _name()));
            }
            return detail::info_of(st, _name());
        }

        SGCL_INLINE_HOT expected<void, error> chmod(permissions p) noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "chmod", _name()));
            }
            if (::fchmod(_d.fd(), static_cast<mode_t>(p)) != 0) {
                return detail::fail(last_error("chmod", _name()));
            }
            return {};
        }

        // `chmod(p)` on this thread, `co_await async_chmod(p)` in a task, on
        // the blocking pool as async_sync
        async::task<expected<void, error>> async_chmod(permissions p) noexcept {
            co_return co_await async::spawn_blocking([self = tracked_ptr<FileState>(this), p] { return self->chmod(p); });
        }

        // The descriptor, -1 when closed; the path it was opened with,
        // or the name given to from_fd
        SGCL_INLINE_HOT int fd() const noexcept {
            return _d.closing() ? -1 : _d.fd();
        }

        SGCL_INLINE_HOT const string& path() const noexcept {
            return _path;
        }

        // Whether the async operations wait on the reactor (a
        // non-blocking descriptor) rather than run on the blocking pool
        SGCL_INLINE_HOT bool is_nonblocking() const noexcept {
            return _reactor;
        }

    private:
        SGCL_INLINE_HOT const string& _name() const noexcept {
            return _path;
        }

        SGCL_INLINE_HOT ssize_t _write(const byte* data, size_t n) noexcept {
#if !defined(F_SETNOSIGPIPE)
            if (_hold_sigpipe) {
                return detail::write_holding_sigpipe(_d.fd(), data, n);
            }
#endif
            return ::write(_d.fd(), data, n);
        }

        // The error of a wait that did not end in readiness: the file
        // closed under it, or the reactor stopped (scheduler::stop)
        SGCL_INLINE_HOT optional<error> _waited(detail::WaitResult r, const char* op) const noexcept {
            if (r == detail::WaitResult::ready) {
                return nullopt;
            }
            if (r == detail::WaitResult::closed) {
                return error(errc::closed, op, _name());
            }
            if (r == detail::WaitResult::failed) {
                return error(_d.wait_failure(), op, _name());
            }
            return error(error_code(ECANCELED, std::system_category()), op, _name());
        }

        mutable detail::Descriptor _d;   // the number, held by every operation (detail/descriptor.h)
        string _path;
        bool _reactor;
#if !defined(F_SETNOSIGPIPE)
        bool _hold_sigpipe;   // a pipe or a socket: the write holds SIGPIPE back
#endif

        // the two halves of the operations above: a thread's and a task's
        expected<size_t, error> _block_read_at(const slice<byte>& buffer, uint64_t offset) noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "read_at", _name()));
            }
            for (;;) {
                ssize_t n = ::pread(_d.fd(), buffer.data(), buffer.size(), static_cast<off_t>(offset));
                if (n >= 0) {
                    return static_cast<size_t>(n);
                }
                if (errno != EINTR) {
                    return detail::fail(last_error("read_at", _name()));
                }
            }
        }

        expected<size_t, error> _block_write_at(const slice<const byte>& data, uint64_t offset) noexcept {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "write_at", _name()));
            }
            size_t written = 0;
            while (written < data.size()) {
                ssize_t n = ::pwrite(_d.fd(), data.data() + written, data.size() - written, static_cast<off_t>(offset + written));
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                } else if (errno != EINTR) {
                    return detail::fail(last_error("write_at", _name()));
                }
            }
            return written;
        }

        async::task<expected<size_t, error>> _co_read_at(slice<byte> buffer, uint64_t offset) noexcept {
            co_return co_await detail::read_via_pool(buffer, [self = tracked_ptr<FileState>(this), offset](const slice<byte>& b) { return self->_block_read_at(b, offset); });
        }

        async::task<expected<size_t, error>> _co_write_at(slice<const byte> data, uint64_t offset) noexcept {
            data = detail::owned_for_pool(data);   // never plain memory without its owner on the pool
            co_return co_await async::spawn_blocking([self = tracked_ptr<FileState>(this), data, offset] { return self->_block_write_at(data, offset); });
        }
    };
    }

    // A file as a handle: one tracked word to the state above, copied and
    // passed by value, the copies sharing one file (as io::reader and
    // net::connection do). A handle is a tracked word: on a stack, in a
    // task, in a managed object; in a global or a std container, a
    // root_ptr to it, as to any managed object. Made by open, create,
    // temp_file, pipe and from_fd; a default-constructed one holds no file
    // (`!f`), and an operation on it is a contract violation. As an
    // io::reader or io::writer it is the state that is bound, not the
    // handle: the stream lives as long as the reader does.
    class file final : public mixin::reader<file>, public mixin::writer<file>, public mixin::seeker<file> {
    public:
        using mixin::writer<file>::write;
        using mixin::writer<file>::async_write;

        file() noexcept = default;

        // `read(...)` on this thread, `co_await async_read(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> read(const slice<byte>& buffer) const {
            return _get().read(buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const noexcept {
            return _get().async_read(buffer);
        }

        // `write(...)` on this thread, `co_await async_write(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> write(const slice<const byte>& data) const {
            return _get().write(data);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const noexcept {
            return _get().async_write(data);
        }

        SGCL_INLINE_HOT expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) const noexcept {
            return _get().seek(offset, from);
        }

        // Ends the file: no operation starts after it, the waits in
        // progress end with errc::closed, and the descriptor is given back
        // to the kernel by whoever lets go of it last, this call or the
        // last operation in progress
        SGCL_INLINE_HOT expected<void, error> close() const noexcept {
            return _get().close();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // pread and pwrite: the position given, the file's own untouched
        // `read_at(...)` on this thread, `co_await async_read_at(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> read_at(const slice<byte>& buffer, uint64_t offset) const noexcept {
            return _get().read_at(buffer, offset);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_read_at(const slice<byte>& buffer, uint64_t offset) const noexcept {
            return _get().async_read_at(buffer, offset);
        }

        // `write_at(...)` on this thread, `co_await async_write_at(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> write_at(const slice<const byte>& data, uint64_t offset) const noexcept {
            return _get().write_at(data, offset);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_write_at(const slice<const byte>& data, uint64_t offset) const noexcept {
            return _get().async_write_at(data, offset);
        }

        // fsync; ftruncate; fstat; fchmod. `sync()` on this thread,
        // `co_await async_sync()` in a task, on the blocking pool;
        // truncate and chmod likewise
        SGCL_INLINE_HOT expected<void, error> sync() const noexcept {
            return _get().sync();
        }

        SGCL_INLINE_HOT async::task<expected<void, error>> async_sync() const noexcept {
            return _get().async_sync();
        }

        SGCL_INLINE_HOT expected<void, error> truncate(uint64_t size) const noexcept {
            return _get().truncate(size);
        }

        SGCL_INLINE_HOT async::task<expected<void, error>> async_truncate(uint64_t size) const noexcept {
            return _get().async_truncate(size);
        }

        SGCL_INLINE_HOT expected<file_info, error> stat() const noexcept {
            return _get().stat();
        }

        SGCL_INLINE_HOT expected<void, error> chmod(permissions p) const noexcept {
            return _get().chmod(p);
        }

        SGCL_INLINE_HOT async::task<expected<void, error>> async_chmod(permissions p) const noexcept {
            return _get().async_chmod(p);
        }

        // The descriptor, -1 when closed; the path it was opened with,
        // or the name given to from_fd
        SGCL_INLINE_HOT int fd() const noexcept {
            return _get().fd();
        }

        SGCL_INLINE_HOT const string& path() const noexcept {
            return _get().path();
        }

        // Whether the async operations wait on the reactor (a
        // non-blocking descriptor) rather than run on the blocking pool
        SGCL_INLINE_HOT bool is_nonblocking() const noexcept {
            return _get().is_nonblocking();
        }

        // Whether this handle holds a file
        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_state;
        }

        // The same file: the same state
        SGCL_INLINE_HOT friend bool operator==(const file& a, const file& b) noexcept {
            return a._state == b._state;
        }

    private:
        friend struct detail::FileAccess;
        friend struct detail::HandleAccess;

        SGCL_INLINE_HOT explicit file(tracked_ptr<detail::FileState> state) noexcept
        : _state(std::move(state)) {
        }

        SGCL_INLINE_HOT detail::FileState& _get() const noexcept {
            assert(_state && "an empty io::file");
            return *_state;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::FileState>& _stream_state() const noexcept {
            return _state;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT file(sgcl::detail::FromWord, const tracked_ptr<detail::FileState>& w) noexcept
        : _state(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::FileState>& _handle_word() noexcept {
            return _state;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::FileState>& _handle_word() const noexcept {
            return _state;
        }

        tracked_ptr<detail::FileState> _state;
    };

    namespace detail {
        template<>
        inline constexpr bool IsStreamHandle<file> = true;

        // The handle made over a state, and the state under a handle, for
        // the library's own code (exec, the standard streams, compress)
        struct FileAccess {
            SGCL_INLINE_HOT static file make(tracked_ptr<FileState> state) noexcept {
                return file(std::move(state));
            }

            SGCL_INLINE_HOT static const tracked_ptr<FileState>& state(const file& f) noexcept {
                return f._state;
            }
        };
    }

    namespace detail {
        // Whether the descriptor has O_NONBLOCK set
        SGCL_INLINE_HOT bool is_nonblocking_fd(int fd) noexcept {
            int f = ::fcntl(fd, F_GETFL);
            return f >= 0 && (f & O_NONBLOCK);
        }

        SGCL_INLINE_HOT bool set_nonblocking_fd(int fd) noexcept {
            int f = ::fcntl(fd, F_GETFL);
            return f >= 0 && ::fcntl(fd, F_SETFL, f | O_NONBLOCK) == 0;
        }

        // The state of a file over a standard descriptor, which it never
        // closes (os.h: io::stdin, io::stdout, io::stderr)
        SGCL_INLINE_HOT tracked_ptr<FileState> std_stream(int fd, const string& name) noexcept {
            return tracked_ptr<FileState>(make_tracked<FileState>(fd, name, is_nonblocking_fd(fd), false));
        }
    }

    // Opens a file: its handle, or the error (is_not_found(),
    // is_permission(), is_exists() with exclusive). A regular file and
    // a terminal are blocking (their async operations use the pool); a
    // FIFO or a device is made non-blocking and served by the reactor.
    inline expected<file, error> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666)) noexcept {
        int f = O_CLOEXEC;
        bool r = flags & open_flags::read, w = flags & open_flags::write;
        f |= (r && w) ? O_RDWR : w ? O_WRONLY : O_RDONLY;
        if (flags & open_flags::create) f |= O_CREAT;
        if (flags & open_flags::truncate) f |= O_TRUNC;
        if (flags & open_flags::append) f |= O_APPEND;
        if (flags & open_flags::exclusive) f |= O_EXCL;
        if (flags & open_flags::sync) f |= O_SYNC;
        int fd;
        do {
            fd = ::open(path.c_str(), f, static_cast<mode_t>(p));
        } while (fd < 0 && errno == EINTR);
        if (fd < 0) {
            return detail::fail(last_error("open", path));
        }
        struct ::stat st;
        bool reactor = false;
        bool hold_sigpipe = false;
        if (::fstat(fd, &st) == 0 && !S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && !::isatty(fd)) {
            reactor = detail::set_nonblocking_fd(fd);
            hold_sigpipe = detail::no_sigpipe(fd);   // a FIFO: EPIPE, not SIGPIPE, once its reader is gone
        }
        return detail::FileAccess::make(make_tracked<detail::FileState>(fd, path, reactor, true, hold_sigpipe));
    }

    // open(path, write | create | truncate, p)
    SGCL_INLINE_HOT expected<file, error> create(const string& path, permissions p = permissions(0666)) noexcept {
        return open(path, open_flags::write | open_flags::create | open_flags::truncate, p);
    }

    // `open(...)` on this thread, `co_await async_open(...)` in a task, on
    // the blocking pool: an open waits for the disk, and one of a FIFO for
    // the other end; create likewise
    SGCL_INLINE_HOT async::task<expected<file, error>> async_open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666)) noexcept {
        return detail::on_pool([path, flags, p] { return open(path, flags, p); });
    }

    SGCL_INLINE_HOT async::task<expected<file, error>> async_create(const string& path, permissions p = permissions(0666)) noexcept {
        return detail::on_pool([path, p] { return create(path, p); });
    }

    // A file over a descriptor opened elsewhere (a pipe from exec, a
    // socket from net), which the file owns from now on. The
    // descriptor's flags are left as they are, but for SIGPIPE (a write
    // without a reader is EPIPE): one that is non-blocking already is
    // served by the reactor, any other by the pool.
    SGCL_INLINE_HOT file from_fd(int fd, const string& name = {}) noexcept {
        const bool hold_sigpipe = detail::no_sigpipe(fd);
        return detail::FileAccess::make(make_tracked<detail::FileState>(fd, name, detail::is_nonblocking_fd(fd), true, hold_sigpipe));
    }

    // The two ends of a pipe, by name: `ends.read`, `ends.write`, or
    // `auto [r, w] = io::pipe().value()`
    struct pipe_ends {
        file read;    // what is written to the other end is read here
        file write;
    };

    // An anonymous pipe: what is written to the write end is read from
    // the read end; both non-blocking, served by the reactor; a write
    // after the read end is closed is EPIPE, not SIGPIPE
    inline expected<pipe_ends, error> pipe() noexcept {
        int fds[2];
        if (::pipe(fds) != 0) {
            return detail::fail(last_error("pipe"));
        }
        for (int fd : fds) {
            ::fcntl(fd, F_SETFD, FD_CLOEXEC);
            detail::set_nonblocking_fd(fd);
        }
        return pipe_ends{from_fd(fds[0], "pipe"), from_fd(fds[1], "pipe")};
    }

    // The whole file in one call: the size from fstat, one buffer of
    // that size, one read (and more, should the file have grown); text
    // as a string
    namespace detail {
    // The bytes of a file opened already, from its start, of the size
    // fstat gives (read_file, and read_text for a size it does not read
    // into a string at once): the open file, never the path again, which
    // may name another file by now, or a FIFO whose writer is gone
    inline expected<vector<byte>, error> _block_read_open_file(const file& f, const string& path)  {
        // the size alone: stat() would make the file_info's name, a string
        // nobody reads here
        struct ::stat st;
        if (::fstat(f.fd(), &st) != 0) {
            return detail::fail(last_error("stat", path));
        }
        vector<byte> out;
        out.resize(static_cast<size_t>(st.st_size));
        size_t got = 0;   // a file that shrank since the stat gives what it has: read, not read_full
        while (got < out.size()) {
            auto n = f.read(out.as_slice().subspan(got));
            if (!n) {
                return detail::fail(n);
            }
            if (*n == 0) {
                break;
            }
            got += *n;
        }
        out.resize(got);
        if (got == static_cast<size_t>(st.st_size)) {
            // one byte more, into the stack, tells whether the file grew
            // since the stat; only then is the rest read to its end
            byte probe;
            auto n = f.read(slice<byte>(&probe, 1));
            if (!n) {
                return detail::fail(n);
            }
            if (*n) {
                out.push_back(probe);
                auto more = io::read_all(f);
                if (!more) {
                    return detail::fail(more);
                }
                out.insert(out.end(), more->begin(), more->end());
            }
        }
        return out;
    }

    SGCL_INLINE_HOT expected<vector<byte>, error> _block_read_file(const string& path)  {
        auto f = open(path);
        if (!f) {
            return detail::fail(f);
        }
        auto r = _block_read_open_file(*f, path);
        (void)f->close();
        return r;
    }
    }

    namespace detail {
    // The text read straight into the string's object, of the size fstat
    // gives (a vector first and a string of it was two of each); a file of
    // size 0 (a FIFO, a file of /proc) is read the vector's way, which
    // reads on to its end, and one that grew since the stat is read on to
    // its end behind what was read: through the file opened, never the
    // path again
    inline expected<string, error> _block_read_text(const string& path)  {
        auto f = open(path);
        if (!f) {
            return detail::fail(f);
        }
        struct ::stat st;
        if (::fstat(f->fd(), &st) != 0) {
            return detail::fail(last_error("stat", path));
        }
        const size_t size = static_cast<size_t>(st.st_size);
        if (size == 0 || size > string::max_size()) {
            auto r = _block_read_open_file(*f, path);
            (void)f->close();
            if (!r) {
                return detail::fail(r);
            }
            return detail::text_of(as_bytes(r->as_slice()));
        }
        auto room = sgcl::detail::StringAccess::unfilled<string>(size);
        size_t got = 0;   // a file that shrank since the stat gives what it has
        while (got < size) {
            auto n = f->read(slice<byte>(reinterpret_cast<byte*>(room.chars) + got, size - got));
            if (!n) {
                return detail::fail(n);
            }
            if (*n == 0) {
                break;
            }
            got += *n;
        }
        if (got == size) {
            byte probe;
            auto n = f->read(slice<byte>(&probe, 1));
            if (!n) {
                return detail::fail(n);
            }
            if (*n) {
                // it grew: the rest read to its end, the text made of the three
                auto rest = io::read_all(*f);
                (void)f->close();
                if (!rest) {
                    return detail::fail(rest);
                }
                return sgcl::detail::StringAccess::filled<string>(size + 1 + rest->size(), [&](char* chars) {
                    sgcl::detail::copy_bytes(chars, room.chars, size);
                    chars[size] = char(probe);
                    sgcl::detail::copy_bytes(chars + size + 1, rest->data(), rest->size());
                });
            }
        }
        (void)f->close();
        return sgcl::detail::StringAccess::finish<string>(std::move(room), got);
    }
    }

    namespace detail {
    inline async::task<expected<vector<byte>, error>> _co_read_file(string path) noexcept {   // by value: a task is lazy, the caller's string may be gone before it runs
        co_return co_await async::spawn_blocking([path] { return _block_read_file(path); });
    }
    }

    // `read_file(...)` on this thread, `co_await async_read_file(...)` in a task
    SGCL_INLINE_HOT expected<vector<byte>, error> read_file(const string& path) {
        return detail::_block_read_file(path);
    }

    SGCL_INLINE_HOT async::task<expected<vector<byte>, error>> async_read_file(const string& path) noexcept {
        return detail::_co_read_file(path);
    }

    namespace detail {
    inline async::task<expected<string, error>> _co_read_text(string path) noexcept {
        co_return co_await async::spawn_blocking([path] { return _block_read_text(path); });
    }
    }

    // `read_text(...)` on this thread, `co_await async_read_text(...)` in a task
    SGCL_INLINE_HOT expected<string, error> read_text(const string& path) {
        return detail::_block_read_text(path);
    }

    SGCL_INLINE_HOT async::task<expected<string, error>> async_read_text(const string& path) noexcept {
        return detail::_co_read_text(path);
    }

    // The lines of a text file in one call, by buffered_reader's read_line:
    // each without its "\n" and a "\r" before it, the last one also without
    // an end, none after a final "\n". The file is read a block at a time,
    // never held whole: what stays is the lines. The error of the open or
    // of a read, the lines read before it dropped, as read_text does
    namespace detail {
    inline expected<vector<string>, error> _block_read_lines(const string& path) {
        auto f = open(path);
        if (!f) {
            return detail::fail(f);
        }
        buffered_reader in(*f);
        vector<string> lines;
        for (;;) {
            auto line = in.read_line();
            if (!line) {
                (void)f->close();
                return detail::fail(line);
            }
            if (!*line) {
                break;
            }
            lines.emplace_back(**line);
        }
        (void)f->close();
        return lines;
    }

    inline async::task<expected<vector<string>, error>> _co_read_lines(string path) noexcept {   // by value: a task is lazy
        co_return co_await async::spawn_blocking([path] { return _block_read_lines(path); });
    }
    }

    // `read_lines(...)` on this thread, `co_await async_read_lines(...)` in a task
    SGCL_INLINE_HOT expected<vector<string>, error> read_lines(const string& path) {
        return detail::_block_read_lines(path);
    }

    SGCL_INLINE_HOT async::task<expected<vector<string>, error>> async_read_lines(const string& path) noexcept {
        return detail::_co_read_lines(path);
    }

    // The whole file written in one call: created or truncated, the
    // bytes stored, closed (the close's error reported too); append_file
    // adds them at the end, creating the file when it is not there. The
    // async forms run on the pool, which is never given plain memory
    // without its owner: bytes whose slice has none are copied into a
    // managed block when the task starts, text is a string the task holds.
    namespace detail {
    inline expected<void, error> _block_write_file(const string& path, const slice<const byte>& data, permissions p) {
        auto f = create(path, p);
        if (!f) {
            return detail::fail(f);
        }
        auto w = f->write(data);
        auto c = f->close();
        if (!w) {
            return detail::fail(w);
        }
        return c;
    }
    }

    namespace detail {
    SGCL_INLINE_HOT expected<void, error> _block_write_file(const string& path, const string& text, permissions p) {
        return _block_write_file(path, detail::bytes_of(text), p);
    }
    }

    namespace detail {
    inline expected<void, error> _block_append_file(const string& path, const slice<const byte>& data, permissions p) {
        auto f = open(path, open_flags::write | open_flags::create | open_flags::append, p);
        if (!f) {
            return detail::fail(f);
        }
        auto w = f->write(data);
        auto c = f->close();
        if (!w) {
            return detail::fail(w);
        }
        return c;
    }
    }

    namespace detail {
    SGCL_INLINE_HOT expected<void, error> _block_append_file(const string& path, const string& text, permissions p) {
        return _block_append_file(path, detail::bytes_of(text), p);
    }
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_write_file(string path, slice<const byte> data, permissions p) noexcept {
        data = detail::owned_for_pool(data);   // never plain memory without its owner on the pool
        co_return co_await async::spawn_blocking([path, data, p] { return _block_write_file(path, data, p); });
    }
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_write_file(string path, string text, permissions p) noexcept {
        co_return co_await async::spawn_blocking([path, text, p] { return _block_write_file(path, text, p); });
    }
    }

    // `io::write_file(...)` on this thread, `co_await io::async_write_file(...)` in a task
    SGCL_INLINE_HOT expected<void, error> write_file(const string& path, const string& text, permissions p = permissions(0666)) {
        return detail::_block_write_file(path, text, p);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> async_write_file(const string& path, const string& text, permissions p = permissions(0666)) noexcept {
        return detail::_co_write_file(path, text, p);
    }

    // `io::write_file(...)` on this thread, `co_await io::async_write_file(...)` in a task
    SGCL_INLINE_HOT expected<void, error> write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666)) {
        return detail::_block_write_file(path, data, p);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> async_write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666)) noexcept {
        return detail::_co_write_file(path, data, p);
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_append_file(string path, slice<const byte> data, permissions p) noexcept {
        data = detail::owned_for_pool(data);   // never plain memory without its owner on the pool
        co_return co_await async::spawn_blocking([path, data, p] { return _block_append_file(path, data, p); });
    }
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_append_file(string path, string text, permissions p) noexcept {
        co_return co_await async::spawn_blocking([path, text, p] { return _block_append_file(path, text, p); });
    }
    }

    // `io::append_file(...)` on this thread, `co_await io::async_append_file(...)` in a task
    SGCL_INLINE_HOT expected<void, error> append_file(const string& path, const string& text, permissions p = permissions(0666)) {
        return detail::_block_append_file(path, text, p);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> async_append_file(const string& path, const string& text, permissions p = permissions(0666)) noexcept {
        return detail::_co_append_file(path, text, p);
    }

    // `io::append_file(...)` on this thread, `co_await io::async_append_file(...)` in a task
    SGCL_INLINE_HOT expected<void, error> append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666)) {
        return detail::_block_append_file(path, data, p);
    }

    SGCL_INLINE_HOT async::task<expected<void, error>> async_append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666)) noexcept {
        return detail::_co_append_file(path, data, p);
    }

    // A literal, a character array, a std::string_view: as a string (an
    // exact match, else the conversions to a string and to bytes tie); the
    // async forms copy the text, which the task then holds
    template<sgcl::detail::TextArgument T>
    SGCL_INLINE_HOT expected<void, error> write_file(const string& path, const T& text, permissions p = permissions(0666)) {
        return detail::_block_write_file(path, slice<const byte>(text), p);
    }

    template<sgcl::detail::TextArgument T>
    SGCL_INLINE_HOT async::task<expected<void, error>> async_write_file(const string& path, const T& text, permissions p = permissions(0666)) {
        return detail::_co_write_file(path, string(slice<const byte>(text)), p);
    }

    template<sgcl::detail::TextArgument T>
    SGCL_INLINE_HOT expected<void, error> append_file(const string& path, const T& text, permissions p = permissions(0666)) {
        return detail::_block_append_file(path, slice<const byte>(text), p);
    }

    template<sgcl::detail::TextArgument T>
    SGCL_INLINE_HOT async::task<expected<void, error>> async_append_file(const string& path, const T& text, permissions p = permissions(0666)) {
        return detail::_co_append_file(path, string(slice<const byte>(text)), p);
    }


    namespace detail {
        // The system's temporary directory: $TMPDIR, else /tmp
        SGCL_INLINE_HOT string temp_root() noexcept {
            const char* t = ::getenv("TMPDIR");
            if (t && *t) {
                return io::path::clean(string(t));
            }
            return string("/tmp");
        }

        // The pattern with its last '*' (or its end) replaced by random
        // characters
        inline string temp_name(const string& pattern) {
            static thread_local std::mt19937_64 rng{std::random_device{}()};
            static constexpr char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
            std::string r;
            uint64_t v = rng();
            for (int i = 0; i < 10; ++i) {
                r += digits[v % 36];
                v /= 36;
            }
            std::string name = pattern.str();
            auto star = name.rfind('*');
            if (star == std::string::npos) {
                return string(name + r);
            }
            return string(name.replace(star, 1, r));
        }
    }

    // A new file in dir (the system's temporary directory when empty)
    // with a name from the pattern, "*" in it replaced by a random
    // string ("upload-*.tmp"), opened for reading and writing, 0600;
    // make_temp_dir the same for a directory, 0700 (Go's os.MkdirTemp;
    // temp_dir() is the system's directory itself). The caller removes it.
    inline expected<file, error> temp_file(const string& dir = {}, const string& pattern = "*") {
        string root = dir.empty() ? detail::temp_root() : dir;
        for (int attempt = 0; attempt < 10000; ++attempt) {
            auto p = io::path::join(root, detail::temp_name(pattern));
            auto f = open(p, open_flags::read | open_flags::write | open_flags::create | open_flags::exclusive, permissions(0600));
            if (f || !f.error().is_exists()) {
                return f;
            }
        }
        return detail::fail(error(std::make_error_code(std::errc::file_exists), "temp_file", pattern));
    }

    inline expected<string, error> make_temp_dir(const string& dir = {}, const string& pattern = "*") {
        string root = dir.empty() ? detail::temp_root() : dir;
        for (int attempt = 0; attempt < 10000; ++attempt) {
            auto p = io::path::join(root, detail::temp_name(pattern));
            auto r = mkdir(p, permissions(0700));
            if (r) {
                return p;
            }
            if (!r.error().is_exists()) {
                return detail::fail(r);
            }
        }
        return detail::fail(error(std::make_error_code(std::errc::file_exists), "make_temp_dir", pattern));
    }

    // `temp_file(...)` on this thread, `co_await async_temp_file(...)` in a
    // task, on the blocking pool, as create; make_temp_dir likewise
    SGCL_INLINE_HOT async::task<expected<file, error>> async_temp_file(const string& dir = {}, const string& pattern = "*") noexcept {
        return detail::on_pool([dir, pattern] { return temp_file(dir, pattern); });
    }

    SGCL_INLINE_HOT async::task<expected<string, error>> async_make_temp_dir(const string& dir = {}, const string& pattern = "*") noexcept {
        return detail::on_pool([dir, pattern] { return make_temp_dir(dir, pattern); });
    }
}
