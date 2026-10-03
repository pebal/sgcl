//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(_WIN32)

// The few functions of kernel32 that a mapped region needs (mapped_region.h:
// io::mapping, io::shared_memory), declared here rather than through
// windows.h, which would bring min, max, ERROR, near, far and the rest into
// every program that includes sgcl/io (DESIGN 306). The declarations are the
// SDK's own, type for type (HANDLE is void*, DWORD unsigned long, BOOL int,
// SIZE_T an unsigned integer of a pointer's width, LPCWSTR const wchar_t*),
// so that they name the same functions as windows.h's and a program that
// includes windows.h too sees no conflict; the structures passed by
// pointer are the SDK's incomplete types, and the program's own
// layouts are cast to them (MemoryInfo; a long long for LARGE_INTEGER). The constants are copies.
// tests/io/mapping.cpp holds them against windows.h (static_assert of the
// constants and the layout) on Windows. Written 2026-09-29 against the
// Windows 10 SDK's memoryapi.h, fileapi.h, handleapi.h and errhandlingapi.h;
// compiled and tested on the Windows machine of the platform matrix.
struct _SECURITY_ATTRIBUTES;
struct _MEMORY_BASIC_INFORMATION;
union _LARGE_INTEGER;

namespace sgcl::io::detail::win {
    using Handle = void*;
    using Dword = unsigned long;
    using Bool = int;
    using Size = decltype(sizeof(0));

    extern "C" {
        __declspec(dllimport) Handle __stdcall CreateFileW(const wchar_t* name, Dword access, Dword share, ::_SECURITY_ATTRIBUTES* security, Dword disposition, Dword flags, Handle tmpl);
        __declspec(dllimport) Bool __stdcall GetFileSizeEx(Handle file, ::_LARGE_INTEGER* size);   // LARGE_INTEGER: a long long (its QuadPart) in its layout
        __declspec(dllimport) Handle __stdcall CreateFileMappingW(Handle file, ::_SECURITY_ATTRIBUTES* security, Dword protect, Dword size_high, Dword size_low, const wchar_t* name);
        __declspec(dllimport) Handle __stdcall OpenFileMappingW(Dword access, Bool inherit, const wchar_t* name);
        __declspec(dllimport) void* __stdcall MapViewOfFile(Handle mapping, Dword access, Dword offset_high, Dword offset_low, Size bytes);
        __declspec(dllimport) Bool __stdcall UnmapViewOfFile(const void* base);
        __declspec(dllimport) Bool __stdcall FlushViewOfFile(const void* base, Size bytes);
        __declspec(dllimport) Bool __stdcall FlushFileBuffers(Handle file);
        __declspec(dllimport) Size __stdcall VirtualQuery(const void* address, ::_MEMORY_BASIC_INFORMATION* info, Size length);
        __declspec(dllimport) Bool __stdcall CloseHandle(Handle object);
        __declspec(dllimport) Dword __stdcall GetLastError();
        __declspec(dllimport) int __stdcall MultiByteToWideChar(unsigned code_page, Dword flags, const char* text, int bytes, wchar_t* wide, int chars);
        long long __cdecl _get_osfhandle(int fd);   // the CRT's (io.h): intptr_t
        int __cdecl _dup(int fd);
        int __cdecl _close(int fd);
    }

    // MEMORY_BASIC_INFORMATION on a 64-bit Windows: the region's size
    // (a view's, rounded up to a page) after the four fields before it
    struct MemoryInfo {
        void* base_address;
        void* allocation_base;
        Dword allocation_protect;
        unsigned short partition_id;
        Size region_size;
        Dword state;
        Dword protect;
        Dword type;
    };

    inline const Handle InvalidHandle = reinterpret_cast<Handle>(-1);       // INVALID_HANDLE_VALUE (a cast: not constexpr)
    inline constexpr Dword GenericRead = 0x80000000;                          // GENERIC_READ
    inline constexpr Dword GenericWrite = 0x40000000;                         // GENERIC_WRITE
    inline constexpr Dword FileShareRead = 0x1;                               // FILE_SHARE_READ
    inline constexpr Dword FileShareWrite = 0x2;                              // FILE_SHARE_WRITE
    inline constexpr Dword FileShareDelete = 0x4;                             // FILE_SHARE_DELETE
    inline constexpr Dword OpenExisting = 3;                                  // OPEN_EXISTING
    inline constexpr Dword FileAttributeNormal = 0x80;                        // FILE_ATTRIBUTE_NORMAL
    inline constexpr Dword PageReadonly = 0x02;                               // PAGE_READONLY
    inline constexpr Dword PageReadwrite = 0x04;                              // PAGE_READWRITE
    inline constexpr Dword PageWritecopy = 0x08;                              // PAGE_WRITECOPY
    inline constexpr Dword FileMapCopy = 0x0001;                              // FILE_MAP_COPY
    inline constexpr Dword FileMapWrite = 0x0002;                             // FILE_MAP_WRITE
    inline constexpr Dword FileMapRead = 0x0004;                              // FILE_MAP_READ
    inline constexpr Dword FileMapAllAccess = 0x000F001F;                     // FILE_MAP_ALL_ACCESS
    inline constexpr Dword ErrorAlreadyExists = 183;                          // ERROR_ALREADY_EXISTS
    inline constexpr unsigned CodePageUtf8 = 65001;                           // CP_UTF8
    inline constexpr Size AllocationGranularity = 65536;                      // SYSTEM_INFO::dwAllocationGranularity: 64 KB on every Windows

    // A UTF-8 text as the wide text the W functions take, NUL-terminated;
    // empty for a text that is not UTF-8
    template<class Wide>
    Wide wide(const char* text, int bytes) noexcept {
        Wide w;
        if (bytes == 0) {
            return w;
        }
        int n = MultiByteToWideChar(CodePageUtf8, 8 /* MB_ERR_INVALID_CHARS */, text, bytes, nullptr, 0);
        if (n <= 0) {
            return w;
        }
        w.resize(size_t(n));
        MultiByteToWideChar(CodePageUtf8, 8, text, bytes, w.data(), n);
        return w;
    }
}

#endif
