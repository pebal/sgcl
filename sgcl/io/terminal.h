//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/posix_tty.h"
#include "error.h"
#include "fs.h"
#include "../async/channel.h"
#include "../async/coroutine.h"
#include "../async/select.h"
#include "../async/signal.h"
#include "../async/stop_token.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <unistd.h>

namespace sgcl::io {
    // The terminal (Go's x/term): its size and its change, raw mode and the
    // echo turned off (crypto::read_password reads a password under it, into
    // secret bytes rather than a managed string). A terminal is a descriptor
    // (is_terminal(fd)); the functions here take one, as Go's take an fd:
    // the standard streams are 0, 1 and 2, a pty's terminal end gives its
    // own. Windows' console is written in detail/win_terminal.h, POSIX
    // here (io's files are POSIX until the platform matrix).

    // The size of a terminal: rows and columns of characters, and its
    // pixels when the terminal says them (0 when it does not)
    struct terminal_size {
        uint16_t rows = 24;
        uint16_t columns = 80;
        uint16_t width_pixels = 0;
        uint16_t height_pixels = 0;

        friend bool operator==(const terminal_size&, const terminal_size&) noexcept = default;
    };

    namespace detail {
        SGCL_INLINE_HOT terminal_size size_of(const tty::WindowSize& w) noexcept {
            return terminal_size{w.ws_row, w.ws_col, w.ws_xpixel, w.ws_ypixel};
        }

        SGCL_INLINE_HOT tty::WindowSize winsize_of(const terminal_size& s) noexcept {
            tty::WindowSize w = {};
            w.ws_row = s.rows;
            w.ws_col = s.columns;
            w.ws_xpixel = s.width_pixels;
            w.ws_ypixel = s.height_pixels;
            return w;
        }

        SGCL_INLINE_HOT int ioctl_retry(int fd, unsigned long request, void* arg) noexcept {
            int r;
            do {
                r = tty::tty_ioctl(fd, request, arg);
            } while (r < 0 && errno == EINTR);
            return r;
        }
    }

    // The size of the terminal on the descriptor (TIOCGWINSZ): ENOTTY
    // when it is no terminal
    SGCL_INLINE_HOT expected<terminal_size, error> get_terminal_size(int fd = 1) noexcept {
        detail::tty::WindowSize w = {};
        if (detail::ioctl_retry(fd, detail::tty::GetWindowSize, &w) != 0) {
            return detail::fail(last_error("get_terminal_size"));
        }
        return detail::size_of(w);
    }

    // The size set (TIOCSWINSZ): the terminal's foreground process group
    // gets SIGWINCH when it changed
    SGCL_INLINE_HOT expected<void, error> set_terminal_size(int fd, const terminal_size& size) noexcept {
        detail::tty::WindowSize w = detail::winsize_of(size);
        if (detail::ioctl_retry(fd, detail::tty::SetWindowSize, &w) != 0) {
            return detail::fail(last_error("set_terminal_size"));
        }
        return {};
    }

    // A terminal's modes changed, as make_raw or disable_echo left them:
    // the modes from before given back by restore() or, failing that, by the
    // destructor, so that no return or exception leaves the terminal raw or
    // silent (Go's term.MakeRaw and term.Restore as one guard). Moved, not
    // copied: one guard restores. Holds no tracked word: a descriptor and
    // the saved modes.
    class terminal_mode {
    public:
        terminal_mode() noexcept = default;

        SGCL_INLINE_HOT terminal_mode(terminal_mode&& other) noexcept
        : _fd(other._fd), _saved(other._saved) {
            other._fd = -1;
        }

        SGCL_INLINE_HOT terminal_mode& operator=(terminal_mode&& other) noexcept {
            if (this != &other) {
                (void)restore();
                _fd = other._fd;
                _saved = other._saved;
                other._fd = -1;
            }
            return *this;
        }

        terminal_mode(const terminal_mode&) = delete;
        terminal_mode& operator=(const terminal_mode&) = delete;

        SGCL_INLINE_HOT ~terminal_mode() {
            (void)restore();
        }

        // The modes from before back (tcsetattr, TCSAFLUSH: what was typed
        // in raw mode and not read is dropped, as a shell does on its
        // return). Once: a second, or one of a guard that holds none, does
        // nothing and succeeds
        expected<void, error> restore() noexcept {
            if (_fd < 0) {
                return {};
            }
            int fd = _fd;
            _fd = -1;
            int r;
            do {
                r = detail::tty::tty_set_attributes(fd, detail::tty::Flush, &_saved);
            } while (r < 0 && errno == EINTR);
            if (r != 0) {
                return detail::fail(last_error("restore"));
            }
            return {};
        }

        // The terminal's descriptor, -1 when the guard holds none
        SGCL_INLINE_HOT int fd() const noexcept {
            return _fd;
        }

        // Whether it holds a terminal to restore
        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return _fd >= 0;
        }

    private:
        friend expected<terminal_mode, error> make_raw(int fd) noexcept;
        friend expected<terminal_mode, error> disable_echo(int fd) noexcept;

