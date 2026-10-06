//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(_WIN32)

#include "win_mapping.h"

#include <cstdint>
#include <cstdlib>

// The pseudo-console of Windows (ConPTY, Windows 10 1809 and later), the
// platform's pseudo-terminal under io::pty (pty.h): a console whose input
// is a pipe the program writes and whose output, the screen as VT
// sequences, a pipe it reads; a child attached to it at its creation
// (PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE) takes it for its console. Written
// to the pattern of win_mapping.h (DESIGN 306): the SDK's declarations
// type for type, no windows.h; the three functions of the pseudo-console
// itself are looked up in kernel32 at their first use (GetProcAddress), so
// that a Windows without them is an error of the call (ERROR_PROC_NOT_FOUND)
// rather than a program that does not load, and so that COORD, a structure
// the SDK passes by value, is passed as what the x64 calling convention
// makes of it, one 32-bit integer (X in the low half, Y in the high).
// Written 2026-10-05 against the Windows 10 SDK's consoleapi.h,
// processthreadsapi.h, namedpipeapi.h and fileapi.h; compile-checked only
// (clang -target x86_64-pc-windows-msvc -fsyntax-only), not run: io's files
// and command are POSIX until the platform matrix.
struct _STARTUPINFOW;
struct _PROCESS_INFORMATION;
struct _PROC_THREAD_ATTRIBUTE_LIST;
struct _OVERLAPPED;
struct HINSTANCE__;

namespace sgcl::io::detail::win {
    extern "C" {
        __declspec(dllimport) Bool __stdcall CreatePipe(Handle* read, Handle* write, ::_SECURITY_ATTRIBUTES* security, Dword size);
        __declspec(dllimport) Bool __stdcall ReadFile(Handle file, void* buffer, Dword bytes, Dword* read, ::_OVERLAPPED* overlapped);
        __declspec(dllimport) Bool __stdcall WriteFile(Handle file, const void* buffer, Dword bytes, Dword* written, ::_OVERLAPPED* overlapped);
        __declspec(dllimport) Bool __stdcall InitializeProcThreadAttributeList(::_PROC_THREAD_ATTRIBUTE_LIST* list, Dword count, Dword flags, Size* size);
        __declspec(dllimport) Bool __stdcall UpdateProcThreadAttribute(::_PROC_THREAD_ATTRIBUTE_LIST* list, Dword flags, Size attribute, void* value, Size size, void* previous, Size* returned);
        __declspec(dllimport) void __stdcall DeleteProcThreadAttributeList(::_PROC_THREAD_ATTRIBUTE_LIST* list);
        __declspec(dllimport) Bool __stdcall CreateProcessW(const wchar_t* application, wchar_t* command_line, ::_SECURITY_ATTRIBUTES* process_security, ::_SECURITY_ATTRIBUTES* thread_security,
                                                            Bool inherit, Dword flags, void* environment, const wchar_t* directory, ::_STARTUPINFOW* startup, ::_PROCESS_INFORMATION* info);
        __declspec(dllimport) ::HINSTANCE__* __stdcall GetModuleHandleW(const wchar_t* name);
        __declspec(dllimport) void* __stdcall GetProcAddress(::HINSTANCE__* module, const char* name);   // FARPROC: any function pointer
    }

    // STARTUPINFOEXW on a 64-bit Windows: STARTUPINFOW and the attribute
    // list after it
    struct StartupInfoEx {
        Dword cb;
        wchar_t* reserved;
        wchar_t* desktop;
        wchar_t* title;
        Dword x, y, x_size, y_size, x_count_chars, y_count_chars, fill_attribute;
        Dword flags;
        unsigned short show_window;
        unsigned short reserved2_size;
        unsigned char* reserved2;
        Handle std_input;
        Handle std_output;
        Handle std_error;
        ::_PROC_THREAD_ATTRIBUTE_LIST* attributes;
    };

    // PROCESS_INFORMATION
    struct ProcessInformation {
        Handle process;
        Handle thread;
        Dword process_id;
        Dword thread_id;
    };

    inline constexpr Size ProcThreadAttributePseudoconsole = 0x00020016;   // PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
    inline constexpr Dword ExtendedStartupinfoPresent = 0x00080000;        // EXTENDED_STARTUPINFO_PRESENT
    inline constexpr Dword CreateUnicodeEnvironment = 0x00000400;          // CREATE_UNICODE_ENVIRONMENT
    inline constexpr Dword StartfUseStdhandles = 0x00000100;               // STARTF_USESTDHANDLES
    inline constexpr Dword ErrorProcNotFound = 127;                        // ERROR_PROC_NOT_FOUND
    inline constexpr Dword ErrorBrokenPipe = 109;                          // ERROR_BROKEN_PIPE

    using Hresult = long;
    using CreatePseudoConsoleFn = Hresult(__stdcall*)(uint32_t size, Handle input, Handle output, Dword flags, void** console);
    using ResizePseudoConsoleFn = Hresult(__stdcall*)(void* console, uint32_t size);
    using ClosePseudoConsoleFn = void(__stdcall*)(void* console);

