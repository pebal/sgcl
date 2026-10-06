//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The rule for platform headers: sgcl/io never includes <termios.h> or
// <sys/ioctl.h> in a header of its own (their B0, ECHO, VMIN … macros would
// reach every program that includes sgcl/io.h); sgcl/io/detail/posix_tty.h
// declares what pty, terminal and exec call under names of its own, bound
// by asm labels to the C library's symbols. This file includes the real
// headers (a test may) and holds the declarations against them: the layouts
// copied (sizes, offsets, types), every constant the system's value, every
// function the same symbol, and a terminal's modes read both ways the same
// bytes.
#if !defined(_WIN32)

#include "tests/types.h"

#include <cstddef>
#include <cstring>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <type_traits>

#include "sgcl/io/detail/posix_tty.h"

namespace tty = sgcl::io::detail::tty;

// struct termios and its scalar types
static_assert(std::is_same_v<tty::Flag, tcflag_t>);
static_assert(std::is_same_v<tty::Speed, speed_t>);
static_assert(std::is_same_v<cc_t, unsigned char>);
static_assert(tty::ControlCount == NCCS);
static_assert(sizeof(tty::Termios) == sizeof(struct termios));
static_assert(alignof(tty::Termios) == alignof(struct termios));
static_assert(offsetof(tty::Termios, c_iflag) == offsetof(struct termios, c_iflag));
static_assert(offsetof(tty::Termios, c_oflag) == offsetof(struct termios, c_oflag));
static_assert(offsetof(tty::Termios, c_cflag) == offsetof(struct termios, c_cflag));
static_assert(offsetof(tty::Termios, c_lflag) == offsetof(struct termios, c_lflag));
static_assert(offsetof(tty::Termios, c_cc) == offsetof(struct termios, c_cc));
static_assert(offsetof(tty::Termios, c_ispeed) == offsetof(struct termios, c_ispeed));
static_assert(offsetof(tty::Termios, c_ospeed) == offsetof(struct termios, c_ospeed));
#if defined(__linux__)
static_assert(offsetof(tty::Termios, c_line) == offsetof(struct termios, c_line));
#endif

// struct winsize
static_assert(sizeof(tty::WindowSize) == sizeof(struct winsize));
static_assert(offsetof(tty::WindowSize, ws_row) == offsetof(struct winsize, ws_row));
static_assert(offsetof(tty::WindowSize, ws_col) == offsetof(struct winsize, ws_col));
static_assert(offsetof(tty::WindowSize, ws_xpixel) == offsetof(struct winsize, ws_xpixel));
static_assert(offsetof(tty::WindowSize, ws_ypixel) == offsetof(struct winsize, ws_ypixel));
static_assert(std::is_same_v<decltype(tty::WindowSize::ws_row), decltype(winsize::ws_row)>);

// the constants
static_assert(tty::IgnoreBreak == IGNBRK);
static_assert(tty::BreakInterrupt == BRKINT);
static_assert(tty::MarkParity == PARMRK);
static_assert(tty::StripInput == ISTRIP);
static_assert(tty::NewlineToReturn == INLCR);
static_assert(tty::IgnoreReturn == IGNCR);
static_assert(tty::ReturnToNewline == ICRNL);
static_assert(tty::OutputFlowControl == IXON);
static_assert(tty::PostProcess == OPOST);
static_assert(tty::CharacterSize == CSIZE);
static_assert(tty::CharacterSize8 == CS8);
static_assert(tty::ParityEnable == PARENB);
static_assert(tty::Echo == ECHO);
static_assert(tty::EchoNewline == ECHONL);
static_assert(tty::Signals == ISIG);
static_assert(tty::Canonical == ICANON);
static_assert(tty::Extended == IEXTEN);
static_assert(tty::MinimumIndex == VMIN);
static_assert(tty::TimeIndex == VTIME);
static_assert(tty::Now == TCSANOW);
static_assert(tty::Flush == TCSAFLUSH);
static_assert(tty::GetWindowSize == (unsigned long)TIOCGWINSZ);
static_assert(tty::SetWindowSize == (unsigned long)TIOCSWINSZ);
static_assert(tty::SetControlling == (unsigned long)TIOCSCTTY);

// Every declaration the C library's symbol
TEST(IoTtyPlatform_Tests, TheDeclarationsAreTheLibcs) {
    // the addresses compared as the loader resolved the symbol (dlsym): two
    // declarations are two functions to the optimizer, which may fold an ==
    // between them to false, so each goes through a volatile first
    auto resolved = [](auto function) {
        const void* volatile p = reinterpret_cast<const void*>(function);
        return p;
    };
    auto same = [&](auto ours, auto theirs, const char* name) {
        const void* symbol = dlsym(RTLD_DEFAULT, name);
        return symbol != nullptr && resolved(ours) == symbol && resolved(theirs) == symbol;
    };
    EXPECT_TRUE(same(&tty::tty_get_attributes, &::tcgetattr, "tcgetattr")) << "tcgetattr";
    EXPECT_TRUE(same(&tty::tty_set_attributes, &::tcsetattr, "tcsetattr")) << "tcsetattr";
    EXPECT_TRUE(same(&tty::tty_ioctl, &::ioctl, "ioctl")) << "ioctl";
}

// A terminal's modes and size read through the declarations and through the
// system's headers: the same bytes; and set through ours, read back by the
// system's
TEST(IoTtyPlatform_Tests, ATerminalReadBothWays) {
    sgcl::io::pty p = sgcl::io::open_pty({30, 100, 640, 480}).value();
    int t = p.terminal().fd();

    struct termios theirs = {};
    tty::Termios ours = {};
    ASSERT_EQ(::tcgetattr(t, &theirs), 0);
    ASSERT_EQ(tty::tty_get_attributes(t, &ours), 0);
    EXPECT_EQ(std::memcmp(&theirs, &ours, sizeof ours), 0);

    ours.c_lflag &= ~tty::Echo;
    ours.c_cc[tty::MinimumIndex] = 3;
    ASSERT_EQ(tty::tty_set_attributes(t, tty::Now, &ours), 0);
    ASSERT_EQ(::tcgetattr(t, &theirs), 0);
    EXPECT_FALSE(theirs.c_lflag & ECHO);
    EXPECT_EQ(theirs.c_cc[VMIN], 3);

    struct winsize w = {};
    tty::WindowSize v = {};
    ASSERT_EQ(::ioctl(t, TIOCGWINSZ, &w), 0);
    ASSERT_EQ(tty::tty_ioctl(t, tty::GetWindowSize, &v), 0);
    EXPECT_EQ(v.ws_row, 30);
    EXPECT_EQ(v.ws_col, 100);
    EXPECT_EQ(v.ws_xpixel, 640);
    EXPECT_EQ(v.ws_ypixel, 480);
    EXPECT_EQ(std::memcmp(&w, &v, sizeof v), 0);
    v.ws_row = 41;
    ASSERT_EQ(tty::tty_ioctl(p.fd(), tty::SetWindowSize, &v), 0);
    ASSERT_EQ(::ioctl(t, TIOCGWINSZ, &w), 0);
    EXPECT_EQ(w.ws_row, 41);
    EXPECT_EQ(w.ws_col, 100);
    (void)p.close();
}

#endif
