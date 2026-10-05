//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"

#include <cstdint>

// The values SSH's client and server share: a terminal asked for, a
// question of keyboard-interactive authentication, how a remote program
// ended, what a command run in one line gave.
namespace sgcl::net::ssh {
    // A pseudo-terminal (RFC 4254 §6.2): the terminal's type, its size in
    // characters and pixels, its modes (RFC 4254 §8: an opcode and its
    // value, ECHO = 53, ISIG = 50 …)
    struct pty {
        string term = "xterm";
        uint32_t columns = 80;
        uint32_t rows = 24;
        uint32_t width_pixels = 0;
        uint32_t height_pixels = 0;
        vector<pair<uint8_t, uint32_t>> modes;
    };

    // A question of keyboard-interactive authentication (RFC 4256): its
    // text and whether the answer may be shown as it is typed
    struct prompt {
        string text;
        bool echo = false;
    };

    // How a remote program ended (RFC 4254 §6.10): its exit code, or the
    // signal that ended it ("TERM", "KILL" …, without "SIG"), whether it
    // dumped core, the server's message about it. code is -1 when the
    // program ended by a signal or the server said nothing
    struct exit_status {
        int code = -1;
        string signal;
        bool core_dumped = false;
        string message;
    };

    // What client::run gives: the command's standard output and error, and
    // how it ended
    struct run_result {
        string out;
        string err;
        ssh::exit_status status;
    };
}