        SGCL_INLINE_HOT terminal_mode(int fd, const detail::tty::Termios& saved) noexcept
        : _fd(fd), _saved(saved) {
        }

        int _fd = -1;
        detail::tty::Termios _saved = {};
    };

    namespace detail {
        SGCL_INLINE_HOT int tcsetattr_retry(int fd, int when, const detail::tty::Termios& t) noexcept {
            int r;
            do {
                r = tty::tty_set_attributes(fd, when, &t);
            } while (r < 0 && errno == EINTR);
            return r;
        }
    }

    // The terminal on the descriptor in raw mode: the bytes as they come,
    // one at a time — no echo, no line editing, no signal from ^C or ^Z, no
    // translation of the line ends either way (cfmakeraw's flags, VMIN 1,
    // VTIME 0: Go's term.MakeRaw). What an editor, a game or a key reader
    // needs; the guard returned gives the terminal back. ENOTTY when the
    // descriptor is no terminal
    inline expected<terminal_mode, error> make_raw(int fd = 0) noexcept {
        namespace tty = detail::tty;
        tty::Termios saved;
        if (tty::tty_get_attributes(fd, &saved) != 0) {
            return detail::fail(last_error("make_raw"));
        }
        tty::Termios raw = saved;
        raw.c_iflag &= ~(tty::IgnoreBreak | tty::BreakInterrupt | tty::MarkParity | tty::StripInput | tty::NewlineToReturn | tty::IgnoreReturn | tty::ReturnToNewline | tty::OutputFlowControl);
        raw.c_oflag &= ~tty::PostProcess;
        raw.c_lflag &= ~(tty::Echo | tty::EchoNewline | tty::Canonical | tty::Signals | tty::Extended);
        raw.c_cflag &= ~(tty::CharacterSize | tty::ParityEnable);
        raw.c_cflag |= tty::CharacterSize8;
        raw.c_cc[tty::MinimumIndex] = 1;
        raw.c_cc[tty::TimeIndex] = 0;
        if (detail::tcsetattr_retry(fd, tty::Now, raw) != 0) {
            return detail::fail(last_error("make_raw"));
        }
        return terminal_mode(fd, saved);
    }

    // The terminal on the descriptor with its echo off, by lines still: what
    // is typed is not shown, a line is read at its Enter (ICANON), ^C keeps
    // its signal (ISIG) and a "\r" ends a line as a "\n" (ICRNL). What a
    // password is read under: crypto::read_password reads one into secret
    // bytes this way. The guard gives the modes back; ENOTTY when the
    // descriptor is no terminal
    inline expected<terminal_mode, error> disable_echo(int fd = 0) noexcept {
        namespace tty = detail::tty;
        tty::Termios saved;
        if (tty::tty_get_attributes(fd, &saved) != 0) {
            return detail::fail(last_error("disable_echo"));
        }
        tty::Termios quiet = saved;
        quiet.c_lflag &= ~tty::Echo;
        quiet.c_lflag |= tty::Canonical | tty::Signals;
        quiet.c_iflag |= tty::ReturnToNewline;
        if (detail::tcsetattr_retry(fd, tty::Now, quiet) != 0) {
            return detail::fail(last_error("disable_echo"));
        }
        return terminal_mode(fd, saved);
    }

    namespace detail {
        // The newest size in the channel: one not received yet replaced
        SGCL_INLINE_HOT void offer_size(const async::channel<terminal_size>& out, const terminal_size& size) {
            if (!out.try_send(size)) {
                (void)out.try_receive();
                (void)out.try_send(size);
            }
        }

        inline async::task<> follow_sizes(int fd, async::channel<int> winch, async::channel<terminal_size> out, async::stop_token stop) noexcept {
            for (;;) {
                bool ended = false;
                if (stop.stop_possible()) {
                    co_await async::select(winch.on_receive([&](optional<int> n) { ended = !n; }), stop.on_stop([&] { ended = true; }));
                } else {
                    ended = !(co_await winch.receive());
                }
                if (ended || out.closed()) {
                    break;
                }
                if (auto size = get_terminal_size(fd)) {
                    try {
                        offer_size(out, *size);
                    } catch (...) {
                        break;
                    }
                }
            }
            async::detail::signals_instance().forget(async::detail::ChannelAccess::state(winch).get());
            out.close();
        }
    }

    // The size of the terminal on the descriptor after every change of it,
    // which the terminal tells the process by SIGWINCH (async::signals): a
    // channel that a task co_awaits, a thread receives on, a select takes
    // as a case. It holds one size, and a newer one replaces a size not
    // received yet, so that a burst of resizes is one redraw. It ends,
    // closed and the signal's registration forgotten, when the stop is
    // requested, or at the first change after the program closed it.
    // std::system_error when the signal's thread cannot be made
    // (async::signals)
    inline async::channel<terminal_size> size_changes(int fd = 1, async::stop_token stop = {}) {
        async::channel<int> winch = async::signals({SIGWINCH});
        async::channel<terminal_size> out(1);
        async::go(detail::follow_sizes(fd, winch, out, stop));
        return out;
    }
}