    // COORD{X = columns, Y = rows} as the x64 convention passes it
    inline uint32_t coord(uint16_t rows, uint16_t columns) noexcept {
        return uint32_t(columns) | (uint32_t(rows) << 16);
    }

    // The three functions of the pseudo-console, from kernel32
    struct ConPtyFunctions {
        CreatePseudoConsoleFn create = nullptr;
        ResizePseudoConsoleFn resize = nullptr;
        ClosePseudoConsoleFn close = nullptr;
    };

    inline const ConPtyFunctions& conpty_functions() noexcept {
        static const ConPtyFunctions f = [] {
            ConPtyFunctions r;
            if (auto k = GetModuleHandleW(L"kernel32.dll")) {
                r.create = reinterpret_cast<CreatePseudoConsoleFn>(GetProcAddress(k, "CreatePseudoConsole"));
                r.resize = reinterpret_cast<ResizePseudoConsoleFn>(GetProcAddress(k, "ResizePseudoConsole"));
                r.close = reinterpret_cast<ClosePseudoConsoleFn>(GetProcAddress(k, "ClosePseudoConsole"));
            }
            return r;
        }();
        return f;
    }

    // A pseudo-console and the program's ends of its two pipes: `input`
    // written (what the child reads as typed), `output` read (what it
    // shows). 0, or the error of the system (GetLastError, an HRESULT's
    // code)
    struct ConPty {
        void* console = nullptr;
        Handle input = nullptr;
        Handle output = nullptr;
    };

    inline Dword open_conpty(uint16_t rows, uint16_t columns, ConPty& out) noexcept {
        const auto& f = conpty_functions();
        if (!f.create || !f.resize || !f.close) {
            return ErrorProcNotFound;
        }
        Handle in_read = nullptr, in_write = nullptr, out_read = nullptr, out_write = nullptr;
        if (!CreatePipe(&in_read, &in_write, nullptr, 0)) {
            return GetLastError();
        }
        if (!CreatePipe(&out_read, &out_write, nullptr, 0)) {
            Dword e = GetLastError();
            CloseHandle(in_read);
            CloseHandle(in_write);
            return e;
        }
        void* console = nullptr;
        Hresult hr = f.create(coord(rows, columns), in_read, out_write, 0, &console);
        CloseHandle(in_read);     // the console holds its ends now
        CloseHandle(out_write);
        if (hr < 0) {
            CloseHandle(in_write);
            CloseHandle(out_read);
            return Dword(hr & 0xFFFF);
        }
        out = ConPty{console, in_write, out_read};
        return 0;
    }

    inline Dword resize_conpty(const ConPty& c, uint16_t rows, uint16_t columns) noexcept {
        Hresult hr = conpty_functions().resize(c.console, coord(rows, columns));
        return hr < 0 ? Dword(hr & 0xFFFF) : 0;
    }

    // The console closed (its children see their console go) and the
    // program's ends of the pipes
    inline void close_conpty(ConPty& c) noexcept {
        if (c.console) {
            conpty_functions().close(c.console);
            c.console = nullptr;
        }
        if (c.input) {
            CloseHandle(c.input);
            c.input = nullptr;
        }
        if (c.output) {
            CloseHandle(c.output);
            c.output = nullptr;
        }
    }

    // A child attached to the console at its creation: the command line
    // as CreateProcessW takes it (written into: the call may change it),
    // the directory and the environment (a block of UTF-16 NAME=value
    // strings, or null for the program's); its process and thread handles
    // in `info`, the caller's to close
    inline Dword spawn_on_conpty(const ConPty& c, wchar_t* command_line, const wchar_t* directory, wchar_t* environment, ProcessInformation& info) noexcept {
        Size bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        auto* list = static_cast<::_PROC_THREAD_ATTRIBUTE_LIST*>(std::malloc(bytes));
        if (!list) {
            return 8;   // ERROR_NOT_ENOUGH_MEMORY
        }
        if (!InitializeProcThreadAttributeList(list, 1, 0, &bytes)) {
            Dword e = GetLastError();
            std::free(list);
            return e;
        }
        Dword e = 0;
        if (!UpdateProcThreadAttribute(list, 0, ProcThreadAttributePseudoconsole, c.console, sizeof(void*), nullptr, nullptr)) {
            e = GetLastError();
        } else {
            StartupInfoEx si = {};
            si.cb = sizeof si;
            si.flags = StartfUseStdhandles;   // no standard handle of the program's: the console's own
            si.attributes = list;
            Dword flags = ExtendedStartupinfoPresent | (environment ? CreateUnicodeEnvironment : 0);
            if (!CreateProcessW(nullptr, command_line, nullptr, nullptr, 0, flags, environment, directory,
                                reinterpret_cast<::_STARTUPINFOW*>(&si), reinterpret_cast<::_PROCESS_INFORMATION*>(&info))) {
                e = GetLastError();
            }
        }
        DeleteProcThreadAttributeList(list);
        std::free(list);
        return e;
    }
}

#endif
