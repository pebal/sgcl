//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// No header of sgcl's defines the short macros of <termios.h> and
// <sys/ioctl.h> (B0, ECHO, VMIN, CS8 …): sgcl/io's terminal code declares
// its own (sgcl/io/detail/posix_tty.h). Once they reached every program
// that included sgcl/io.h, and codec's own B0 did not compile after it.
// This file is compiled, not run: a variable of the program's named as one
// of those macros, between io and codec, and then the whole platform with
// none of them defined. It includes nothing before sgcl/io.h.
#include "sgcl/io.h"

namespace no_platform_macros {
    int B0, ECHO, VMIN;
}

#include "sgcl/codec.h"
#include "sgcl/sgcl.h"

#if defined(B0) || defined(B9600) || defined(B38400) || defined(ECHO) || defined(ECHONL) || defined(ICANON) || defined(ISIG) \
    || defined(IEXTEN) || defined(ICRNL) || defined(IXON) || defined(OPOST) || defined(VMIN) || defined(VTIME) || defined(VEOF) \
    || defined(CS8) || defined(CSIZE) || defined(PARENB) || defined(NCCS) || defined(TCSANOW) || defined(TCSAFLUSH) \
    || defined(TAB0) || defined(CR0) || defined(NL0) || defined(CTRL) || defined(CEOF) || defined(CINTR) \
    || defined(TIOCGWINSZ) || defined(TIOCSWINSZ) || defined(TIOCSCTTY) || defined(FIONREAD) || defined(FIONBIO) \
    || defined(NCC) || defined(N_TTY)
#error "a header of sgcl's brings the macros of <termios.h> or <sys/ioctl.h>"
#endif

#include <gtest/gtest.h>

TEST(IoNoPlatformMacros_Tests, TheProgramsNamesCompile) {
    no_platform_macros::B0 = 1;
    no_platform_macros::ECHO = 2;
    no_platform_macros::VMIN = 3;
    EXPECT_EQ(no_platform_macros::B0 + no_platform_macros::ECHO + no_platform_macros::VMIN, 6);
}
