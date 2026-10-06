//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(_WIN32)

#include "win_mapping.h"

#include <cstddef>

// Dynamic libraries on Windows (library.h: io::open_library, io::library):
// LoadLibraryExW of the UTF-8 path made wide, GetProcAddress, FreeLibrary,
// the system's text of an error from FormatMessageW (as UTF-16; the caller
// makes it UTF-8). Hard links (fs.h: io::link) are CreateHardLinkW. Written to
// the pattern of win_mapping.h (DESIGN 306): the SDK's declarations type for
// type, no windows.h. Written 2026-10-06 against the Windows 10 SDK's
// libloaderapi.h, winbase.h and fileapi.h; compile-checked only
// (clang -target x86_64-pc-windows-msvc -fsyntax-only), not run: io's files
// are POSIX until the platform matrix.
struct HINSTANCE__;

namespace sgcl::io::detail::win {
    extern "C" {
        __declspec(dllimport) ::HINSTANCE__* __stdcall LoadLibraryExW(const wchar_t* name, Handle file, Dword flags);
        __declspec(dllimport) Bool __stdcall FreeLibrary(::HINSTANCE__* module);
        __declspec(dllimport) void* __stdcall GetProcAddress(::HINSTANCE__* module, const char* name);   // FARPROC
        __declspec(dllimport) Dword __stdcall FormatMessageW(Dword flags, const void* source, Dword message, Dword language, wchar_t* buffer, Dword size, void* arguments);
        __declspec(dllimport) Bool __stdcall CreateHardLinkW(const wchar_t* link, const wchar_t* target, ::_SECURITY_ATTRIBUTES* security);
    }

    inline constexpr Dword LoadLibrarySearchDefaultDirs = 0x00001000;   // LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
    inline constexpr Dword FormatMessageFromSystem = 0x00001000;        // FORMAT_MESSAGE_FROM_SYSTEM
    inline constexpr Dword FormatMessageIgnoreInserts = 0x00000200;     // FORMAT_MESSAGE_IGNORE_INSERTS

    // The system's text of an error, in `buffer` (UTF-16, its line end
    // dropped); its length in characters, 0 when the system has none
    inline Dword error_text(Dword code, wchar_t* buffer, Dword size) noexcept {
        Dword n = FormatMessageW(FormatMessageFromSystem | FormatMessageIgnoreInserts, nullptr, code, 0, buffer, size, nullptr);
        while (n > 0 && (buffer[n - 1] == L'\r' || buffer[n - 1] == L'\n' || buffer[n - 1] == L' ')) {
            buffer[--n] = 0;
        }
        return n;
    }

    // A library loaded from its wide path; null and GetLastError() when not
    inline ::HINSTANCE__* load_library(const wchar_t* path) noexcept {
        return LoadLibraryExW(path, nullptr, 0);
    }

    inline void* library_symbol(::HINSTANCE__* module, const char* name) noexcept {
        return GetProcAddress(module, name);
    }

    inline Dword free_library(::HINSTANCE__* module) noexcept {
        return FreeLibrary(module) ? 0 : GetLastError();
    }
}

#endif
