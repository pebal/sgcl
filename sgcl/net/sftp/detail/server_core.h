//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "fs.h"
#include "protocol.h"

#include <cstdio>
#include <ctime>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// An SFTP server's work without I/O: a request taken, its answer made, over a
// tree (fs.h) whose every path went through the resolver first. The version
// (INIT, VERSION 3 with OpenSSH's extensions), the handles of open files
// and directories (at most server_options::max_handles), each request's
// rules: a path followed or not on its last component as POSIX does it, a
// read-only server refusing every change, OpenSSH's SYMLINK order (the
// target first, the link second: what its sftp-server and sftp send, the
// reverse of draft-02's text).
namespace sgcl::net::sftp::detail {
    class ServerCore {
    public:
        ServerCore(std::unique_ptr<Fs> fs, const server_options& o)
        : _fs(std::move(fs))
        , _read_only(o.read_only)
        , _max_handles(o.max_handles ? o.max_handles : 1)
        , _home(std::string(o.home.view())) {
            std::string rel;
            if (resolve(*_fs, _home, "/", true, rel) != 0) {
                rel.clear();
            }
            _home = "/" + rel;
        }

        ServerCore(const ServerCore&) = delete;

        ~ServerCore() {
            for (auto& [id, h] : _handles) {
                if (!h.dir) {
                    _fs->close(h.fh);
                }
            }
        }

        // One packet (its type and payload, without the length) taken, its
        // answer appended to out with its length; false when the session
        // must end (no INIT first, a packet that cannot be read)
        bool handle(const uint8_t* p, size_t n, Bytes& out) {
            Reader r(p, n);
            const uint8_t type = r.u8();
            if (!r.ok()) {
                return false;
            }
            if (!_started) {
                if (type != FxpInit) {
                    return false;
                }
                (void)r.u32();   // the client's version: 3 is answered whatever it says
                _started = true;
                Writer w(out);
                size_t at = begin_packet(w, FxpVersion);
                w.u32(Version);
                for (auto& [name, version] : extensions) {
                    w.string(name).string(version);
                }
                end_packet(w, at);
                return true;
            }
            const uint32_t id = r.u32();
            if (!r.ok()) {
                return false;
            }
            _request(type, id, r, out);
            return true;
        }

        SGCL_INLINE_HOT size_t open_handles() const noexcept {
            return _handles.size();
        }

    private:
        struct Handle {
            bool dir = false;
            int fh = -1;
            std::vector<DirEntry> listing;
            size_t pos = 0;
            bool listed = false;
        };

        void _status(Bytes& out, uint32_t id, uint32_t code, std::string_view message = {}) {
            Writer w(out);
            size_t at = begin_packet(w, FxpStatus);
            w.u32(id).u32(code).string(message.empty() ? std::string_view(status_text(code)) : message).string("");
            end_packet(w, at);
        }

        void _errno(Bytes& out, uint32_t id, int e) {
            const uint32_t code = status_of_errno(e);
            if (code == FxFailure) {
                _status(out, id, code, std::strerror(e));
            } else {
                _status(out, id, code);
            }
        }

        void _handle_reply(Bytes& out, uint32_t id, uint32_t h) {
            Writer w(out);
            size_t at = begin_packet(w, FxpHandle);
            uint8_t b[4];
            store32(b, h);
            w.u32(id).string(b, 4);
            end_packet(w, at);
        }

        void _attrs_reply(Bytes& out, uint32_t id, const Attrs& a) {
            Writer w(out);
            size_t at = begin_packet(w, FxpAttrs);
            w.u32(id);
            write_attrs(w, a);
            end_packet(w, at);
        }

        void _name_reply(Bytes& out, uint32_t id, std::string_view name) {
            Writer w(out);
            size_t at = begin_packet(w, FxpName);
            w.u32(id).u32(1).string(name).string(name);
            write_attrs(w, Attrs{});
            end_packet(w, at);
        }

