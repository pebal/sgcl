//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../types.h"
#include "../../error.h"
#include "../../ssh/detail/wire.h"
#include "../../../io/error.h"

#include <cerrno>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/stat.h>

// SFTP version 3 (draft-ietf-secsh-filexfer-02), the version OpenSSH
// speaks, without I/O: the packets' numbers and framing, the attributes both
// ways, the status codes and their errno values, the extensions of OpenSSH's
// PROTOCOL this module speaks. Over the wire types of ssh/detail/wire.h.
namespace sgcl::net::sftp::detail {
    using ssh::detail::Bytes;
    using ssh::detail::Reader;
    using ssh::detail::Span;
    using ssh::detail::Writer;
    using ssh::detail::load32;
    using ssh::detail::store32;

    inline constexpr uint32_t Version = 3;

    enum : uint8_t {
        FxpInit = 1,
        FxpVersion = 2,
        FxpOpen = 3,
        FxpClose = 4,
        FxpRead = 5,
        FxpWrite = 6,
        FxpLstat = 7,
        FxpFstat = 8,
        FxpSetstat = 9,
        FxpFsetstat = 10,
        FxpOpendir = 11,
        FxpReaddir = 12,
        FxpRemove = 13,
        FxpMkdir = 14,
        FxpRmdir = 15,
        FxpRealpath = 16,
        FxpStat = 17,
        FxpRename = 18,
        FxpReadlink = 19,
        FxpSymlink = 20,
        FxpStatus = 101,
        FxpHandle = 102,
        FxpData = 103,
        FxpName = 104,
        FxpAttrs = 105,
        FxpExtended = 200,
        FxpExtendedReply = 201,
    };

    // The open flags (pflags)
    enum : uint32_t {
        FxfRead = 1,
        FxfWrite = 2,
        FxfAppend = 4,
        FxfCreat = 8,
        FxfTrunc = 0x10,
        FxfExcl = 0x20,
    };

    // The attributes' flags
    enum : uint32_t {
        AttrSize = 1,
        AttrUidGid = 2,
        AttrPermissions = 4,
        AttrAcModTime = 8,
        AttrExtended = 0x80000000u,
    };

    // The status codes
    enum : uint32_t {
        FxOk = 0,
        FxEof = 1,
        FxNoSuchFile = 2,
        FxPermissionDenied = 3,
        FxFailure = 4,
        FxBadMessage = 5,
        FxNoConnection = 6,
        FxConnectionLost = 7,
        FxOpUnsupported = 8,
    };

    // The largest packet taken (its length field), as OpenSSH's
    // SFTP_MAX_MSG_LENGTH
    inline constexpr uint32_t MaxPacket = 256 * 1024;
    // The largest read and write asked for and served: a packet less room
    // for its fields
    inline constexpr uint32_t MaxData = MaxPacket - 1024;

    // The extensions of OpenSSH this module speaks, with their versions
    inline constexpr std::pair<std::string_view, std::string_view> extensions[] = {
        {"posix-rename@openssh.com", "1"},
        {"statvfs@openssh.com", "2"},
        {"fstatvfs@openssh.com", "2"},
        {"hardlink@openssh.com", "1"},
        {"fsync@openssh.com", "1"},
        {"lsetstat@openssh.com", "1"},
        {"limits@openssh.com", "1"},
        {"expand-path@openssh.com", "1"},
        {"home-directory", "1"},
    };

    // The attributes as they go on the wire
    struct Attrs {
        uint32_t flags = 0;
        uint64_t size = 0;
        uint32_t uid = 0;
        uint32_t gid = 0;
        uint32_t permissions = 0;   // with the file type's bits (S_IFMT), as OpenSSH sends them
        uint32_t atime = 0;
        uint32_t mtime = 0;
    };

    inline void write_attrs(Writer& w, const Attrs& a) {
        uint32_t flags = a.flags & ~AttrExtended;
        w.u32(flags);
        if (flags & AttrSize) {
            w.u64(a.size);
        }
        if (flags & AttrUidGid) {
            w.u32(a.uid).u32(a.gid);
        }
        if (flags & AttrPermissions) {
            w.u32(a.permissions);
        }
        if (flags & AttrAcModTime) {
            w.u32(a.atime).u32(a.mtime);
        }
    }

    // The attributes read; extended pairs passed over (at most 1024 of them)
    inline bool read_attrs(Reader& r, Attrs& a) noexcept {
        a.flags = r.u32();
        if (a.flags & AttrSize) {
            a.size = r.u64();
        }
        if (a.flags & AttrUidGid) {
            a.uid = r.u32();
            a.gid = r.u32();
        }
        if (a.flags & AttrPermissions) {
            a.permissions = r.u32();
        }
        if (a.flags & AttrAcModTime) {
            a.atime = r.u32();
            a.mtime = r.u32();
        }
        if (a.flags & AttrExtended) {
            uint32_t n = r.u32();
            if (n > 1024) {
                r.fail();
                return false;
            }
            for (uint32_t i = 0; i < n && r.ok(); ++i) {
                (void)r.string();
                (void)r.string();
            }
        }
        return r.ok();
    }

