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

#include <cstdint>
#include <fcntl.h>
#include <poll.h>
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
    result<tracked_ptr<file>> open(const string& path, open_flags flags, permissions p);
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
    class file final : public stream, public seeker {
        friend class sgcl::detail::MakerBase;   // make_tracked constructs a file here, for the three below
        friend result<tracked_ptr<file>> open(const string&, open_flags, permissions);
        friend tracked_ptr<file> from_fd(int, const string&);
        friend tracked_ptr<file> detail::std_stream(int, const string&);

        file(int fd, const string& name, bool reactor, bool owns) noexcept
        : _fd(fd), _path(std::move(name)), _reactor(reactor), _owns(owns) {
        }

    public:
        ~file() override {
            if (_fd >= 0 && _owns) {
                ::close(_fd);
            }
        }

        result<size_t> read(slice<std::byte> buffer) override {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "read", _name()));
            }
            if (buffer.empty()) {
                return 0;
            }
            for (;;) {
                ssize_t n = ::read(_fd, buffer.data(), buffer.size());
                if (n >= 0) {
                    return static_cast<size_t>(n);
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN && _reactor) {
                    _poll(POLLIN);
                    continue;
                }
                return detail::fail(last_error("read", _name()));
            }
        }

        task<result<size_t>> async_read(slice<std::byte> buffer) override {
            if (_fd < 0) {
                co_return detail::fail(error(errc::closed, "read", _name()));
            }
            if (buffer.empty()) {
                co_return 0;
            }
            if (!_reactor) {
                co_return co_await spawn_blocking([this, buffer] { return read(buffer); });
            }
            for (;;) {
                ssize_t n = ::read(_fd, buffer.data(), buffer.size());
                if (n >= 0) {
                    co_return static_cast<size_t>(n);
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    co_await readable(_fd)->async_receive();
                    continue;
                }
                co_return detail::fail(last_error("read", _name()));
            }
        }

        result<size_t> write(slice<const std::byte> data) override {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "write", _name()));
            }
            size_t written = 0;
            while (written < data.size()) {
                ssize_t n = ::write(_fd, data.data() + written, data.size() - written);
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                    continue;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN && _reactor) {
                    _poll(POLLOUT);
                    continue;
                }
                return detail::fail(last_error("write", _name()));
            }
            return written;
        }

        task<result<size_t>> async_write(slice<const std::byte> data) override {
            if (_fd < 0) {
                co_return detail::fail(error(errc::closed, "write", _name()));
            }
            if (!_reactor) {
                co_return co_await spawn_blocking([this, data] { return write(data); });
            }
            size_t written = 0;
            while (written < data.size()) {
                ssize_t n = ::write(_fd, data.data() + written, data.size() - written);
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                    continue;
                }
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN) {
                    co_await writable(_fd)->async_receive();
                    continue;
                }
                co_return detail::fail(last_error("write", _name()));
            }
            co_return written;
        }

        result<uint64_t> seek(int64_t offset, seek_from from = seek_from::begin) override {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "seek", _name()));
            }
            int whence = from == seek_from::begin ? SEEK_SET : from == seek_from::current ? SEEK_CUR : SEEK_END;
            off_t r = ::lseek(_fd, static_cast<off_t>(offset), whence);
            if (r < 0) {
                return detail::fail(last_error("seek", _name()));
            }
            return static_cast<uint64_t>(r);
        }

        result<void> close() override {
            if (_fd < 0) {
                return {};
            }
            int fd = _fd;
            _fd = -1;
            if (_owns && ::close(fd) != 0) {
                return detail::fail(last_error("close", _name()));
            }
            return {};
        }

        bool is_closed() const noexcept override {
            return _fd < 0;
        }

        result<size_t> read_at(slice<std::byte> buffer, uint64_t offset) {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "read_at", _name()));
            }
            for (;;) {
                ssize_t n = ::pread(_fd, buffer.data(), buffer.size(), static_cast<off_t>(offset));
                if (n >= 0) {
                    return static_cast<size_t>(n);
                }
                if (errno != EINTR) {
                    return detail::fail(last_error("read_at", _name()));
                }
            }
        }

        result<size_t> write_at(slice<const std::byte> data, uint64_t offset) {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "write_at", _name()));
            }
            size_t written = 0;
            while (written < data.size()) {
                ssize_t n = ::pwrite(_fd, data.data() + written, data.size() - written, static_cast<off_t>(offset + written));
                if (n >= 0) {
                    written += static_cast<size_t>(n);
                } else if (errno != EINTR) {
                    return detail::fail(last_error("write_at", _name()));
                }
            }
            return written;
        }

        task<result<size_t>> async_read_at(slice<std::byte> buffer, uint64_t offset) {
            co_return co_await spawn_blocking([this, buffer, offset] { return read_at(buffer, offset); });
        }

        task<result<size_t>> async_write_at(slice<const std::byte> data, uint64_t offset) {
            co_return co_await spawn_blocking([this, data, offset] { return write_at(data, offset); });
        }

        // fsync; ftruncate; fstat; fchmod
        result<void> sync() {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "sync", _name()));
            }
            if (::fsync(_fd) != 0) {
                return detail::fail(last_error("sync", _name()));
            }
            return {};
        }

        result<void> truncate(uint64_t size) {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "truncate", _name()));
            }
            if (::ftruncate(_fd, static_cast<off_t>(size)) != 0) {
                return detail::fail(last_error("truncate", _name()));
            }
            return {};
        }

        result<file_info> stat() const {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "stat", _name()));
            }
            struct ::stat st;
            if (::fstat(_fd, &st) != 0) {
                return detail::fail(last_error("stat", _name()));
            }
            return detail::info_of(st, _name());
        }

        result<void> chmod(permissions p) {
            if (_fd < 0) {
                return detail::fail(error(errc::closed, "chmod", _name()));
            }
            if (::fchmod(_fd, static_cast<mode_t>(p)) != 0) {
                return detail::fail(last_error("chmod", _name()));
            }
            return {};
        }

        // The descriptor, -1 when closed; the path it was opened with,
        // or the name given to from_fd
        int fd() const noexcept {
            return _fd;
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

        void _poll(short events) noexcept {
            struct pollfd p{_fd, events, 0};
            while (::poll(&p, 1, -1) < 0 && errno == EINTR) {
            }
        }

        int _fd;
        string _path;
        bool _reactor;
        bool _owns;
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
    inline result<tracked_ptr<file>> open(const string& path, open_flags flags = open_flags::read, permissions p = permissions(0666)) {
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
    inline result<tracked_ptr<file>> create(const string& path, permissions p = permissions(0666)) {
        return open(path, open_flags::write | open_flags::create | open_flags::truncate, p);
    }

    // A file over a descriptor opened elsewhere (a pipe from exec, a
    // socket from net), which the file owns from now on. The
    // descriptor's flags are left as they are: one that is non-blocking
    // already is served by the reactor, any other by the pool.
    inline tracked_ptr<file> from_fd(int fd, const string& name = {}) {
        return tracked_ptr<file>(make_tracked<file>(fd, name, detail::is_nonblocking_fd(fd), true));
    }

    // An anonymous pipe: what is written to the second is read from the
    // first; both ends non-blocking, served by the reactor
    inline result<pair<tracked_ptr<file>, tracked_ptr<file>>> pipe() {
        int fds[2];
        if (::pipe(fds) != 0) {
            return detail::fail(last_error("pipe"));
        }
        for (int fd : fds) {
            ::fcntl(fd, F_SETFD, FD_CLOEXEC);
            detail::set_nonblocking_fd(fd);
        }
        return pair<tracked_ptr<file>, tracked_ptr<file>>(from_fd(fds[0], "pipe"), from_fd(fds[1], "pipe"));
    }

    // The whole file in one call: the size from fstat, one buffer of
    // that size, one read (and more, should the file have grown); text
    // as a string
    inline result<vector<std::byte>> read_file(const string& path) {
        auto f = open(path);
        if (!f) {
            return detail::fail(f);
        }
        auto info = (*f)->stat();
        if (!info) {
            return detail::fail(info);
        }
        vector<std::byte> out;
        out.resize(static_cast<size_t>(info->size));
        auto n = (*f)->read_full(out.as_slice());
        if (!n) {
            return detail::fail(n);
        }
        out.resize(*n);
        if (*n == info->size) {
            auto more = (*f)->read_all();
            if (!more) {
                return detail::fail(more);
            }
            out.insert(out.end(), more->begin(), more->end());
        }
        (*f)->close();
        return out;
    }

    inline result<string> read_text(const string& path) {
        auto r = read_file(path);
        if (!r) {
            return detail::fail(r);
        }
        return detail::text_of(as_bytes(r->as_slice()));
    }

    inline task<result<vector<std::byte>>> async_read_file(const string& path) {
        co_return co_await spawn_blocking([path] { return read_file(path); });
    }

    inline task<result<string>> async_read_text(const string& path) {
        co_return co_await spawn_blocking([path] { return read_text(path); });
    }

    // The whole file written in one call: created or truncated, the
    // bytes stored, closed (the close's error reported too); append_file
    // adds them at the end, creating the file when it is not there. The
    // async forms run on the pool: the data stays alive while the task
    // awaits, as a task's local does.
    inline result<void> write_file(const string& path, slice<const std::byte> data, permissions p = permissions(0666)) {
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

    inline result<void> write_file(const string& path, const string& text, permissions p = permissions(0666)) {
        return write_file(path, detail::bytes_of(text), p);
    }

    inline result<void> append_file(const string& path, slice<const std::byte> data, permissions p = permissions(0666)) {
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

    inline result<void> append_file(const string& path, const string& text, permissions p = permissions(0666)) {
        return append_file(path, detail::bytes_of(text), p);
    }

    inline task<result<void>> async_write_file(const string& path, slice<const std::byte> data, permissions p = permissions(0666)) {
        co_return co_await spawn_blocking([path, data, p] { return write_file(path, data, p); });
    }

    inline task<result<void>> async_write_file(const string& path, const string& text, permissions p = permissions(0666)) {
        co_return co_await spawn_blocking([path, text, p] { return write_file(path, text, p); });
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
    // temp_dir the same for a directory, 0700. The caller removes it.
    inline result<tracked_ptr<file>> temp_file(const string& dir = {}, const string& pattern = "*") {
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

    inline result<string> temp_dir(const string& dir = {}, const string& pattern = "*") {
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
        return detail::fail(error(std::make_error_code(std::errc::file_exists), "temp_dir", pattern));
    }
}
