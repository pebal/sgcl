//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstdint>

// The terminal calls io's pty, terminal and exec make (tcgetattr, tcsetattr,
// ioctl with TIOCGWINSZ, TIOCSWINSZ and TIOCSCTTY), declared here under
// names of their own and bound to the C library's symbols by asm labels,
// rather than through <termios.h> and <sys/ioctl.h>: those define B0 …
// B230400, ECHO, ISIG, VMIN, CS8, NCCS, TAB0, CR0 and a hundred other short
// macros (glibc's <sys/ioctl.h> also CTRL(x) through <sys/ttydefaults.h>),
// which every program that includes sgcl/io.h got, so that a variable named
// B0 or ECHO of its own, or of another sgcl module, did not compile (the
// rule for platform headers, the pattern of codec/detail/apple_imageio.h).
// The names here never meet the C library's, so a program that includes
// <termios.h> too sees no conflict. struct termios and struct winsize are
// copies of the system's layout, the values the system's constants
// (tests/io/tty_platform.cpp holds all of them, and the declarations,
// against the real headers: macOS 26 SDK on arm64 and x86-64; glibc on
// x86-64 and AArch64, written from glibc's and the kernel's asm-generic
// headers and checked there by the same test on the Linux machine; another
// Linux architecture is an #error until its values are written).
namespace sgcl::io::detail::tty {
#if defined(__APPLE__)
    using Flag = unsigned long;                         // tcflag_t
    using Speed = unsigned long;                        // speed_t
    inline constexpr int ControlCount = 20;             // NCCS

    struct Termios {                                    // struct termios
        Flag c_iflag;
        Flag c_oflag;
        Flag c_cflag;
        Flag c_lflag;
        unsigned char c_cc[ControlCount];
        Speed c_ispeed;
        Speed c_ospeed;
    };

    // c_iflag
    inline constexpr Flag IgnoreBreak = 0x00000001;     // IGNBRK
    inline constexpr Flag BreakInterrupt = 0x00000002;  // BRKINT
    inline constexpr Flag MarkParity = 0x00000008;      // PARMRK
    inline constexpr Flag StripInput = 0x00000020;      // ISTRIP
    inline constexpr Flag NewlineToReturn = 0x00000040; // INLCR
    inline constexpr Flag IgnoreReturn = 0x00000080;    // IGNCR
    inline constexpr Flag ReturnToNewline = 0x00000100; // ICRNL
    inline constexpr Flag OutputFlowControl = 0x00000200; // IXON
    // c_oflag
    inline constexpr Flag PostProcess = 0x00000001;     // OPOST
    // c_cflag
    inline constexpr Flag CharacterSize = 0x00000300;   // CSIZE
    inline constexpr Flag CharacterSize8 = 0x00000300;  // CS8
    inline constexpr Flag ParityEnable = 0x00001000;    // PARENB
    // c_lflag
    inline constexpr Flag Echo = 0x00000008;            // ECHO
    inline constexpr Flag EchoNewline = 0x00000010;     // ECHONL
    inline constexpr Flag Signals = 0x00000080;         // ISIG
    inline constexpr Flag Canonical = 0x00000100;       // ICANON
    inline constexpr Flag Extended = 0x00000400;        // IEXTEN
    // c_cc
    inline constexpr int MinimumIndex = 16;             // VMIN
    inline constexpr int TimeIndex = 17;                // VTIME
    // tcsetattr's when
    inline constexpr int Now = 0;                       // TCSANOW
    inline constexpr int Flush = 2;                     // TCSAFLUSH
    // ioctl's requests: _IOR('t', 104, struct winsize), _IOW('t', 103,
    // struct winsize), _IO('t', 97)
    inline constexpr unsigned long GetWindowSize = 0x40087468;  // TIOCGWINSZ
    inline constexpr unsigned long SetWindowSize = 0x80087467;  // TIOCSWINSZ
    inline constexpr unsigned long SetControlling = 0x20007461; // TIOCSCTTY

    extern "C" {
        int tty_get_attributes(int fd, Termios* t) noexcept __asm__("_tcgetattr");
        int tty_set_attributes(int fd, int when, const Termios* t) noexcept __asm__("_tcsetattr");
        int tty_ioctl(int fd, unsigned long request, ...) noexcept __asm__("_ioctl");   // variadic, as the system's: arm64 passes the argument on the stack
    }
#elif defined(__linux__) && (defined(__x86_64__) || defined(__aarch64__))
    using Flag = unsigned int;                          // tcflag_t
    using Speed = unsigned int;                         // speed_t
    inline constexpr int ControlCount = 32;             // NCCS

    struct Termios {                                    // struct termios (glibc and musl alike)
        Flag c_iflag;
        Flag c_oflag;
        Flag c_cflag;
        Flag c_lflag;
        unsigned char c_line;
        unsigned char c_cc[ControlCount];
        Speed c_ispeed;
        Speed c_ospeed;
    };

    // c_iflag
    inline constexpr Flag IgnoreBreak = 0000001;        // IGNBRK
    inline constexpr Flag BreakInterrupt = 0000002;     // BRKINT
    inline constexpr Flag MarkParity = 0000010;         // PARMRK
    inline constexpr Flag StripInput = 0000040;         // ISTRIP
    inline constexpr Flag NewlineToReturn = 0000100;    // INLCR
    inline constexpr Flag IgnoreReturn = 0000200;       // IGNCR
    inline constexpr Flag ReturnToNewline = 0000400;    // ICRNL
    inline constexpr Flag OutputFlowControl = 0002000;  // IXON
    // c_oflag
    inline constexpr Flag PostProcess = 0000001;        // OPOST
    // c_cflag
    inline constexpr Flag CharacterSize = 0000060;      // CSIZE
    inline constexpr Flag CharacterSize8 = 0000060;     // CS8
    inline constexpr Flag ParityEnable = 0000400;       // PARENB
    // c_lflag
    inline constexpr Flag Echo = 0000010;               // ECHO
    inline constexpr Flag EchoNewline = 0000100;        // ECHONL
    inline constexpr Flag Signals = 0000001;            // ISIG
    inline constexpr Flag Canonical = 0000002;          // ICANON
    inline constexpr Flag Extended = 0100000;           // IEXTEN
    // c_cc
    inline constexpr int MinimumIndex = 6;              // VMIN
    inline constexpr int TimeIndex = 5;                 // VTIME
    // tcsetattr's when
    inline constexpr int Now = 0;                       // TCSANOW
    inline constexpr int Flush = 2;                     // TCSAFLUSH
    // ioctl's requests (asm-generic/ioctls.h)
    inline constexpr unsigned long GetWindowSize = 0x5413;      // TIOCGWINSZ
    inline constexpr unsigned long SetWindowSize = 0x5414;      // TIOCSWINSZ
    inline constexpr unsigned long SetControlling = 0x540E;     // TIOCSCTTY

    extern "C" {
        int tty_get_attributes(int fd, Termios* t) noexcept __asm__("tcgetattr");
        int tty_set_attributes(int fd, int when, const Termios* t) noexcept __asm__("tcsetattr");
        int tty_ioctl(int fd, unsigned long request, ...) noexcept __asm__("ioctl");
    }
#elif !defined(_WIN32)
#error "sgcl/io/detail/posix_tty.h: the terminal declarations of this system are not written yet"
#endif

#if !defined(_WIN32)
    struct WindowSize {                                 // struct winsize
        unsigned short ws_row;
        unsigned short ws_col;
        unsigned short ws_xpixel;
        unsigned short ws_ypixel;
    };
#endif
}
