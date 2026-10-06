//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "file.h"
#include "../async/coroutine.h"
#include "../async/timer.h"
#include "../core/aliases.h"
#include "../core/duration.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <fcntl.h>
#include <sys/file.h>
#include <thread>
#include <unistd.h>

namespace sgcl::io {
    // Locks of files between processes, advisory as everywhere on POSIX: a
    // whole file by flock (the lock flock(1) and the Maildir programs take;
    // LockFileEx on Windows, written in detail/win_lock.h), a byte range by
    // an open-file-description lock (fcntl F_OFD_SETLK: Linux 3.15 and
    // later, macOS). Both belong to the open file, not to the process: two
    // opens of a file in one process exclude each other as two processes
    // do, and a close of another descriptor of the file drops nothing (the
    // classic fcntl locks of POSIX are the process's, dropped by any close,
    // and are not used). flock and fcntl locks are separate families, which
    // may or may not see each other by the system: a file is locked one way.
    // Go has none in its standard library (syscall.Flock by hand); Rust's
    // File::lock is the nearest.

    // Shared (any number of holders, readers) or exclusive (one, a writer)
    enum class lock_mode { shared, exclusive };

    // What a lock takes beyond the one-line form
    struct lock_options {
        lock_mode mode = lock_mode::exclusive;
        uint64_t offset = 0;          // a byte range from here, of `length` bytes; both 0: the whole file
        uint64_t length = 0;          // 0 with an offset: to the end of the file and past it
        optional<duration> timeout;   // none: wait as long as it takes; zero: try once; else at most this long
    };

    class file_lock;

    namespace detail {
        // Whether the options ask for a range rather than the whole file
        SGCL_INLINE_HOT bool lock_is_range(const lock_options& o) noexcept {
            return o.offset != 0 || o.length != 0;
        }

#if defined(F_OFD_SETLK)
        inline constexpr bool HaveOfdLocks = true;
#else
        inline constexpr bool HaveOfdLocks = false;
#endif

        // One attempt, or a wait in the kernel (`wait`): 0 or the errno
        inline int lock_once(int fd, const lock_options& o, bool wait) noexcept {
            if (!lock_is_range(o)) {
                int op = (o.mode == lock_mode::exclusive ? LOCK_EX : LOCK_SH) | (wait ? 0 : LOCK_NB);
                while (::flock(fd, op) != 0) {
                    if (errno != EINTR) {
                        return errno == EWOULDBLOCK ? EWOULDBLOCK : errno;
                    }
                }
                return 0;
            }
#if defined(F_OFD_SETLK)
            if (o.offset > uint64_t(INT64_MAX) || o.length > uint64_t(INT64_MAX)) {
                return EINVAL;
            }
            struct ::flock fl = {};
            fl.l_type = o.mode == lock_mode::exclusive ? F_WRLCK : F_RDLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = off_t(o.offset);
            fl.l_len = off_t(o.length);
            fl.l_pid = 0;   // an OFD lock asks for 0
            while (::fcntl(fd, wait ? F_OFD_SETLKW : F_OFD_SETLK, &fl) != 0) {
                if (errno != EINTR) {
                    return errno == EAGAIN || errno == EACCES ? EWOULDBLOCK : errno;
                }
            }
            return 0;
#else
            (void)fd;
            (void)wait;
            return ENOTSUP;
#endif
        }

        // A wait of at most `timeout`: macOS's kernel waits for a range itself
        // (F_OFD_SETLKWTIMEOUT); otherwise attempts, a sleep between them,
        // 1 ms doubling to 50 ms, to the deadline. 0, ETIMEDOUT or the errno
        inline int lock_within(int fd, const lock_options& o, duration timeout) noexcept {
#if defined(F_OFD_SETLKWTIMEOUT)
            if (lock_is_range(o)) {
                if (o.offset > uint64_t(INT64_MAX) || o.length > uint64_t(INT64_MAX)) {
                    return EINVAL;
                }
                struct ::flocktimeout ft = {};
                ft.fl.l_type = o.mode == lock_mode::exclusive ? F_WRLCK : F_RDLCK;
                ft.fl.l_whence = SEEK_SET;
                ft.fl.l_start = off_t(o.offset);
                ft.fl.l_len = off_t(o.length);
                const auto ns = std::max<int64_t>(0, timeout.nanoseconds());
                ft.timeout.tv_sec = time_t(ns / 1000000000);
                ft.timeout.tv_nsec = long(ns % 1000000000);
                while (::fcntl(fd, F_OFD_SETLKWTIMEOUT, &ft) != 0) {
                    if (errno != EINTR) {
                        return errno == EAGAIN || errno == EACCES ? ETIMEDOUT : errno;
                    }
                }
                return 0;
            }
#endif
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::nanoseconds(timeout.nanoseconds());
            auto pause = std::chrono::milliseconds(1);
            for (;;) {
                int e = lock_once(fd, o, false);
                if (e != EWOULDBLOCK) {
                    return e;
                }
                auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    return ETIMEDOUT;
                }
                std::this_thread::sleep_for(std::min<std::chrono::steady_clock::duration>(pause, deadline - now));
                pause = std::min(pause * 2, std::chrono::milliseconds(50));
            }
        }

