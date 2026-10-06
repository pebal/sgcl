//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
#include "error.h"
#include "exec.h"
#include "file.h"
#include "stream.h"
#include "terminal.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

namespace sgcl::io {
    // A pseudo-terminal: a pair of descriptors, the program's end (the
    // master) and the terminal end (the slave), that a program on the
    // terminal end takes for a terminal — what a terminal emulator, an SSH
    // server or `script` gives the shell it runs. What is written to the
    // master the program reads as typed (echoed, edited by the line
    // discipline, ^C its SIGINT); what it writes is read from the master.
    //
    // A child is started on the terminal by start(command&): the streams
    // the command leaves null are the terminal, the child leads a session
    // of its own and the terminal is its controlling terminal (its ^C, its
    // SIGWINCH, its /dev/tty). Without fork: posix_spawn makes the session
    // (POSIX_SPAWN_SETSID) and opens the terminal in the child by its name,
    // which a session leader without a controlling terminal acquires by
    // opening it (Linux and macOS alike); a fork with TIOCSCTTY, Go's way,
    // is no option in a process whose collector has threads.
    //
    // The program's copy of the terminal end is kept from open_pty to the
    // start (a size set before a child opened the terminal stays) and
    // closed by it, so that a read of the master gives 0, the end of the
    // stream, once the child and whatever it left on the terminal let go
    // of it: macOS's read gives 0 then, Linux's EIO, which read takes for
    // the end as well.
    //
    // The state of a pseudo-terminal, the object its handles share
    // (io::pty below): the two ends and the terminal's name.
    namespace detail {
        class PtyState final {
        public:
            SGCL_INLINE_HOT PtyState(const file& master, const file& terminal, const string& name, const terminal_size& size) noexcept
            : _master(master), _terminal(terminal), _name(name), _size(size) {
            }

            expected<size_t, error> read(const slice<byte>& buffer) const {
                return _ended(_master.read(buffer));
            }

            SGCL_INLINE_HOT async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const noexcept {
                return _co_read(_master, buffer);
            }

            SGCL_INLINE_HOT expected<size_t, error> write(const slice<const byte>& data) const {
                return _master.write(data);
            }

            SGCL_INLINE_HOT async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const noexcept {
                return _master.async_write(data);
            }

            // The master closed (a child on the terminal gets SIGHUP), and
            // the program's terminal end when start() has not closed it
            SGCL_INLINE_HOT expected<void, error> close() const noexcept {
                if (_terminal && !_terminal.is_closed()) {
                    (void)_terminal.close();
                }
                return _master.close();
            }

            SGCL_INLINE_HOT bool is_closed() const noexcept {
                return _master.is_closed();
            }

            SGCL_INLINE_HOT int fd() const noexcept {
                return _master.fd();
            }

            SGCL_INLINE_HOT const string& name() const noexcept {
                return _name;
            }

            SGCL_INLINE_HOT const file& terminal() const noexcept {
                return _terminal;
            }

            expected<void, error> start(command& c) {
                if (_master.is_closed()) {
                    return detail::fail(error(errc::closed, "start", _name));
                }
                if (!_terminal || _terminal.is_closed()) {   // a second start: the terminal end opened again by its name
                    auto t = _open_terminal(_name);
                    if (!t) {
                        return detail::fail(t);
                    }
                    _terminal = *t;
                    // the size the pty was last given: a terminal nobody held
                    // any more was reset by its last close (macOS: 0 by 0)
                    detail::tty::WindowSize w = detail::winsize_of(_size.load(std::memory_order_relaxed));
                    (void)detail::ioctl_retry(_terminal.fd(), detail::tty::SetWindowSize, &w);
                }
                const bool set_in = !c.in, set_out = !c.out, set_err = !c.err;
                if (set_in) {
                    c.in = _terminal;
                }
                if (set_out) {
                    c.out = _terminal;
                }
                if (set_err) {
                    c.err = set_out ? c.out : io::writer(_terminal);   // one writer for both: one open, a dup for the other
                }
                c._terminal = _name;
                c._terminal_fd = _terminal.fd();
                auto r = c.start();
                c._terminal = string();
                c._terminal_fd = -1;
                if (!r) {
                    if (set_in) {
                        c.in = io::reader();
                    }
                    if (set_out) {
                        c.out = io::writer();
                    }
                    if (set_err) {
                        c.err = io::writer();
                    }
                    return r;
                }
                (void)_terminal.close();   // the child holds the terminal now: the master's reads end with it
                return {};
            }

