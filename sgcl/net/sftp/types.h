//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../io/fs.h"

#include <cstdint>

// The values SFTP's client and server share: what a stat says about a remote
// file, the attributes a set_stat changes, what statvfs says about a file
// system, the server's limits, the options of a server.
namespace sgcl::net::sftp {
    // What the server says about a file (SFTP v3's attributes,
    // draft-ietf-secsh-filexfer-02 §5): its name (the last element of the
    // path, or a listing's entry), size, type and mode, owner, times. A field
    // the server did not send is zero
    struct file_info {
        string name;
        uint64_t size = 0;
        io::file_type type = io::file_type::unknown;
        io::permissions mode = io::permissions::none;
        uint32_t uid = 0;
        uint32_t gid = 0;
        io::file_time accessed;
        io::file_time modified;

        SGCL_INLINE_HOT bool is_regular() const noexcept {
            return type == io::file_type::regular;
        }

        SGCL_INLINE_HOT bool is_directory() const noexcept {
            return type == io::file_type::directory;
        }

        SGCL_INLINE_HOT bool is_symlink() const noexcept {
            return type == io::file_type::symlink;
        }
    };

    // The attributes a set_stat changes: each one set is sent, the rest left
    // as they are. The times go in whole seconds (SFTP v3), both together:
    // one set alone takes the other from the file
    struct attributes {
        optional<uint64_t> size;             // truncated or extended to it
        optional<uint32_t> uid;              // with gid: both go together
        optional<uint32_t> gid;
        optional<io::permissions> mode;
        optional<io::file_time> accessed;
        optional<io::file_time> modified;
    };

    // What statvfs@openssh.com says about the file system of a path
    // (statvfs(3)'s fields)
    struct file_system_info {
        uint64_t block_size = 0;
        uint64_t fragment_size = 0;
        uint64_t blocks = 0;
        uint64_t blocks_free = 0;
        uint64_t blocks_available = 0;
        uint64_t files = 0;
        uint64_t files_free = 0;
        uint64_t files_available = 0;
        uint64_t id = 0;
        uint64_t flags = 0;                  // 1: read-only, 2: no set-id
        uint64_t max_name_length = 0;
    };

    // The server's limits (limits@openssh.com): the largest packet, the
    // largest read and write, the handles a client may hold open; zero for
    // no limit given
    struct limits {
        uint64_t max_packet_length = 0;
        uint64_t max_read_length = 0;
        uint64_t max_write_length = 0;
        uint64_t max_open_handles = 0;
    };

    // How a server serves a directory
    struct server_options {
        bool read_only = false;              // every change refused (SSH_FX_PERMISSION_DENIED)
        uint32_t max_handles = 256;          // files and directories open at once
        string home = "/";                   // what home-directory and expand-path take "~" for, a path under the root
    };
}