    SGCL_INLINE_HOT io::file_type type_of_mode(uint32_t m) noexcept {
        switch (m & S_IFMT) {
            case S_IFREG: return io::file_type::regular;
            case S_IFDIR: return io::file_type::directory;
            case S_IFLNK: return io::file_type::symlink;
            case S_IFBLK: return io::file_type::block;
            case S_IFCHR: return io::file_type::character;
            case S_IFIFO: return io::file_type::fifo;
            case S_IFSOCK: return io::file_type::socket;
        }
        return io::file_type::unknown;
    }

    SGCL_INLINE_HOT uint32_t mode_of_type(io::file_type t) noexcept {
        switch (t) {
            case io::file_type::regular: return S_IFREG;
            case io::file_type::directory: return S_IFDIR;
            case io::file_type::symlink: return S_IFLNK;
            case io::file_type::block: return S_IFBLK;
            case io::file_type::character: return S_IFCHR;
            case io::file_type::fifo: return S_IFIFO;
            case io::file_type::socket: return S_IFSOCK;
            default: return 0;
        }
    }

    inline file_info info_of(const Attrs& a, std::string_view name) {
        file_info f;
        f.name = string(name);
        f.size = a.size;
        if (a.flags & AttrPermissions) {
            f.type = type_of_mode(a.permissions);
            f.mode = io::permissions(a.permissions & 07777);
        }
        f.uid = a.uid;
        f.gid = a.gid;
        if (a.flags & AttrAcModTime) {
            f.accessed = io::file_time(std::chrono::seconds(a.atime));
            f.modified = io::file_time(std::chrono::seconds(a.mtime));
        }
        return f;
    }

    // A set_stat's attributes on the wire; the times take what the other
    // one of them was from `now_info` when only one is set
    inline Attrs attrs_of(const attributes& at, const file_info* current = nullptr) {
        Attrs a;
        if (at.size) {
            a.flags |= AttrSize;
            a.size = *at.size;
        }
        if (at.uid || at.gid) {
            a.flags |= AttrUidGid;
            a.uid = at.uid ? *at.uid : (current ? current->uid : 0);
            a.gid = at.gid ? *at.gid : (current ? current->gid : 0);
        }
        if (at.mode) {
            a.flags |= AttrPermissions;
            a.permissions = uint32_t(*at.mode) & 07777;
        }
        if (at.accessed || at.modified) {
            a.flags |= AttrAcModTime;
            auto secs = [](io::file_time t) {
                return uint32_t(std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count());
            };
            a.atime = at.accessed ? secs(*at.accessed) : (current ? secs(current->accessed) : 0);
            a.mtime = at.modified ? secs(*at.modified) : (current ? secs(current->modified) : 0);
        }
        return a;
    }

    // A status as the errno value io's predicates read (is_not_found,
    // is_permission), or net's own codes for what errno has no name for
    SGCL_INLINE_HOT error_code status_error(uint32_t code) noexcept {
        switch (code) {
            case FxNoSuchFile: return error_code(ENOENT, std::system_category());
            case FxPermissionDenied: return error_code(EACCES, std::system_category());
            case FxOpUnsupported: return error_code(ENOTSUP, std::system_category());
            case FxEof: return make_error_code(io::errc::unexpected_eof);
            case FxNoConnection:
            case FxConnectionLost: return make_error_code(io::errc::closed);
            case FxBadMessage: return net::make_error_code(net::errc::sftp_protocol);
            default: return net::make_error_code(net::errc::sftp_failure);
        }
    }

    // An errno value of the server's file system as a status
    SGCL_INLINE_HOT uint32_t status_of_errno(int e) noexcept {
        switch (e) {
            case 0: return FxOk;
            case ENOENT:
            case ENOTDIR:
            case ELOOP:
            case ENAMETOOLONG: return FxNoSuchFile;
            case EACCES:
            case EPERM:
            case EROFS: return FxPermissionDenied;
            case ENOSYS:
            case ENOTSUP: return FxOpUnsupported;
            default: return FxFailure;
        }
    }

    SGCL_INLINE_HOT const char* status_text(uint32_t code) noexcept {
        switch (code) {
            case FxOk: return "Success";
            case FxEof: return "End of file";
            case FxNoSuchFile: return "No such file";
            case FxPermissionDenied: return "Permission denied";
            case FxFailure: return "Failure";
            case FxBadMessage: return "Bad message";
            case FxNoConnection: return "No connection";
            case FxConnectionLost: return "Connection lost";
            case FxOpUnsupported: return "Operation unsupported";
        }
        return "Unknown status";
    }

    // A packet started: its length's place, its type and (but INIT and
    // VERSION) its id; end_packet fills the length
    SGCL_INLINE_HOT size_t begin_packet(Writer& w, uint8_t type) {
        size_t at = w.begin_string();
        w.u8(type);
        return at;
    }

    SGCL_INLINE_HOT void end_packet(Writer& w, size_t at) noexcept {
        w.end_string(at);
    }
}
