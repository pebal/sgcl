//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(_WIN32)

#include "win_mapping.h"

#include <cstdint>

// The locks of files on Windows (lock.h: io::lock_file): LockFileEx and
// UnlockFileEx over a range of bytes, the whole file being the range of
// every byte (offset 0, length 2^64 - 1), mandatory where POSIX's are
// advisory, held by the handle as flock and the OFD locks are held by the
// open file. Written to the pattern of win_mapping.h (DESIGN 306): the SDK's
// declarations type for type, no windows.h, OVERLAPPED the SDK's incomplete
// type with the program's own layout cast to it. A wait with a timeout has
// no call of its own: LOCKFILE_FAIL_IMMEDIATELY tried, a sleep between the
// tries, as on POSIX. Written 2026-10-05 against the Windows 10 SDK's
// fileapi.h and minwinbase.h; compile-checked only (clang -target
// x86_64-pc-windows-msvc -fsyntax-only), not run: io's files are POSIX until
// the platform matrix.
struct _OVERLAPPED;

namespace sgcl::io::detail::win {
    extern "C" {
        __declspec(dllimport) Bool __stdcall LockFileEx(Handle file, Dword flags, Dword reserved, Dword bytes_low, Dword bytes_high, ::_OVERLAPPED* overlapped);
        __declspec(dllimport) Bool __stdcall UnlockFileEx(Handle file, Dword reserved, Dword bytes_low, Dword bytes_high, ::_OVERLAPPED* overlapped);
    }

    // OVERLAPPED on a 64-bit Windows: the status, the bytes, the offset
    // (low and high), the event
    struct Overlapped {
        Size internal;
        Size internal_high;
        Dword offset;
        Dword offset_high;
        Handle event;
    };

    inline constexpr Dword LockfileFailImmediately = 0x00000001;   // LOCKFILE_FAIL_IMMEDIATELY
    inline constexpr Dword LockfileExclusiveLock = 0x00000002;     // LOCKFILE_EXCLUSIVE_LOCK
    inline constexpr Dword ErrorLockViolation = 33;                // ERROR_LOCK_VIOLATION: held by another, with FAIL_IMMEDIATELY
    inline constexpr Dword ErrorIoPending = 997;                   // ERROR_IO_PENDING

    // A range locked, `length` 0 for every byte from `offset` on; 0, or
    // the error of the system (ErrorLockViolation when a try finds it held)
    inline Dword lock_range(Handle file, bool exclusive, bool wait, uint64_t offset, uint64_t length) noexcept {
        Overlapped o = {};
        o.offset = Dword(offset & 0xFFFFFFFFu);
        o.offset_high = Dword(offset >> 32);
        const uint64_t n = length ? length : ~uint64_t(0);
        Dword flags = (exclusive ? LockfileExclusiveLock : 0) | (wait ? 0 : LockfileFailImmediately);
        if (!LockFileEx(file, flags, 0, Dword(n & 0xFFFFFFFFu), Dword(n >> 32), reinterpret_cast<::_OVERLAPPED*>(&o))) {
            return GetLastError();
        }
        return 0;
    }

    inline Dword unlock_range(Handle file, uint64_t offset, uint64_t length) noexcept {
        Overlapped o = {};
        o.offset = Dword(offset & 0xFFFFFFFFu);
        o.offset_high = Dword(offset >> 32);
        const uint64_t n = length ? length : ~uint64_t(0);
        if (!UnlockFileEx(file, 0, Dword(n & 0xFFFFFFFFu), Dword(n >> 32), reinterpret_cast<::_OVERLAPPED*>(&o))) {
            return GetLastError();
        }
        return 0;
    }
}

#endif
