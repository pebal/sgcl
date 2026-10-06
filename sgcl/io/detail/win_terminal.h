//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(_WIN32)

#include "win_mapping.h"

// The console of Windows as a terminal (terminal.h: make_raw, read_password,
// get_terminal_size): its input and output modes and the size of its
// window, what Go's golang.org/x/term does on Windows. Written to the
// pattern of win_mapping.h (DESIGN 306): the SDK's declarations type for
// type, no windows.h, the structure passed by pointer the SDK's incomplete
// type and the program's own layout cast to it. Written 2026-10-05 against
// the Windows 10 SDK's consoleapi.h, consoleapi2.h and processenv.h;
// compile-checked only (clang -target x86_64-pc-windows-msvc
// -fsyntax-only), not run: io's files are POSIX until the platform matrix.
struct _CONSOLE_SCREEN_BUFFER_INFO;

namespace sgcl::io::detail::win {
    extern "C" {
        __declspec(dllimport) Bool __stdcall GetConsoleMode(Handle console, Dword* mode);
        __declspec(dllimport) Bool __stdcall SetConsoleMode(Handle console, Dword mode);
        __declspec(dllimport) Bool __stdcall GetConsoleScreenBufferInfo(Handle console, ::_CONSOLE_SCREEN_BUFFER_INFO* info);
        __declspec(dllimport) Handle __stdcall GetStdHandle(Dword which);
    }

    // CONSOLE_SCREEN_BUFFER_INFO: the buffer's size, the cursor, the
    // attributes, the window (left, top, right, bottom, inclusive) and the
    // largest window
    struct ScreenBufferInfo {
        short size_x, size_y;
        short cursor_x, cursor_y;
        unsigned short attributes;
        short left, top, right, bottom;
        short max_x, max_y;
    };

    inline constexpr Dword StdInputHandle = Dword(-10);                 // STD_INPUT_HANDLE
    inline constexpr Dword StdOutputHandle = Dword(-11);                // STD_OUTPUT_HANDLE
    inline constexpr Dword EnableProcessedInput = 0x0001;               // ENABLE_PROCESSED_INPUT
    inline constexpr Dword EnableLineInput = 0x0002;                    // ENABLE_LINE_INPUT
    inline constexpr Dword EnableEchoInput = 0x0004;                    // ENABLE_ECHO_INPUT
    inline constexpr Dword EnableVirtualTerminalInput = 0x0200;         // ENABLE_VIRTUAL_TERMINAL_INPUT
    inline constexpr Dword EnableProcessedOutput = 0x0001;              // ENABLE_PROCESSED_OUTPUT

    // The window of the console on the handle, in characters; 0 or the
    // error of the system
    inline Dword console_size(Handle console, unsigned short& rows, unsigned short& columns) noexcept {
        ScreenBufferInfo info = {};
        if (!GetConsoleScreenBufferInfo(console, reinterpret_cast<::_CONSOLE_SCREEN_BUFFER_INFO*>(&info))) {
            return GetLastError();
        }
        rows = static_cast<unsigned short>(info.bottom - info.top + 1);
        columns = static_cast<unsigned short>(info.right - info.left + 1);
        return 0;
    }

    // The console's input in raw mode, its mode before in `saved` (Go's
    // term.MakeRaw: no echo, no line, no processing of ^C, the keys as VT
    // sequences)
    inline Dword console_make_raw(Handle console, Dword& saved) noexcept {
        if (!GetConsoleMode(console, &saved)) {
            return GetLastError();
        }
        Dword raw = saved & ~(EnableEchoInput | EnableProcessedInput | EnableLineInput | EnableProcessedOutput);
        raw |= EnableVirtualTerminalInput;
        return SetConsoleMode(console, raw) ? 0 : GetLastError();
    }

    // The console's input without its echo, by lines (read_password), its
    // mode before in `saved`
    inline Dword console_echo_off(Handle console, Dword& saved) noexcept {
        if (!GetConsoleMode(console, &saved)) {
            return GetLastError();
        }
        Dword quiet = (saved & ~EnableEchoInput) | EnableProcessedInput | EnableLineInput;
        return SetConsoleMode(console, quiet) ? 0 : GetLastError();
    }

    inline Dword console_restore(Handle console, Dword saved) noexcept {
        return SetConsoleMode(console, saved) ? 0 : GetLastError();
    }
}

#endif