        // The handle a request names, or null
        Handle* _get(const Span& s, uint32_t& key) {
            if (s.n != 4) {
                return nullptr;
            }
            key = load32(s.p);
            auto it = _handles.find(key);
            return it == _handles.end() ? nullptr : &it->second;
        }

        bool _room() const noexcept {
            return _handles.size() < _max_handles;
        }

        uint32_t _add(Handle h) {
            while (_handles.count(_next) || _next == 0) {
                ++_next;
            }
            uint32_t key = _next++;
            _handles.emplace(key, std::move(h));
            return key;
        }

        int _path(const Span& s, bool follow, std::string& rel) {
            return resolve(*_fs, s.view(), _home, follow, rel);
        }

        // "~" and "~/…" of expand-path: the home's
        bool _expand(std::string_view p, std::string& out) {
            if (p.empty() || p[0] != '~') {
                out.assign(p);
                return true;
            }
            if (p.size() > 1 && p[1] != '/') {
                return false;   // another user's home: there is none here
            }
            out = _home + std::string(p.substr(1));
            return true;
        }

        static std::string _longname(const DirEntry& e) {
            const uint32_t m = e.attrs.permissions;
            char kind = '-';
            switch (m & S_IFMT) {
                case S_IFDIR: kind = 'd'; break;
                case S_IFLNK: kind = 'l'; break;
                case S_IFCHR: kind = 'c'; break;
                case S_IFBLK: kind = 'b'; break;
                case S_IFIFO: kind = 'p'; break;
                case S_IFSOCK: kind = 's'; break;
                default: break;
            }
            const char rwx[] = "rwxrwxrwx";
            char perm[11];
            perm[0] = kind;
            for (int i = 0; i < 9; ++i) {
                perm[1 + i] = (m & (0400 >> i)) ? rwx[i] : '-';
            }
            perm[10] = 0;
            char when[32] = "";
            time_t t = time_t(e.attrs.mtime);
            struct tm tmv;
            if (::gmtime_r(&t, &tmv)) {
                std::strftime(when, sizeof when, "%b %e %H:%M", &tmv);
            }
            char line[512];
            std::snprintf(line, sizeof line, "%s    1 %-8u %-8u %8llu %s ", perm, e.attrs.uid, e.attrs.gid, (unsigned long long)e.attrs.size, when);
            return std::string(line) + e.name;
        }