        // The lock given back: flock's LOCK_UN, or the range unlocked
        inline int unlock_once(int fd, bool range, uint64_t offset, uint64_t length) noexcept {
            if (!range) {
                while (::flock(fd, LOCK_UN) != 0) {
                    if (errno != EINTR) {
                        return errno;
                    }
                }
                return 0;
            }
#if defined(F_OFD_SETLK)
            struct ::flock fl = {};
            fl.l_type = F_UNLCK;
            fl.l_whence = SEEK_SET;
            fl.l_start = off_t(offset);
            fl.l_len = off_t(length);
            while (::fcntl(fd, F_OFD_SETLK, &fl) != 0) {
                if (errno != EINTR) {
                    return errno;
                }
            }
            return 0;
#else
            (void)fd;
            (void)offset;
            (void)length;
            return ENOTSUP;
#endif
        }

        struct FileLockAccess;
    }

    // A lock held on a file, given back by unlock() or, failing that, by the
    // destructor: a scoped guard, so that no return and no exception leaves
    // a file locked. Moved, not copied: one guard unlocks. It holds the file
    // (a handle), so that the descriptor, and the lock with it, stays open
    // for as long as the guard does.
    class file_lock {
    public:
        file_lock() noexcept = default;

        SGCL_INLINE_HOT file_lock(file_lock&& other) noexcept
        : _file(other._file), _mode(other._mode), _offset(other._offset), _length(other._length), _range(other._range), _held(other._held) {
            other._held = false;
        }

        SGCL_INLINE_HOT file_lock& operator=(file_lock&& other) noexcept {
            if (this != &other) {
                (void)unlock();
                _file = other._file;
                _mode = other._mode;
                _offset = other._offset;
                _length = other._length;
                _range = other._range;
                _held = other._held;
                other._held = false;
            }
            return *this;
        }

        file_lock(const file_lock&) = delete;
        file_lock& operator=(const file_lock&) = delete;

        SGCL_INLINE_HOT ~file_lock() {
            (void)unlock();
        }

        // The lock given back now; once: a second, or one of a guard that
        // holds none, does nothing and succeeds. A file closed meanwhile
        // has given it back already (errc::closed)
        expected<void, error> unlock() noexcept {
            if (!_held) {
                return {};
            }
            _held = false;
            int fd = _file.fd();
            if (fd < 0) {
                return detail::fail(error(errc::closed, "unlock", _file.path()));
            }
            if (int e = detail::unlock_once(fd, _range, _offset, _length)) {
                return detail::fail(error(error_code(e, std::system_category()), "unlock", _file.path()));
            }
            return {};
        }

        // What it holds: shared or exclusive, the range (both 0 for the
        // whole file), the file
        SGCL_INLINE_HOT lock_mode mode() const noexcept {
            return _mode;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return _offset;
        }

        SGCL_INLINE_HOT uint64_t length() const noexcept {
            return _length;
        }

        SGCL_INLINE_HOT const io::file& file() const noexcept {
            return _file;
        }

        // Whether it holds a lock (not one made by the default constructor,
        // moved from or unlocked)
        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return _held;
        }

    private:
        friend struct detail::FileLockAccess;

        SGCL_INLINE_HOT file_lock(const io::file& f, const lock_options& o) noexcept
        : _file(f), _mode(o.mode), _offset(o.offset), _length(o.length), _range(detail::lock_is_range(o)), _held(true) {
        }

        io::file _file;
        lock_mode _mode = lock_mode::exclusive;
        uint64_t _offset = 0;
        uint64_t _length = 0;
        bool _range = false;
        bool _held = false;
    };

    namespace detail {
        struct FileLockAccess {
            SGCL_INLINE_HOT static file_lock make(const file& f, const lock_options& o) noexcept {
                return file_lock(f, o);
            }
        };

        // The error of a lock not taken: the errno as it is (EWOULDBLOCK and
        // ETIMEDOUT are_timeout())
        SGCL_INLINE_HOT error lock_error(int e, const file& f) noexcept {
            return error(error_code(e, std::system_category()), "lock", f.path());
        }

        // The lock file at the path: opened for reading and writing,
        // created when missing
        SGCL_INLINE_HOT expected<file, error> lock_file_at(const string& path) noexcept {
            return open(path, open_flags::read | open_flags::write | open_flags::create);
        }

        inline async::task<expected<file_lock, error>> co_lock_file(file f, lock_options o) noexcept {
            const int fd = f.fd();
            if (fd < 0) {
                co_return detail::fail(error(errc::closed, "lock", f.path()));
            }
            const bool limited = o.timeout.has_value();
            const auto deadline = limited ? clock::now() + *o.timeout : clock::time_point::max();
            duration pause = std::chrono::milliseconds(1);
            for (;;) {
                int e = lock_once(fd, o, false);
                if (e == 0) {
                    co_return FileLockAccess::make(f, o);
                }
                if (e != EWOULDBLOCK) {
                    co_return detail::fail(lock_error(e, f));
                }
                if (limited && *o.timeout == duration::zero()) {
                    co_return detail::fail(lock_error(EWOULDBLOCK, f));
                }
                auto now = clock::now();
                if (limited && now >= deadline) {
                    co_return detail::fail(lock_error(ETIMEDOUT, f));
                }
                co_await async::sleep(limited ? std::min(pause, duration(deadline - now)) : pause);
                pause = std::min(pause * 2, duration(std::chrono::milliseconds(50)));
            }
        }

        inline async::task<expected<file_lock, error>> co_lock_path(string path, lock_options o) noexcept {
            auto f = co_await async_open(path, open_flags::read | open_flags::write | open_flags::create);
            if (!f) {
                co_return detail::fail(f);
            }
            co_return co_await co_lock_file(*f, o);
        }
    }

    // The file locked as the options say: the whole file (flock) or a byte
    // range (an OFD lock), shared or exclusive, waiting as long as it takes,
    // or trying once (a zero timeout: EWOULDBLOCK when it is held), or
    // waiting at most the timeout (ETIMEDOUT). errc::closed for a closed
    // file; ENOTSUP for a range where the system has no OFD lock; EINVAL for
    // a range past INT64_MAX
    inline expected<file_lock, error> lock_file(const file& f, const lock_options& options = {}) noexcept {
        const int fd = f.fd();
        if (fd < 0) {
            return detail::fail(error(errc::closed, "lock", f.path()));
        }
        int e;
        if (!options.timeout) {
            e = detail::lock_once(fd, options, true);
        } else if (*options.timeout <= duration::zero()) {
            e = detail::lock_once(fd, options, false);
        } else {
            e = detail::lock_within(fd, options, *options.timeout);
        }
        if (e != 0) {
            return detail::fail(detail::lock_error(e, f));
        }
        return detail::FileLockAccess::make(f, options);
    }

    // The one-line form: the lock file at the path opened (created when
    // missing, 0666 less the umask) and locked; the guard holds it open
    inline expected<file_lock, error> lock_file(const string& path, const lock_options& options = {}) noexcept {
        auto f = detail::lock_file_at(path);
        if (!f) {
            return detail::fail(f);
        }
        return lock_file(*f, options);
    }

    // `lock_file(...)` on this thread, `co_await async_lock_file(...)` in a
    // task: the lock tried, and a sleep on the timers between the tries
    // (1 ms doubling to 50 ms), so that no thread is held for the length of
    // somebody else's lock; a timeout as the blocking form's
    SGCL_INLINE_HOT async::task<expected<file_lock, error>> async_lock_file(const file& f, lock_options options = {}) noexcept {
        return detail::co_lock_file(f, std::move(options));
    }

    SGCL_INLINE_HOT async::task<expected<file_lock, error>> async_lock_file(const string& path, lock_options options = {}) noexcept {
        return detail::co_lock_path(path, std::move(options));
    }
}
