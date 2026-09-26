//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "fs.h"
#include "stream.h"
#include "../async/blocking.h"
#include "../async/reactor.h"
#include "detail/descriptor.h"

#include <cstdint>
#include <fcntl.h>
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

    constexpr open_flags operator|(open_flags a, open_flags b) noexcept {
        return static_cast<open_flags>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    constexpr bool operator&(open_flags a, open_flags b) noexcept {
        return (static_cast<unsigned>(a) & static_cast<unsigned>(b)) != 0;
    }

    class file;
    expected<tracked_ptr<file>, error> open(const string& path, open_flags flags, permissions p);
    tracked_ptr<file> from_fd(int fd, const string& name);

    namespace detail {
        tracked_ptr<file> std_stream(int fd, const string& name);
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
    class file final : public mixin::reader<file>, public mixin::writer<file>, public mixin::seeker<file> {
        friend class sgcl::detail::MakerBase;   // make_tracked constructs a file here, for the three below
        friend expected<tracked_ptr<file>, error> open(const string&, open_flags, permissions);
        friend tracked_ptr<file> from_fd(int, const string&);
        friend tracked_ptr<file> detail::std_stream(int, const string&);

        file(int fd, const string& name, bool reactor, bool owns) noexcept
        : _d(fd, owns), _path(std::move(name)), _reactor(reactor) {
        }

    public:
        using mixin::writer<file>::write;
        using mixin::writer<file>::async_write;

        expected<size_t, error> read(const slice<byte>& buffer) {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "read", _name()));
            }
            if (buffer.empty()) {
                return 0;
            }
            for (;;) {
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

        async::task<expected<size_t, error>> async_read(slice<byte> buffer) {
            if (!_reactor) {
                co_return co_await async::spawn_blocking([this, buffer] { return read(buffer); });
            }
            detail::Operation op(_d);
            if (!op) {
                co_return detail::fail(error(errc::closed, "read", _name()));
            }
            if (buffer.empty()) {
                co_return 0;
            }
            for (;;) {
                ssize_t n = ::read(_d.fd(), buffer.data(), buffer.size());
                if (n >= 0) {
                    co_return static_cast<size_t>(n);
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    auto w = _d.begin_wait(detail::Descriptor::Read);
                    auto r = w.done ? w.result : _d.end_wait(w, co_await w.channel->receive());
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
                ssize_t n = ::write(_d.fd(), data.data() + written, data.size() - written);
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

        async::task<expected<size_t, error>> async_write(slice<const byte> data) {
            if (!_reactor) {
                co_return co_await async::spawn_blocking([this, data] { return write(data); });
            }
            detail::Operation op(_d);
            if (!op) {
                co_return detail::fail(error(errc::closed, "write", _name()));
            }
            size_t written = 0;
            while (written < data.size()) {
                ssize_t n = ::write(_d.fd(), data.data() + written, data.size() - written);
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                    continue;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    auto w = _d.begin_wait(detail::Descriptor::Write);
                    auto r = w.done ? w.result : _d.end_wait(w, co_await w.channel->receive());
                    if (auto e = _waited(r, "write")) {
                        co_return detail::fail(*e);
                    }
                    continue;
                }
                co_return detail::fail(last_error("write", _name()));
            }
            co_return written;
        }

        expected<uint64_t, error> seek(int64_t offset, seek_from from = seek_from::begin) {
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
        expected<void, error> close() {
            if (int e = _d.close()) {
                errno = e;
                return detail::fail(last_error("close", _name()));
            }
            return {};
        }

        bool is_closed() const noexcept {
            return _d.closing();
        }

        // `read_at(...)` on this thread, `co_await async_read_at(...)` in a task
        expected<size_t, error> read_at(const slice<byte>& buffer, uint64_t offset) {
            return _block_read_at(buffer, offset);
        }

        async::task<expected<size_t, error>> async_read_at(const slice<byte>& buffer, uint64_t offset) {
            return _co_read_at(buffer, offset);
        }

        // `write_at(...)` on this thread, `co_await async_write_at(...)` in a task
        expected<size_t, error> write_at(const slice<const byte>& data, uint64_t offset) {
            return _block_write_at(data, offset);
        }

        async::task<expected<size_t, error>> async_write_at(const slice<const byte>& data, uint64_t offset) {
            return _co_write_at(data, offset);
        }

        // fsync; ftruncate; fstat; fchmod
        expected<void, error> sync() {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "sync", _name()));
            }
            if (::fsync(_d.fd()) != 0) {
                return detail::fail(last_error("sync", _name()));
            }
            return {};
        }

        expected<void, error> truncate(uint64_t size) {
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
        async::task<expected<void, error>> async_sync() {
            co_return co_await async::spawn_blocking([this] { return sync(); });
        }

        async::task<expected<void, error>> async_truncate(uint64_t size) {
            co_return co_await async::spawn_blocking([this, size] { return truncate(size); });
        }

        expected<file_info, error> stat() const {
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

        expected<void, error> chmod(permissions p) {
            detail::Operation op(_d);
            if (!op) {
                return detail::fail(error(errc::closed, "chmod", _name()));
            }
            if (::fchmod(_d.fd(), static_cast<mode_t>(p)) != 0) {
                return detail::fail(last_error("chmod", _name()));
            }
            return {};
        }

        // The descriptor, -1 when closed; the path it was opened with,
        // or the name given to from_fd
        int fd() const noexcept {
            return _d.closing() ? -1 : _d.fd();
        }

        const string& path() const noexcept {
            return _path;
        }

        // Whether the async operations wait on the reactor (a
        // non-blocking descriptor) rather than run on the blocking pool
        bool is_nonblocking() const noexcept {
            return _reactor;
        }

    private:
        const string& _name() const noexcept {
            return _path;
        }

        // The error of a wait that did not end in readiness: the file
        // closed under it, or the reactor stopped (scheduler::stop)
        optional<error> _waited(detail::WaitResult r, const char* op) const {
            if (r == detail::WaitResult::ready) {
                return nullopt;
            }
            if (r == detail::WaitResult::closed) {
                return error(errc::closed, op, _name());
            }
            return error(error_code(ECANCELED, std::system_category()), op, _name());
        }

        mutable detail::Descriptor _d;   // the number, held by every operation (detail/descriptor.h)
        string _path;
        bool _reactor;

        // the two halves of the operations above: a thread's and a task's
        expected<size_t, error> _block_read_at(const slice<byte>& buffer, uint64_t offset)  {
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

        expected<size_t, error> _block_write_at(const slice<const byte>& data, uint64_t offset)  {
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

        async::task<expected<size_t, error>> _co_read_at(slice<byte> buffer, uint64_t offset)  {
            co_return co_await async::spawn_blocking([this, buffer, offset] { return _block_read_at(buffer, offset); });
        }

        async::task<expected<size_t, error>> _co_write_at(slice<const byte> data, uint64_t offset)  {
            co_return co_await async::spawn_blocking([this, data, offset] { return _block_write_at(data, offset); });
        }
    };

    namespace detail {
        // Whether the descriptor has O_NONBLOCK set
        inline bool is_nonblocking_fd(int fd) noexcept {
            int f = ::fcntl(fd, F_GETFL);
            return f >= 0 && (f & O_NONBLOCK);
        }

        inline bool set_nonblocking_fd(int fd) noexcept {
            int f = ::fcntl(fd, F_GETFL);
            return f >= 0 && ::fcntl(fd, F_SETFL, f | O_NONBLOCK) == 0;
        }

        inline tracked_ptr<file> std_stream(int fd, const string& name) {
            return tracked_ptr<file>(make_tracked<file>(fd, name, is_nonblocking_fd(fd), false));
        }
    }

    // Opens a file: a managed file, or the error (is_not_found(),
    // is_permission(), is_exists() with exclusive). A regular file and
    // a terminal are blocking (their async operations use the pool); a
    // FIFO or a device is made non-blocking and served by the reactor.
    inline expected<tracked_ptr<file>, error> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666)) {
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
        if (::fstat(fd, &st) == 0 && !S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && !::isatty(fd)) {
            reactor = detail::set_nonblocking_fd(fd);
        }
        return tracked_ptr<file>(make_tracked<file>(fd, path, reactor, true));
    }

    // open(path, write | create | truncate, p)
    inline expected<tracked_ptr<file>, error> create(const string& path, permissions p = permissions(0666)) {
        return open(path, open_flags::write | open_flags::create | open_flags::truncate, p);
    }

    // `open(...)` on this thread, `co_await async_open(...)` in a task, on
    // the blocking pool: an open waits for the disk, and one of a FIFO for
    // the other end; create likewise
    inline async::task<expected<tracked_ptr<file>, error>> async_open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666)) {
        return detail::on_pool([path, flags, p] { return open(path, flags, p); });
    }

    inline async::task<expected<tracked_ptr<file>, error>> async_create(const string& path, permissions p = permissions(0666)) {
        return detail::on_pool([path, p] { return create(path, p); });
    }

    // A file over a descriptor opened elsewhere (a pipe from exec, a
    // socket from net), which the file owns from now on. The
    // descriptor's flags are left as they are: one that is non-blocking
    // already is served by the reactor, any other by the pool.
    inline tracked_ptr<file> from_fd(int fd, const string& name = {}) {
        return tracked_ptr<file>(make_tracked<file>(fd, name, detail::is_nonblocking_fd(fd), true));
    }

    // The two ends of a pipe, by name: `p->read`, `p->write`, or
    // `auto [r, w] = *p`
    struct pipe_ends {
        tracked_ptr<file> read;    // what is written to the other end is read here
        tracked_ptr<file> write;
    };

    // An anonymous pipe: what is written to the write end is read from
    // the read end; both non-blocking, served by the reactor
    inline expected<pipe_ends, error> pipe() {
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
    inline expected<vector<byte>, error> _block_read_file(const string& path)  {
        auto f = open(path);
        if (!f) {
            return detail::fail(f);
        }
        auto info = (*f)->stat();
        if (!info) {
            return detail::fail(info);
        }
        vector<byte> out;
        out.resize(static_cast<size_t>(info->size));
        size_t got = 0;   // a file that shrank since the stat gives what it has: read, not read_full
        while (got < out.size()) {
            auto n = (*f)->read(out.as_slice().subspan(got));
            if (!n) {
                return detail::fail(n);
            }
            if (*n == 0) {
                break;
            }
            got += *n;
        }
        out.resize(got);
        if (got == info->size) {
            auto more = (*f)->read_all();
            if (!more) {
                return detail::fail(more);
            }
            out.insert(out.end(), more->begin(), more->end());
        }
        (void)(*f)->close();
        return out;
    }
    }

    namespace detail {
    inline expected<string, error> _block_read_text(const string& path)  {
        auto r = _block_read_file(path);
        if (!r) {
            return detail::fail(r);
        }
        return detail::text_of(as_bytes(r->as_slice()));
    }
    }

    namespace detail {
    inline async::task<expected<vector<byte>, error>> _co_read_file(string path)  {   // by value: a task is lazy, the caller's string may be gone before it runs
        co_return co_await async::spawn_blocking([path] { return _block_read_file(path); });
    }
    }

    // `read_file(...)` on this thread, `co_await async_read_file(...)` in a task
    inline expected<vector<byte>, error> read_file(const string& path) {
        return detail::_block_read_file(path);
    }

    inline async::task<expected<vector<byte>, error>> async_read_file(const string& path) {
        return detail::_co_read_file(path);
    }

    namespace detail {
    inline async::task<expected<string, error>> _co_read_text(string path)  {
        co_return co_await async::spawn_blocking([path] { return _block_read_text(path); });
    }
    }

    // `read_text(...)` on this thread, `co_await async_read_text(...)` in a task
    inline expected<string, error> read_text(const string& path) {
        return detail::_block_read_text(path);
    }

    inline async::task<expected<string, error>> async_read_text(const string& path) {
        return detail::_co_read_text(path);
    }

    // The whole file written in one call: created or truncated, the
    // bytes stored, closed (the close's error reported too); append_file
    // adds them at the end, creating the file when it is not there. The
    // async forms run on the pool: the data stays alive while the task
    // awaits, as a task's local does.
    namespace detail {
    inline expected<void, error> _block_write_file(const string& path, const slice<const byte>& data, permissions p) {
        auto f = create(path, p);
        if (!f) {
            return detail::fail(f);
        }
        auto w = (*f)->write(data);
        auto c = (*f)->close();
        if (!w) {
            return detail::fail(w);
        }
        return c;
    }
    }

    namespace detail {
    inline expected<void, error> _block_write_file(const string& path, const string& text, permissions p) {
        return _block_write_file(path, detail::bytes_of(text), p);
    }
    }

    namespace detail {
    inline expected<void, error> _block_append_file(const string& path, const slice<const byte>& data, permissions p) {
        auto f = open(path, open_flags::write | open_flags::create | open_flags::append, p);
        if (!f) {
            return detail::fail(f);
        }
        auto w = (*f)->write(data);
        auto c = (*f)->close();
        if (!w) {
            return detail::fail(w);
        }
        return c;
    }
    }

    namespace detail {
    inline expected<void, error> _block_append_file(const string& path, const string& text, permissions p) {
        return _block_append_file(path, detail::bytes_of(text), p);
    }
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_write_file(string path, slice<const byte> data, permissions p) {
        co_return co_await async::spawn_blocking([path, data, p] { return _block_write_file(path, data, p); });
    }
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_write_file(string path, string text, permissions p) {
        co_return co_await async::spawn_blocking([path, text, p] { return _block_write_file(path, text, p); });
    }
    }

    // `io::write_file(...)` on this thread, `co_await io::async_write_file(...)` in a task
    inline expected<void, error> write_file(const string& path, const string& text, permissions p = permissions(0666) ) {
        return detail::_block_write_file(path, text, p);
    }

    inline async::task<expected<void, error>> async_write_file(const string& path, const string& text, permissions p = permissions(0666) ) {
        return detail::_co_write_file(path, text, p);
    }

    // `io::write_file(...)` on this thread, `co_await io::async_write_file(...)` in a task
    inline expected<void, error> write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666) ) {
        return detail::_block_write_file(path, data, p);
    }

    inline async::task<expected<void, error>> async_write_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666) ) {
        return detail::_co_write_file(path, data, p);
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_append_file(string path, slice<const byte> data, permissions p) {
        co_return co_await async::spawn_blocking([path, data, p] { return _block_append_file(path, data, p); });
    }
    }

    namespace detail {
    inline async::task<expected<void, error>> _co_append_file(string path, string text, permissions p) {
        co_return co_await async::spawn_blocking([path, text, p] { return _block_append_file(path, text, p); });
    }
    }

    // `io::append_file(...)` on this thread, `co_await io::async_append_file(...)` in a task
    inline expected<void, error> append_file(const string& path, const string& text, permissions p = permissions(0666) ) {
        return detail::_block_append_file(path, text, p);
    }

    inline async::task<expected<void, error>> async_append_file(const string& path, const string& text, permissions p = permissions(0666) ) {
        return detail::_co_append_file(path, text, p);
    }

    // `io::append_file(...)` on this thread, `co_await io::async_append_file(...)` in a task
    inline expected<void, error> append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666) ) {
        return detail::_block_append_file(path, data, p);
    }

    inline async::task<expected<void, error>> async_append_file(const string& path, const slice<const byte>& data, permissions p = permissions(0666) ) {
        return detail::_co_append_file(path, data, p);
    }


    namespace detail {
        // The system's temporary directory: $TMPDIR, else /tmp
        inline string temp_root() {
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
    inline expected<tracked_ptr<file>, error> temp_file(const string& dir = {}, const string& pattern = "*") {
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
    inline async::task<expected<tracked_ptr<file>, error>> async_temp_file(const string& dir = {}, const string& pattern = "*") {
        return detail::on_pool([dir, pattern] { return temp_file(dir, pattern); });
    }

    inline async::task<expected<string, error>> async_make_temp_dir(const string& dir = {}, const string& pattern = "*") {
        return detail::on_pool([dir, pattern] { return make_temp_dir(dir, pattern); });
    }
}