        void _request(uint8_t type, uint32_t id, Reader& r, Bytes& out) {
            switch (type) {
                case FxpOpen: {
                    Span path = r.string();
                    uint32_t pflags = r.u32();
                    Attrs a;
                    if (!read_attrs(r, a)) {
                        return _status(out, id, FxBadMessage);
                    }
                    const bool change = pflags & (FxfWrite | FxfAppend | FxfCreat | FxfTrunc);
                    if (_read_only && change) {
                        return _status(out, id, FxPermissionDenied);
                    }
                    if (!(pflags & (FxfRead | FxfWrite))) {
                        pflags |= FxfRead;
                    }
                    if (!_room()) {
                        return _status(out, id, FxFailure, "too many open handles");
                    }
                    std::string rel;
                    if (int e = _path(path, true, rel); e) {
                        return _errno(out, id, e);
                    }
                    int fh;
                    if (int e = _fs->open(rel, pflags, (a.flags & AttrPermissions) ? a.permissions : 0666, fh); e) {
                        return _errno(out, id, e);
                    }
                    Handle h;
                    h.fh = fh;
                    return _handle_reply(out, id, _add(std::move(h)));
                }
                case FxpClose: {
                    Span hs = r.string();
                    uint32_t key;
                    Handle* h = _get(hs, key);
                    if (!r.ok() || !h) {
                        return _status(out, id, FxFailure, "no such handle");
                    }
                    if (!h->dir) {
                        _fs->close(h->fh);
                    }
                    _handles.erase(key);
                    return _status(out, id, FxOk);
                }
                case FxpRead: {
                    Span hs = r.string();
                    uint64_t offset = r.u64();
                    uint32_t len = r.u32();
                    uint32_t key;
                    Handle* h = _get(hs, key);
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (!h || h->dir) {
                        return _status(out, id, FxFailure, "no such file handle");
                    }
                    len = std::min(len, MaxData);
                    Writer w(out);
                    size_t at = begin_packet(w, FxpData);
                    w.u32(id);
                    size_t len_at = w.begin_string();
                    size_t data_at = out.size();
                    out.resize(data_at + len);
                    size_t got = 0;
                    int e = _fs->read(h->fh, offset, out.data() + data_at, len, got);
                    if (e || got == 0) {
                        out.resize(at);
                        return e ? _errno(out, id, e) : _status(out, id, FxEof);
                    }
                    out.resize(data_at + got);
                    w.end_string(len_at);
                    end_packet(w, at);
                    return;
                }
                case FxpWrite: {
                    Span hs = r.string();
                    uint64_t offset = r.u64();
                    Span data = r.string();
                    uint32_t key;
                    Handle* h = _get(hs, key);
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (!h || h->dir) {
                        return _status(out, id, FxFailure, "no such file handle");
                    }
                    if (_read_only) {
                        return _status(out, id, FxPermissionDenied);
                    }
                    if (int e = _fs->write(h->fh, offset, data.p, data.n); e) {
                        return _errno(out, id, e);
                    }
                    return _status(out, id, FxOk);
                }
                case FxpLstat:
                case FxpStat: {
                    Span path = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    std::string rel;
                    if (int e = _path(path, type == FxpStat, rel); e) {
                        return _errno(out, id, e);
                    }
                    Attrs a;
                    if (int e = _fs->lstat(rel, a); e) {
                        return _errno(out, id, e);
                    }
                    return _attrs_reply(out, id, a);
                }
                case FxpFstat: {
                    Span hs = r.string();
                    uint32_t key;
                    Handle* h = _get(hs, key);
                    if (!r.ok() || !h || h->dir) {
                        return _status(out, id, FxFailure, "no such file handle");
                    }
                    Attrs a;
                    if (int e = _fs->fstat(h->fh, a); e) {
                        return _errno(out, id, e);
                    }
                    return _attrs_reply(out, id, a);
                }
                case FxpSetstat:
                case FxpFsetstat: {
                    Span target = r.string();
                    Attrs a;
                    if (!read_attrs(r, a)) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (_read_only) {
                        return _status(out, id, FxPermissionDenied);
                    }
                    if (type == FxpFsetstat) {
                        uint32_t key;
                        Handle* h = _get(target, key);
                        if (!h || h->dir) {
                            return _status(out, id, FxFailure, "no such file handle");
                        }
                        if (int e = _fs->fsetstat(h->fh, a); e) {
                            return _errno(out, id, e);
                        }
                        return _status(out, id, FxOk);
                    }
                    std::string rel;
                    if (int e = _path(target, true, rel); e) {
                        return _errno(out, id, e);
                    }
                    if (int e = _fs->setstat(rel, a, true); e) {
                        return _errno(out, id, e);
                    }
                    return _status(out, id, FxOk);
                }
                case FxpOpendir: {
                    Span path = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (!_room()) {
                        return _status(out, id, FxFailure, "too many open handles");
                    }
                    std::string rel;
                    if (int e = _path(path, true, rel); e) {
                        return _errno(out, id, e);
                    }
                    Handle h;
                    h.dir = true;
                    if (int e = _fs->list(rel, h.listing); e) {
                        return _errno(out, id, e);
                    }
                    return _handle_reply(out, id, _add(std::move(h)));
                }
                case FxpReaddir: {
                    Span hs = r.string();
                    uint32_t key;
                    Handle* h = _get(hs, key);
                    if (!r.ok() || !h || !h->dir) {
                        return _status(out, id, FxFailure, "no such directory handle");
                    }
                    if (h->pos >= h->listing.size()) {
                        return _status(out, id, FxEof);
                    }
                    const size_t count = std::min<size_t>(100, h->listing.size() - h->pos);
                    Writer w(out);
                    size_t at = begin_packet(w, FxpName);
                    w.u32(id).u32(uint32_t(count));
                    for (size_t i = 0; i < count; ++i) {
                        const DirEntry& e = h->listing[h->pos + i];
                        w.string(e.name).string(_longname(e));
                        write_attrs(w, e.attrs);
                    }
                    h->pos += count;
                    end_packet(w, at);
                    return;
                }
                case FxpRemove:
                case FxpRmdir: {
                    Span path = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (_read_only) {
                        return _status(out, id, FxPermissionDenied);
                    }
                    std::string rel;
                    if (int e = _path(path, false, rel); e) {
                        return _errno(out, id, e);
                    }
                    int e = type == FxpRemove ? _fs->remove(rel) : _fs->rmdir(rel);
                    return e ? _errno(out, id, e) : _status(out, id, FxOk);
                }
                case FxpMkdir: {
                    Span path = r.string();
                    Attrs a;
                    if (!read_attrs(r, a)) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (_read_only) {
                        return _status(out, id, FxPermissionDenied);
                    }
                    std::string rel;
                    if (int e = _path(path, false, rel); e) {
                        return _errno(out, id, e);
                    }
                    int e = _fs->mkdir(rel, (a.flags & AttrPermissions) ? a.permissions : 0777);
                    return e ? _errno(out, id, e) : _status(out, id, FxOk);
                }
                case FxpRealpath: {
                    Span path = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    std::string rel;
                    if (int e = _path(path, true, rel); e) {
                        return _errno(out, id, e);
                    }
                    return _name_reply(out, id, "/" + rel);
                }
                case FxpRename: {
                    Span from = r.string();
                    Span to = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    return _rename(out, id, from, to, false);
                }
                case FxpReadlink: {
                    Span path = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    std::string rel, target;
                    if (int e = _path(path, false, rel); e) {
                        return _errno(out, id, e);
                    }
                    if (int e = _fs->readlink(rel, target); e) {
                        return _errno(out, id, e);
                    }
                    return _name_reply(out, id, target);
                }
                case FxpSymlink: {
                    // OpenSSH's order: the target first, the link second
                    Span target = r.string();
                    Span link = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (_read_only) {
                        return _status(out, id, FxPermissionDenied);
                    }
                    if (target.view().find('\0') != std::string_view::npos || target.n > 4096) {
                        return _status(out, id, FxFailure, "a target with a NUL");
                    }
                    std::string rel;
                    if (int e = _path(link, false, rel); e) {
                        return _errno(out, id, e);
                    }
                    int e = _fs->symlink(std::string(target.view()), rel);
                    return e ? _errno(out, id, e) : _status(out, id, FxOk);
                }
                case FxpExtended: {
                    Span name = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    return _extended(name.view(), id, r, out);
                }
                default:
                    return _status(out, id, FxOpUnsupported);
            }
        }

