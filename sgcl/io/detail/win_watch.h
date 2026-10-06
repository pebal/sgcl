//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(_WIN32)

#include "win_mapping.h"

#include <cstddef>
#include <cstdint>

// The changes of a directory on Windows (watch.h: io::watch):
// ReadDirectoryChangesW over a directory's handle (FILE_FLAG_BACKUP_SEMANTICS),
// its whole tree with bWatchSubtree, the records FILE_NOTIFY_INFORMATION
// (an offset to the next, the action, the name's length in bytes, the name in
// UTF-16) read into the ops of io::watch; a file is watched through its
// directory, as on macOS. Written to the pattern of win_mapping.h (DESIGN
// 306): the SDK's declarations type for type, no windows.h. Written
// 2026-10-05 against the Windows 10 SDK's winbase.h, winnt.h and fileapi.h;
// compile-checked only (clang -target x86_64-pc-windows-msvc
// -fsyntax-only), not run: io's files are POSIX until the platform matrix.
struct _OVERLAPPED;

namespace sgcl::io::detail::win {
    extern "C" {
        __declspec(dllimport) Bool __stdcall ReadDirectoryChangesW(Handle directory, void* buffer, Dword length, Bool subtree, Dword filter, Dword* returned,
                                                                   ::_OVERLAPPED* overlapped, void* completion);
        __declspec(dllimport) Bool __stdcall CancelIoEx(Handle file, ::_OVERLAPPED* overlapped);
    }

    inline constexpr Dword FileListDirectory = 0x0001;                 // FILE_LIST_DIRECTORY
    inline constexpr Dword FileFlagBackupSemantics = 0x02000000;       // FILE_FLAG_BACKUP_SEMANTICS
    inline constexpr Dword FileNotifyChangeFileName = 0x001;           // FILE_NOTIFY_CHANGE_FILE_NAME
    inline constexpr Dword FileNotifyChangeDirName = 0x002;            // FILE_NOTIFY_CHANGE_DIR_NAME
    inline constexpr Dword FileNotifyChangeAttributes = 0x004;         // FILE_NOTIFY_CHANGE_ATTRIBUTES
    inline constexpr Dword FileNotifyChangeSize = 0x008;               // FILE_NOTIFY_CHANGE_SIZE
    inline constexpr Dword FileNotifyChangeLastWrite = 0x010;          // FILE_NOTIFY_CHANGE_LAST_WRITE
    inline constexpr Dword FileNotifyChangeSecurity = 0x100;           // FILE_NOTIFY_CHANGE_SECURITY
    inline constexpr Dword FileActionAdded = 1;                        // FILE_ACTION_ADDED
    inline constexpr Dword FileActionRemoved = 2;                      // FILE_ACTION_REMOVED
    inline constexpr Dword FileActionModified = 3;                     // FILE_ACTION_MODIFIED
    inline constexpr Dword FileActionRenamedOldName = 4;               // FILE_ACTION_RENAMED_OLD_NAME
    inline constexpr Dword FileActionRenamedNewName = 5;               // FILE_ACTION_RENAMED_NEW_NAME
    inline constexpr Dword ErrorNotifyEnumDir = 1022;                  // ERROR_NOTIFY_ENUM_DIR: the buffer overflowed, rescan

    inline constexpr Dword WatchFilter = FileNotifyChangeFileName | FileNotifyChangeDirName | FileNotifyChangeAttributes
                                       | FileNotifyChangeSize | FileNotifyChangeLastWrite | FileNotifyChangeSecurity;

    // FILE_NOTIFY_INFORMATION's head; the name follows, `name_bytes` long
    struct NotifyInformation {
        Dword next_entry_offset;
        Dword action;
        Dword name_bytes;
        wchar_t name[1];
    };

    // The ops of io::watch (watch_op's values) for an action
    inline uint8_t watch_ops_of(Dword action) noexcept {
        switch (action) {
            case FileActionAdded: return 1;                 // created
            case FileActionModified: return 2;              // modified
            case FileActionRemoved: return 4;               // removed
            case FileActionRenamedOldName:
            case FileActionRenamedNewName: return 8;        // renamed
        }
        return 0;
    }

    // The records of a buffer ReadDirectoryChangesW filled, each given to f
    // as (name, its length in UTF-16 units, the ops); a record that would
    // run past the buffer ends the walk
    template<class F>
    void each_notification(const void* buffer, Dword bytes, F&& f) {
        const char* at = static_cast<const char*>(buffer);
        const char* end = at + bytes;
        for (;;) {
            if (end - at < Dword(sizeof(NotifyInformation))) {
                return;
            }
            const auto* n = reinterpret_cast<const NotifyInformation*>(at);
            if (n->name_bytes > Dword(end - at) - Dword(offsetof(NotifyInformation, name))) {
                return;
            }
            f(n->name, n->name_bytes / sizeof(wchar_t), watch_ops_of(n->action));
            if (n->next_entry_offset == 0) {
                return;
            }
            at += n->next_entry_offset;
        }
    }
}

#endif