            SGCL_INLINE_HOT expected<void, error> resize(const terminal_size& size) const noexcept {
                if (_master.is_closed()) {
                    return detail::fail(error(errc::closed, "resize", _name));
                }
                detail::tty::WindowSize w = detail::winsize_of(size);
                if (detail::ioctl_retry(_master.fd(), detail::tty::SetWindowSize, &w) != 0) {
                    return detail::fail(last_error("resize", _name));
                }
                _size.store(size, std::memory_order_relaxed);
                return {};
            }

            SGCL_INLINE_HOT expected<terminal_size, error> size() const noexcept {
                if (_master.is_closed()) {
                    return detail::fail(error(errc::closed, "size", _name));
                }
                detail::tty::WindowSize w = {};
                if (detail::ioctl_retry(_master.fd(), detail::tty::GetWindowSize, &w) != 0) {
                    return detail::fail(last_error("size", _name));
                }
                return detail::size_of(w);
            }

            // The terminal end opened by its name: never the controlling
            // terminal of this process (O_NOCTTY), closed in its children
            static expected<file, error> _open_terminal(const string& name) noexcept {
                int fd;
                do {
                    fd = ::open(name.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
                } while (fd < 0 && errno == EINTR);
                if (fd < 0) {
                    return detail::fail(last_error("open", name));
                }
                return from_fd(fd, name);
            }

        private:
            // The end of the child is the end of the stream: Linux's EIO
            // after the last holder of the terminal end let go of it
            SGCL_INLINE_HOT static expected<size_t, error> _ended(expected<size_t, error> r) noexcept {
                if (!r && r.error().code() == std::errc::io_error) {
                    return size_t(0);
                }
                return r;
            }

            static async::task<expected<size_t, error>> _co_read(file master, slice<byte> buffer) noexcept {   // by value: a task is lazy
                co_return _ended(co_await master.async_read(buffer));
            }

            file _master;
            file _terminal;
            string _name;
            mutable std::atomic<terminal_size> _size;   // the size last given, for a terminal end opened again
        };

        struct PtyAccess;
    }

    // A pseudo-terminal as a handle: one tracked word to the state above,
    // copied and passed by value, the copies one pseudo-terminal. A stream
    // (Go's *os.File of creack/pty): read what the program on the terminal
    // writes, write what it reads, with the async forms on the reactor.
    // Made by open_pty; a default-constructed one holds none (`!p`), and an
    // operation on it is a contract violation.
    class pty final : public mixin::reader<pty>, public mixin::writer<pty> {
    public:
        using mixin::writer<pty>::write;
        using mixin::writer<pty>::async_write;

        pty() noexcept = default;