        void _rename(Bytes& out, uint32_t id, const Span& from, const Span& to, bool replace) {
            if (_read_only) {
                return _status(out, id, FxPermissionDenied);
            }
            std::string a, b;
            if (int e = _path(from, false, a); e) {
                return _errno(out, id, e);
            }
            if (int e = _path(to, false, b); e) {
                return _errno(out, id, e);
            }
            int e = _fs->rename(a, b, replace);
            return e ? _errno(out, id, e) : _status(out, id, FxOk);
        }

        void _extended(std::string_view name, uint32_t id, Reader& r, Bytes& out) {
            if (name == "posix-rename@openssh.com") {
                Span from = r.string();
                Span to = r.string();
                if (!r.ok()) {
                    return _status(out, id, FxBadMessage);
                }
                return _rename(out, id, from, to, true);
            }
            if (name == "hardlink@openssh.com") {
                Span from = r.string();
                Span to = r.string();
                if (!r.ok()) {
                    return _status(out, id, FxBadMessage);
                }
                if (_read_only) {
                    return _status(out, id, FxPermissionDenied);
                }
                std::string a, b;
                if (int e = _path(from, true, a); e) {   // the file a symlink names, within the root
                    return _errno(out, id, e);
                }
                if (int e = _path(to, false, b); e) {
                    return _errno(out, id, e);
                }
                int e = _fs->link(a, b);
                return e ? _errno(out, id, e) : _status(out, id, FxOk);
            }
            if (name == "statvfs@openssh.com" || name == "fstatvfs@openssh.com") {
                std::string rel;
                if (name == "statvfs@openssh.com") {
                    Span path = r.string();
                    if (!r.ok()) {
                        return _status(out, id, FxBadMessage);
                    }
                    if (int e = _path(path, true, rel); e) {
                        return _errno(out, id, e);
                    }
                } else {
                    Span hs = r.string();
                    uint32_t key;
                    if (!r.ok() || !_get(hs, key)) {
                        return _status(out, id, FxFailure, "no such handle");
                    }
                    rel = "";   // the file system of the served tree
                }
                file_system_info v;
                if (int e = _fs->statvfs(rel, v); e) {
                    return _errno(out, id, e);
                }
                Writer w(out);
                size_t at = begin_packet(w, FxpExtendedReply);
                w.u32(id).u64(v.block_size).u64(v.fragment_size).u64(v.blocks).u64(v.blocks_free).u64(v.blocks_available).u64(v.files).u64(v.files_free).u64(v.files_available)
                    .u64(v.id).u64(v.flags).u64(v.max_name_length);
                end_packet(w, at);
                return;
            }
            if (name == "fsync@openssh.com") {
                Span hs = r.string();
                uint32_t key;
                Handle* h = _get(hs, key);
                if (!r.ok() || !h || h->dir) {
                    return _status(out, id, FxFailure, "no such file handle");
                }
                int e = _fs->fsync(h->fh);
                return e ? _errno(out, id, e) : _status(out, id, FxOk);
            }
            if (name == "lsetstat@openssh.com") {
                Span path = r.string();
                Attrs a;
                if (!read_attrs(r, a)) {
                    return _status(out, id, FxBadMessage);
                }
                if (_read_only) {
                    return _status(out, id, FxPermissionDenied);
                }
                std::string rel;
                if (int e = _path(path, false, rel); e) {
                    return _errno(out, id, e);
                }
                int e = _fs->setstat(rel, a, false);
                return e ? _errno(out, id, e) : _status(out, id, FxOk);
            }
            if (name == "limits@openssh.com") {
                Writer w(out);
                size_t at = begin_packet(w, FxpExtendedReply);
                w.u32(id).u64(MaxPacket).u64(MaxData).u64(MaxData).u64(_max_handles);
                end_packet(w, at);
                return;
            }
            if (name == "expand-path@openssh.com") {
                Span path = r.string();
                if (!r.ok()) {
                    return _status(out, id, FxBadMessage);
                }
                std::string p, rel;
                if (!_expand(path.view(), p)) {
                    return _status(out, id, FxFailure, "no such user");
                }
                if (int e = resolve(*_fs, p, _home, true, rel); e) {
                    return _errno(out, id, e);
                }
                return _name_reply(out, id, "/" + rel);
            }
            if (name == "home-directory") {
                (void)r.string();   // the user: one home here, the session's
                return _name_reply(out, id, _home);
            }
            return _status(out, id, FxOpUnsupported);
        }

        std::unique_ptr<Fs> _fs;
        bool _read_only;
        uint32_t _max_handles;
        std::string _home;
        bool _started = false;
        std::map<uint32_t, Handle> _handles;
        uint32_t _next = 1;
    };
}
