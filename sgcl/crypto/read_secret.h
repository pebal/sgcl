//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "secret.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/string.h"
#include "../io/error.h"
#include "../io/file.h"
#include "../io/terminal.h"

#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <string_view>
#include <unistd.h>

namespace sgcl::crypto {
    // A file's bytes as a secret: a private key's PEM, a password, a key
    // file. The bytes go straight from the file into a secret_bytes, never
    // through managed memory (io::read_file gives a managed vector); every
    // block a growth leaves behind is zeroed. The file is read to its end
    // on this thread (a task's form, if one is asked for, would run it on
    // the blocking pool).
    inline expected<secret_bytes, io::error> read_secret(const string& path) {
        auto f = io::open(path);
        if (!f) {
            return unexpected(f.error());
        }
        secret_bytes out(secret_bytes::inline_capacity);
        size_t n = 0;
        for (;;) {
            if (n == out.size()) {
                out.resize(out.size() * 2);
            }
            auto r = f->read(out.as_slice().subslice(n));
            if (!r) {
                (void)f->close();   // now, as after a whole read, not when the collector finds the file dead
                return unexpected(r.error());
            }
            if (*r == 0) {
                break;
            }
            n += *r;
        }
        out.resize(n);
        (void)f->close();
        return out;
    }

    namespace detail {
        // The text written to the terminal whole, EINTR retried; nothing
        // done about a failure (a prompt that cannot be shown changes nothing)
        inline void terminal_put(int fd, std::string_view text) noexcept {
            while (!text.empty()) {
                ssize_t n = ::write(fd, text.data(), text.size());
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return;
                }
                text.remove_prefix(size_t(n));
            }
        }
    }

    // A password typed on the process's own terminal (/dev/tty, whatever
    // the standard streams were redirected to): the prompt written to it,
    // the echo turned off (io::disable_echo), a line read straight into a
    // secret_bytes, never through managed memory or a buffer left behind
    // (getpass and Python's getpass read into a string the collector would
    // free without zeroing), the modes given back on every path and the new
    // line the hidden Enter did not show written after it. A backspace (BS
    // or DEL) takes the last byte back; the line's "\n" (and a "\r" before
    // it) is not part of it. ENXIO with the path /dev/tty when the process
    // has no terminal (a service, a job of a scheduler);
    // io::errc::unexpected_eof when the input ends (^D) before anything was
    // typed
    inline expected<secret_bytes, io::error> read_password(const string& prompt = {}) {
        int fd;
        do {
            fd = ::open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
        } while (fd < 0 && errno == EINTR);
        if (fd < 0) {
            return unexpected(io::last_error("read_password", "/dev/tty"));
        }
        detail::terminal_put(fd, prompt.view());
        auto quiet = io::disable_echo(fd);
        if (!quiet) {
            ::close(fd);
            return unexpected(io::error(quiet.error().code(), "read_password", "/dev/tty"));
        }
        secret_bytes out(secret_bytes::inline_capacity);
        size_t kept = 0;     // the password's bytes, at the front
        bool line = false;   // its end read
        optional<io::error> failed;
        while (!line) {
            if (kept == out.size()) {
                out.resize(out.size() * 2);   // the old block zeroed
            }
            ssize_t n = ::read(fd, out.as_slice().data() + kept, out.size() - kept);
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                failed = io::last_error("read_password", "/dev/tty");
                break;
            }
            if (n == 0) {
                if (kept == 0) {
                    failed = io::error(io::errc::unexpected_eof, "read_password", "/dev/tty");
                }
                break;
            }
            // the bytes read, edited in place: a backspace takes one back, the
            // line ends at its "\n"
            byte* b = out.as_slice().data();
            const size_t end = kept + size_t(n);
            for (size_t i = kept; i < end; ++i) {
                const unsigned char c = (unsigned char)b[i];
                b[i] = byte(0);
                if (c == '\n') {
                    line = true;
                    break;
                }
                if (c == '\b' || c == 0x7F) {
                    if (kept > 0) {
                        b[--kept] = byte(0);
                    }
                    continue;
                }
                b[kept++] = byte(c);
            }
        }
        (void)quiet->restore();
        detail::terminal_put(fd, "\n");
        ::close(fd);
        if (kept > 0 && (unsigned char)out.as_slice()[kept - 1] == '\r') {
            --kept;
        }
        out.resize(kept);   // a shrink zeroes the bytes it drops
        if (failed) {
            return unexpected(*failed);
        }
        return out;
    }
}