        // What the program on the terminal wrote; 0 once the terminal end
        // is held by nobody (the child and its descendants ended).
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> read(const slice<byte>& buffer) const {
            return _get().read(buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const noexcept {
            return _get().async_read(buffer);
        }

        // What the program on the terminal reads, as typed (the terminal
        // echoes it, ^C is its SIGINT). `write(...)` on this thread,
        // `co_await async_write(...)` in a task
        SGCL_INLINE_HOT expected<size_t, error> write(const slice<const byte>& data) const {
            return _get().write(data);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const noexcept {
            return _get().async_write(data);
        }

        // The pseudo-terminal closed: the master (a child on the terminal
        // gets SIGHUP) and the program's terminal end
        SGCL_INLINE_HOT expected<void, error> close() const noexcept {
            return _get().close();
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _get().is_closed();
        }

        // The command started on the terminal: the streams it leaves null
        // are the terminal, the child the leader of a session of its own
        // whose controlling terminal the terminal is; then the program's
        // copy of the terminal end closed. command::start's errors
        SGCL_INLINE_HOT expected<void, error> start(command& c) const {
            return _get().start(c);
        }

        // The size of the terminal (TIOCSWINSZ): the session's foreground
        // process group gets SIGWINCH; size() reads it (TIOCGWINSZ)
        SGCL_INLINE_HOT expected<void, error> resize(const terminal_size& size) const noexcept {
            return _get().resize(size);
        }

        SGCL_INLINE_HOT expected<terminal_size, error> size() const noexcept {
            return _get().size();
        }

        // The terminal end's path ("/dev/ttys003", "/dev/pts/4"); the
        // terminal end itself, open until start() closes it; the master's
        // descriptor, -1 when closed
        SGCL_INLINE_HOT const string& name() const noexcept {
            return _get().name();
        }

        SGCL_INLINE_HOT io::file terminal() const noexcept {
            return _get().terminal();
        }

        SGCL_INLINE_HOT int fd() const noexcept {
            return _get().fd();
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_state;
        }

        SGCL_INLINE_HOT friend bool operator==(const pty& a, const pty& b) noexcept {
            return a._state == b._state;
        }

    private:
        friend struct detail::PtyAccess;
        friend struct detail::HandleAccess;

        SGCL_INLINE_HOT explicit pty(tracked_ptr<detail::PtyState> state) noexcept
        : _state(std::move(state)) {
        }

        SGCL_INLINE_HOT detail::PtyState& _get() const noexcept {
            assert(_state && "an empty io::pty");
            return *_state;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::PtyState>& _stream_state() const noexcept {
            return _state;
        }

        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT pty(sgcl::detail::FromWord, const tracked_ptr<detail::PtyState>& w) noexcept
        : _state(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::PtyState>& _handle_word() noexcept {
            return _state;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::PtyState>& _handle_word() const noexcept {
            return _state;
        }

        tracked_ptr<detail::PtyState> _state;
    };

    namespace detail {
        template<>
        inline constexpr bool IsStreamHandle<pty> = true;

        struct PtyAccess {
            SGCL_INLINE_HOT static pty make(tracked_ptr<PtyState> state) noexcept {
                return pty(std::move(state));
            }
        };
    }

    // A new pseudo-terminal of the size given: the master (posix_openpt,
    // grantpt, unlockpt) non-blocking, served by the reactor; the terminal
    // end opened by its name (ptsname_r) and kept for start(); both
    // closed in children (close-on-exec) but for the child start() makes
    inline expected<pty, error> open_pty(const terminal_size& size = {}) noexcept {
        int m = ::posix_openpt(O_RDWR | O_NOCTTY);
        if (m < 0) {
            return detail::fail(last_error("open_pty"));
        }
        auto failed = [m](const char* op) {
            error e = last_error(op);
            ::close(m);
            return detail::fail(e);
        };
        if (::fcntl(m, F_SETFD, FD_CLOEXEC) != 0) {
            return failed("open_pty");
        }
        if (::grantpt(m) != 0) {
            return failed("grantpt");
        }
        if (::unlockpt(m) != 0) {
            return failed("unlockpt");
        }
        char name[128];
        if (int rc = ::ptsname_r(m, name, sizeof name); rc != 0) {
            if (rc > 0) {
                errno = rc;
            }
            return failed("ptsname");
        }
        string path(name);
        auto terminal = detail::PtyState::_open_terminal(path);
        if (!terminal) {
            ::close(m);
            return detail::fail(terminal);
        }
        detail::tty::WindowSize w = detail::winsize_of(size);
        if (detail::ioctl_retry(m, detail::tty::SetWindowSize, &w) != 0) {
            error e = last_error("resize", path);
            (void)terminal->close();
            ::close(m);
            return detail::fail(e);
        }
        detail::set_nonblocking_fd(m);
        file master = from_fd(m, path);
        return detail::PtyAccess::make(make_tracked<detail::PtyState>(master, *terminal, path, size));
    }
}
