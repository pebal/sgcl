//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "protocol.h"
#include "../../../core/detail/bytes.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <unistd.h>
#include <utility>
#include <vector>

// What an SFTP server serves: a tree under a root, reached only through
// paths that a resolver made safe. The resolver takes the client's path (as
// a chroot would: "/" is the root, a relative path is under the server's
// home) and walks it a component at a time, ".." stopping at the root, each
// symlink on the way read and followed within the root (an absolute target
// starts again at the root, a relative one from the link's directory, at
// most 40 of them), so that what it gives — a path of plain components
// under the root, none of them a symlink but maybe the last — never leaves
// it, whatever the names, the symlinks or the ".." of the request.
//
// Two trees: the disk under a directory (DiskFs, what serve() serves), and
// one in memory (MemoryFs, for the tests and the fuzzer: the same requests
// over a tree that has no outside). The paths they take are relative to
// their root, components joined by "/", "" for the root itself.
namespace sgcl::net::sftp::detail {
    // A directory's entry: its name and attributes (of the entry itself,
    // not of what a symlink points to)
    struct DirEntry {
        std::string name;
        Attrs attrs;
    };

    class Fs {
    public:
        virtual ~Fs() = default;

        // Each returns 0 or an errno value
        virtual int lstat(const std::string& rel, Attrs& out) = 0;
        virtual int readlink(const std::string& rel, std::string& out) = 0;
        virtual int open(const std::string& rel, uint32_t pflags, uint32_t mode, int& handle) = 0;
        virtual int read(int handle, uint64_t offset, uint8_t* buf, size_t n, size_t& got) = 0;
        virtual int write(int handle, uint64_t offset, const uint8_t* data, size_t n) = 0;
        virtual int fstat(int handle, Attrs& out) = 0;
        virtual int fsetstat(int handle, const Attrs& a) = 0;
        virtual int fsync(int handle) = 0;
        virtual void close(int handle) = 0;
        virtual int list(const std::string& rel, std::vector<DirEntry>& out) = 0;
        virtual int setstat(const std::string& rel, const Attrs& a, bool follow) = 0;
        virtual int remove(const std::string& rel) = 0;
        virtual int mkdir(const std::string& rel, uint32_t mode) = 0;
        virtual int rmdir(const std::string& rel) = 0;
        virtual int rename(const std::string& from, const std::string& to, bool replace) = 0;
        virtual int symlink(const std::string& target, const std::string& link) = 0;
        virtual int link(const std::string& from, const std::string& to) = 0;
        virtual int statvfs(const std::string& rel, file_system_info& out) = 0;
    };

    inline constexpr int MaxSymlinks = 40;

    SGCL_INLINE_HOT std::string join_rel(const std::vector<std::string>& parts) {
        std::string s;
        for (const auto& p : parts) {
            if (!s.empty()) {
                s += '/';
            }
            s += p;
        }
        return s;
    }

    inline void split_into(std::string_view path, std::vector<std::string>& out) {
        size_t from = 0;
        while (from <= path.size()) {
            size_t slash = path.find('/', from);
            std::string_view c = path.substr(from, slash == std::string_view::npos ? std::string_view::npos : slash - from);
            if (!c.empty()) {
                out.emplace_back(c);
            }
            if (slash == std::string_view::npos) {
                break;
            }
            from = slash + 1;
        }
    }

    // The client's path resolved under the root: `rel`, components with no
    // symlink among them (the last one followed too when follow_last), and
    // whether the last one is there. 0, or an errno value: ENOENT for a
    // directory on the way that is not there, ENOTDIR, ELOOP past 40
    // symlinks, EINVAL for a path with a NUL
    inline int resolve(Fs& fs, std::string_view path, std::string_view home, bool follow_last, std::string& rel) {
        if (path.find('\0') != std::string_view::npos || path.size() > 4096) {
            return EINVAL;
        }
        std::vector<std::string> pending;   // in reverse: back() is the next
        {
            std::vector<std::string> parts;
            if (path.empty() || path[0] != '/') {
                split_into(home, parts);
            }
            split_into(path, parts);
            pending.assign(parts.rbegin(), parts.rend());
        }
        std::vector<std::string> done;
        int links = 0;
        while (!pending.empty()) {
            std::string c = std::move(pending.back());
            pending.pop_back();
            if (c == ".") {
                continue;
            }
            if (c == "..") {
                if (!done.empty()) {
                    done.pop_back();
                }
                continue;
            }
            const bool last = pending.empty();
            done.push_back(c);
            if (last && !follow_last) {
                break;
            }
            Attrs a;
            const std::string here = join_rel(done);
            int e = fs.lstat(here, a);
            if (e == ENOENT && last) {
                break;   // a name to make
            }
            if (e) {
                return e;
            }
            const uint32_t type = a.permissions & S_IFMT;
            if (type == S_IFLNK) {
                if (++links > MaxSymlinks) {
                    return ELOOP;
                }
                std::string target;
                if (int r = fs.readlink(here, target); r) {
                    return r;
                }
                if (target.find('\0') != std::string::npos) {
                    return EINVAL;
                }
                done.pop_back();
                if (!target.empty() && target[0] == '/') {
                    done.clear();   // within the root, as a chroot reads it
                }
                std::vector<std::string> parts;
                split_into(target, parts);
                for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
                    pending.push_back(*it);
                }
                continue;
            }
            if (!last && type != S_IFDIR) {
                return ENOTDIR;
            }
        }
        rel = join_rel(done);
        return 0;
    }

    // --- the disk -----------------------------------------------------------

    inline Attrs attrs_of_stat(const struct ::stat& st) {
        Attrs a;
        a.flags = AttrSize | AttrUidGid | AttrPermissions | AttrAcModTime;
        a.size = uint64_t(st.st_size);
        a.uid = st.st_uid;
        a.gid = st.st_gid;
        a.permissions = uint32_t(st.st_mode);
#if defined(__APPLE__)
        a.atime = uint32_t(st.st_atimespec.tv_sec);
        a.mtime = uint32_t(st.st_mtimespec.tv_sec);
#else
        a.atime = uint32_t(st.st_atim.tv_sec);
        a.mtime = uint32_t(st.st_mtim.tv_sec);
#endif
        return a;
    }

    // A directory of the disk as the tree. Every call reaches its path from
    // a descriptor of the root, a directory at a time (openat, O_NOFOLLOW),
    // and acts on the last element by the *at calls without following it:
    // the resolver followed the symlinks, so a symlink met here was put in
    // place meanwhile (by another session, another process) and is refused
    // rather than followed out of the root. New files are made with O_EXCL
    // where asked
    class DiskFs final : public Fs {
    public:
        explicit DiskFs(const std::string& root) {
            _root = ::open(root.c_str(), SearchFlags | O_DIRECTORY | O_CLOEXEC);
            _root_error = _root < 0 ? errno : 0;
        }

        DiskFs(const DiskFs&) = delete;
        DiskFs& operator=(const DiskFs&) = delete;

        ~DiskFs() override {
            for (auto& [h, fd] : _files) {
                ::close(fd);
            }
            if (_root >= 0) {
                ::close(_root);
            }
        }

        int lstat(const std::string& rel, Attrs& out) override {
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            struct ::stat st;
            if (::fstatat(p.fd, p.leaf.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
                return errno;
            }
            out = attrs_of_stat(st);
            return 0;
        }

        int readlink(const std::string& rel, std::string& out) override {
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            char buf[4096];
            ssize_t n = ::readlinkat(p.fd, p.leaf.c_str(), buf, sizeof buf);
            if (n < 0) {
                return errno;
            }
            out.assign(buf, size_t(n));
            return 0;
        }

        int open(const std::string& rel, uint32_t pflags, uint32_t mode, int& handle) override {
            int flags = O_CLOEXEC | O_NOFOLLOW;
            const bool rd = pflags & FxfRead, wr = pflags & FxfWrite;
            flags |= rd && wr ? O_RDWR : wr ? O_WRONLY : O_RDONLY;
            if (pflags & FxfAppend) {
                flags |= O_APPEND;
            }
            if (pflags & FxfCreat) {
                flags |= O_CREAT;
            }
            if (pflags & FxfTrunc) {
                flags |= O_TRUNC;
            }
            if (pflags & FxfExcl) {
                flags |= O_EXCL;
            }
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            int fd = ::openat(p.fd, p.leaf.c_str(), flags | O_NONBLOCK, mode_t(mode & 07777));
            if (fd < 0) {
                return errno;
            }
            struct ::stat st;
            if (::fstat(fd, &st) == 0 && !S_ISREG(st.st_mode)) {
                ::close(fd);   // a FIFO, a device, a directory: not a file to serve
                return S_ISDIR(st.st_mode) ? EISDIR : EPERM;
            }
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) & ~O_NONBLOCK);
            handle = _next++;
            _files[handle] = fd;
            if (pflags & FxfAppend) {
                _append.push_back(handle);
            }
            return 0;
        }

        int read(int handle, uint64_t offset, uint8_t* buf, size_t n, size_t& got) override {
            int fd = _fd(handle);
            if (fd < 0) {
                return EBADF;
            }
            ssize_t r;
            do {
                r = ::pread(fd, buf, n, off_t(offset));
            } while (r < 0 && errno == EINTR);
            if (r < 0) {
                return errno;
            }
            got = size_t(r);
            return 0;
        }

        int write(int handle, uint64_t offset, const uint8_t* data, size_t n) override {
            int fd = _fd(handle);
            if (fd < 0) {
                return EBADF;
            }
            const bool append = std::find(_append.begin(), _append.end(), handle) != _append.end();
            while (n) {
                // a file opened to append is written at its end, the offset
                // aside (pwrite honours O_APPEND on Linux, not on macOS)
                ssize_t r = append ? ::write(fd, data, n) : ::pwrite(fd, data, n, off_t(offset));
                if (r < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return errno;
                }
                data += r;
                n -= size_t(r);
                offset += uint64_t(r);
            }
            return 0;
        }

        int fstat(int handle, Attrs& out) override {
            int fd = _fd(handle);
            struct ::stat st;
            if (fd < 0 || ::fstat(fd, &st) != 0) {
                return fd < 0 ? EBADF : errno;
            }
            out = attrs_of_stat(st);
            return 0;
        }

        int fsetstat(int handle, const Attrs& a) override {
            int fd = _fd(handle);
            if (fd < 0) {
                return EBADF;
            }
            if ((a.flags & AttrSize) && ::ftruncate(fd, off_t(a.size)) != 0) {
                return errno;
            }
            if ((a.flags & AttrPermissions) && ::fchmod(fd, mode_t(a.permissions & 07777)) != 0) {
                return errno;
            }
            if ((a.flags & AttrUidGid) && ::fchown(fd, a.uid, a.gid) != 0) {
                return errno;
            }
            if (a.flags & AttrAcModTime) {
                struct timeval tv[2] = {{time_t(a.atime), 0}, {time_t(a.mtime), 0}};
                if (::futimes(fd, tv) != 0) {
                    return errno;
                }
            }
            return 0;
        }

        int fsync(int handle) override {
            int fd = _fd(handle);
            if (fd < 0) {
                return EBADF;
            }
            return ::fsync(fd) == 0 ? 0 : errno;
        }

        void close(int handle) override {
            auto it = _files.find(handle);
            if (it != _files.end()) {
                ::close(it->second);
                _files.erase(it);
            }
            _append.erase(std::remove(_append.begin(), _append.end(), handle), _append.end());
        }

        int list(const std::string& rel, std::vector<DirEntry>& out) override {
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            int dfd = ::openat(p.fd, p.leaf.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (dfd < 0) {
                return errno;
            }
            DIR* d = ::fdopendir(dfd);
            if (!d) {
                int e = errno;
                ::close(dfd);
                return e;
            }
            while (struct dirent* e = ::readdir(d)) {
                std::string_view name(e->d_name);
                if (name == "." || name == "..") {
                    continue;
                }
                struct ::stat st;
                if (::fstatat(dfd, e->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
                    continue;   // gone meanwhile
                }
                out.push_back(DirEntry{std::string(name), attrs_of_stat(st)});
            }
            ::closedir(d);
            std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) { return a.name < b.name; });
            return 0;
        }

        // The path's last element is never followed: with follow, the
        // resolver followed the symlinks already, and one met here came
        // meanwhile; without, a symlink's own attributes are set
        int setstat(const std::string& rel, const Attrs& a, bool follow) override {
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            if (a.flags & AttrSize) {
                int fd = ::openat(p.fd, p.leaf.c_str(), O_WRONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
                if (fd < 0) {
                    return errno == ELOOP && !follow ? EINVAL : errno;   // a symlink has no size to set
                }
                int r = ::ftruncate(fd, off_t(a.size));
                int e = errno;
                ::close(fd);
                if (r != 0) {
                    return e;
                }
            }
            if ((a.flags & AttrPermissions) && ::fchmodat(p.fd, p.leaf.c_str(), mode_t(a.permissions & 07777), AT_SYMLINK_NOFOLLOW) != 0) {
                return errno;
            }
            if ((a.flags & AttrUidGid) && ::fchownat(p.fd, p.leaf.c_str(), a.uid, a.gid, AT_SYMLINK_NOFOLLOW) != 0) {
                return errno;
            }
            if (a.flags & AttrAcModTime) {
                struct timespec ts[2] = {{time_t(a.atime), 0}, {time_t(a.mtime), 0}};
                if (::utimensat(p.fd, p.leaf.c_str(), ts, AT_SYMLINK_NOFOLLOW) != 0) {
                    return errno;
                }
            }
            return 0;
        }

        int remove(const std::string& rel) override {
            if (rel.empty()) {
                return EPERM;
            }
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            struct ::stat st;
            if (::fstatat(p.fd, p.leaf.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
                return errno;
            }
            if (S_ISDIR(st.st_mode)) {
                return EISDIR;
            }
            return ::unlinkat(p.fd, p.leaf.c_str(), 0) == 0 ? 0 : errno;
        }

        int mkdir(const std::string& rel, uint32_t mode) override {
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            return ::mkdirat(p.fd, p.leaf.c_str(), mode_t(mode & 07777)) == 0 ? 0 : errno;
        }

        int rmdir(const std::string& rel) override {
            if (rel.empty()) {
                return EPERM;   // the root itself
            }
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            return ::unlinkat(p.fd, p.leaf.c_str(), AT_REMOVEDIR) == 0 ? 0 : errno;
        }

        int rename(const std::string& from, const std::string& to, bool replace) override {
            if (from.empty() || to.empty()) {
                return EPERM;
            }
            Parent pf, pt;
            if (int e = _parent(from, pf); e) {
                return e;
            }
            if (int e = _parent(to, pt); e) {
                return e;
            }
            if (!replace) {
                struct ::stat st;
                if (::fstatat(pt.fd, pt.leaf.c_str(), &st, AT_SYMLINK_NOFOLLOW) == 0) {
                    return EEXIST;
                }
            }
            return ::renameat(pf.fd, pf.leaf.c_str(), pt.fd, pt.leaf.c_str()) == 0 ? 0 : errno;
        }

        int symlink(const std::string& target, const std::string& link) override {
            Parent p;
            if (int e = _parent(link, p); e) {
                return e;
            }
            return ::symlinkat(target.c_str(), p.fd, p.leaf.c_str()) == 0 ? 0 : errno;
        }

        int link(const std::string& from, const std::string& to) override {
            Parent pf, pt;
            if (int e = _parent(from, pf); e) {
                return e;
            }
            if (int e = _parent(to, pt); e) {
                return e;
            }
            // never a symlink (macOS's link follows one, whatever it points
            // at) nor a directory
            struct ::stat st;
            if (::fstatat(pf.fd, pf.leaf.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) {
                return errno;
            }
            if (S_ISLNK(st.st_mode) || S_ISDIR(st.st_mode)) {
                return EPERM;
            }
            return ::linkat(pf.fd, pf.leaf.c_str(), pt.fd, pt.leaf.c_str(), 0) == 0 ? 0 : errno;
        }

        int statvfs(const std::string& rel, file_system_info& out) override {
            Parent p;
            if (int e = _parent(rel, p); e) {
                return e;
            }
            struct ::statvfs v;
            int fd = ::openat(p.fd, p.leaf.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
            const int r = fd >= 0 ? ::fstatvfs(fd, &v) : ::fstatvfs(p.fd, &v);   // unreadable: its directory's
            const int e = errno;
            if (fd >= 0) {
                ::close(fd);
            }
            if (r != 0) {
                return e;
            }
            out.block_size = v.f_bsize;
            out.fragment_size = v.f_frsize;
            out.blocks = v.f_blocks;
            out.blocks_free = v.f_bfree;
            out.blocks_available = v.f_bavail;
            out.files = v.f_files;
            out.files_free = v.f_ffree;
            out.files_available = v.f_favail;
            out.id = v.f_fsid;
            out.flags = (v.f_flag & ST_RDONLY ? 1 : 0) | (v.f_flag & ST_NOSUID ? 2 : 0);
            out.max_name_length = v.f_namemax;
            return 0;
        }

    private:
#if defined(O_SEARCH)
        static constexpr int SearchFlags = O_SEARCH;   // a directory to walk, not to read
#elif defined(O_PATH)
        static constexpr int SearchFlags = O_PATH;
#else
        static constexpr int SearchFlags = O_RDONLY;
#endif

        // The directory a path's last element is in, and that element
        // ("." for the root)
        struct Parent {
            int fd = -1;
            bool owned = false;
            std::string leaf;

            Parent() = default;
            Parent(const Parent&) = delete;
            Parent& operator=(const Parent&) = delete;

            ~Parent() {
                if (owned) {
                    ::close(fd);
                }
            }
        };

        int _parent(const std::string& rel, Parent& p) const {
            if (_root < 0) {
                return _root_error;
            }
            p.fd = _root;
            size_t at = 0;
            for (;;) {
                const size_t slash = rel.find('/', at);
                if (slash == std::string::npos) {
                    p.leaf = rel.empty() ? std::string(".") : rel.substr(at);
                    return 0;
                }
                const std::string dir = rel.substr(at, slash - at);
                int fd = ::openat(p.fd, dir.c_str(), SearchFlags | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
                if (fd < 0) {
                    return errno;   // ELOOP for a symlink come meanwhile, ENOTDIR for a file
                }
                if (p.owned) {
                    ::close(p.fd);
                }
                p.fd = fd;
                p.owned = true;
                at = slash + 1;
            }
        }

        SGCL_INLINE_HOT int _fd(int handle) const noexcept {
            auto it = _files.find(handle);
            return it == _files.end() ? -1 : it->second;
        }

        int _root = -1;
        int _root_error = 0;
        std::map<int, int> _files;
        std::vector<int> _append;
        int _next = 1;
    };

    // --- a tree in memory ---------------------------------------------------

    // A tree of files, directories and symlinks in memory, for the tests and
    // the fuzzer; bounded (a file at most 16 MB, 4096 nodes in all)
    class MemoryFs final : public Fs {
    public:
        struct Node {
            uint32_t mode = S_IFDIR | 0755;
            std::vector<uint8_t> data;
            std::string target;
            std::map<std::string, std::shared_ptr<Node>> children;
            uint32_t atime = 0;
            uint32_t mtime = 0;
        };

        static constexpr size_t MaxFile = 16 << 20;
        static constexpr size_t MaxNodes = 4096;

        MemoryFs()
        : _root(std::make_shared<Node>()) {
        }

        // A file of the tree made (the directories on the way too), for a
        // test's setup
        void put(const std::string& rel, std::string_view content) {
            std::vector<std::string> parts;
            split_into(rel, parts);
            auto n = _root;
            for (size_t i = 0; i + 1 < parts.size(); ++i) {
                auto& c = n->children[parts[i]];
                if (!c) {
                    c = std::make_shared<Node>();
                }
                n = c;
            }
            auto f = std::make_shared<Node>();
            f->mode = S_IFREG | 0644;
            f->data.assign(content.begin(), content.end());
            n->children[parts.back()] = f;
        }

        int lstat(const std::string& rel, Attrs& out) override {
            auto n = _find(rel);
            if (!n) {
                return _err;
            }
            out = _attrs(*n);
            return 0;
        }

        int readlink(const std::string& rel, std::string& out) override {
            auto n = _find(rel);
            if (!n) {
                return _err;
            }
            if ((n->mode & S_IFMT) != S_IFLNK) {
                return EINVAL;
            }
            out = n->target;
            return 0;
        }

        int open(const std::string& rel, uint32_t pflags, uint32_t mode, int& handle) override {
            std::shared_ptr<Node> parent;
            std::string name;
            auto n = _find(rel, &parent, &name);
            if (n && (pflags & FxfCreat) && (pflags & FxfExcl)) {
                return EEXIST;
            }
            if (!n) {
                if (!(pflags & FxfCreat) || !parent || name.empty()) {
                    return _err ? _err : ENOENT;
                }
                if (_nodes >= MaxNodes) {
                    return ENOSPC;
                }
                n = std::make_shared<Node>();
                n->mode = S_IFREG | (mode & 07777);
                parent->children[name] = n;
                ++_nodes;
            }
            if ((n->mode & S_IFMT) == S_IFDIR) {
                return EISDIR;
            }
            if ((n->mode & S_IFMT) != S_IFREG) {
                return ELOOP;   // O_NOFOLLOW on a symlink
            }
            if (pflags & FxfTrunc) {
                n->data.clear();
            }
            handle = _next++;
            _open[handle] = Open{n, (pflags & FxfAppend) != 0};
            return 0;
        }

        int read(int handle, uint64_t offset, uint8_t* buf, size_t n, size_t& got) override {
            auto it = _open.find(handle);
            if (it == _open.end()) {
                return EBADF;
            }
            const auto& d = it->second.node->data;
            got = offset >= d.size() ? 0 : std::min<size_t>(n, d.size() - size_t(offset));
            if (got) {
                sgcl::detail::copy_bytes(buf, d.data() + offset, got);
            }
            return 0;
        }

        int write(int handle, uint64_t offset, const uint8_t* data, size_t n) override {
            auto it = _open.find(handle);
            if (it == _open.end()) {
                return EBADF;
            }
            auto& d = it->second.node->data;
            if (it->second.append) {
                offset = d.size();
            }
            if (offset > MaxFile || n > MaxFile - offset) {
                return EFBIG;
            }
            if (d.size() < offset + n) {
                d.resize(size_t(offset + n));
            }
            if (n) {
                sgcl::detail::copy_bytes(d.data() + offset, data, n);
            }
            return 0;
        }

        int fstat(int handle, Attrs& out) override {
            auto it = _open.find(handle);
            if (it == _open.end()) {
                return EBADF;
            }
            out = _attrs(*it->second.node);
            return 0;
        }

        int fsetstat(int handle, const Attrs& a) override {
            auto it = _open.find(handle);
            if (it == _open.end()) {
                return EBADF;
            }
            return _set(*it->second.node, a);
        }

        int fsync(int handle) override {
            return _open.count(handle) ? 0 : EBADF;
        }

        void close(int handle) override {
            _open.erase(handle);
        }

        int list(const std::string& rel, std::vector<DirEntry>& out) override {
            auto n = _find(rel);
            if (!n) {
                return _err;
            }
            if ((n->mode & S_IFMT) != S_IFDIR) {
                return ENOTDIR;
            }
            for (auto& [name, c] : n->children) {
                out.push_back(DirEntry{name, _attrs(*c)});
            }
            return 0;
        }

        int setstat(const std::string& rel, const Attrs& a, bool) override {
            auto n = _find(rel);
            if (!n) {
                return _err;
            }
            return _set(*n, a);
        }

        int remove(const std::string& rel) override {
            std::shared_ptr<Node> parent;
            std::string name;
            auto n = _find(rel, &parent, &name);
            if (!n) {
                return _err;
            }
            if (!parent) {
                return EPERM;
            }
            if ((n->mode & S_IFMT) == S_IFDIR) {
                return EISDIR;
            }
            parent->children.erase(name);
            --_nodes;
            return 0;
        }

        int mkdir(const std::string& rel, uint32_t mode) override {
            std::shared_ptr<Node> parent;
            std::string name;
            if (_find(rel, &parent, &name)) {
                return EEXIST;
            }
            if (!parent || name.empty()) {
                return _err ? _err : ENOENT;
            }
            if (_nodes >= MaxNodes) {
                return ENOSPC;
            }
            auto d = std::make_shared<Node>();
            d->mode = S_IFDIR | (mode & 07777);
            parent->children[name] = d;
            ++_nodes;
            return 0;
        }

        int rmdir(const std::string& rel) override {
            std::shared_ptr<Node> parent;
            std::string name;
            auto n = _find(rel, &parent, &name);
            if (!n) {
                return _err;
            }
            if (!parent) {
                return EPERM;
            }
            if ((n->mode & S_IFMT) != S_IFDIR) {
                return ENOTDIR;
            }
            if (!n->children.empty()) {
                return ENOTEMPTY;
            }
            parent->children.erase(name);
            --_nodes;
            return 0;
        }

        int rename(const std::string& from, const std::string& to, bool replace) override {
            std::shared_ptr<Node> fp, tp;
            std::string fname, tname;
            auto n = _find(from, &fp, &fname);
            if (!n) {
                return _err;
            }
            auto existing = _find(to, &tp, &tname);
            if (!fp || !tp || tname.empty()) {
                return fp && tp ? ENOENT : EPERM;
            }
            if (existing && !replace) {
                return EEXIST;
            }
            if (existing == n) {
                return 0;
            }
            // a directory into itself: refused, as rename(2) refuses it
            if ((n->mode & S_IFMT) == S_IFDIR && (to + "/").rfind(from + "/", 0) == 0) {
                return EINVAL;
            }
            if (existing) {
                --_nodes;
            }
            tp->children[tname] = n;
            fp->children.erase(fname);
            return 0;
        }

        int symlink(const std::string& target, const std::string& link) override {
            std::shared_ptr<Node> parent;
            std::string name;
            if (_find(link, &parent, &name)) {
                return EEXIST;
            }
            if (!parent || name.empty()) {
                return _err ? _err : ENOENT;
            }
            if (_nodes >= MaxNodes) {
                return ENOSPC;
            }
            auto s = std::make_shared<Node>();
            s->mode = S_IFLNK | 0777;
            s->target = target;
            parent->children[name] = s;
            ++_nodes;
            return 0;
        }

        int link(const std::string& from, const std::string& to) override {
            auto n = _find(from);
            if (!n) {
                return _err;
            }
            if ((n->mode & S_IFMT) == S_IFDIR || (n->mode & S_IFMT) == S_IFLNK) {
                return EPERM;
            }
            std::shared_ptr<Node> parent;
            std::string name;
            if (_find(to, &parent, &name)) {
                return EEXIST;
            }
            if (!parent || name.empty()) {
                return _err ? _err : ENOENT;
            }
            parent->children[name] = n;
            ++_nodes;
            return 0;
        }

        int statvfs(const std::string& rel, file_system_info& out) override {
            if (!_find(rel)) {
                return _err;
            }
            out = file_system_info{};
            out.block_size = 4096;
            out.files = MaxNodes;
            out.files_free = MaxNodes - _nodes;
            out.max_name_length = 255;
            return 0;
        }

    private:
        struct Open {
            std::shared_ptr<Node> node;
            bool append = false;
        };

        // The node of a path, never following a symlink (the resolver has);
        // its parent and its last component's name when asked. Null with
        // _err set when it is not there
        std::shared_ptr<Node> _find(const std::string& rel, std::shared_ptr<Node>* parent = nullptr, std::string* name = nullptr) {
            std::vector<std::string> parts;
            split_into(rel, parts);
            auto n = _root;
            _err = 0;
            if (parent) {
                *parent = nullptr;
            }
            for (size_t i = 0; i < parts.size(); ++i) {
                if ((n->mode & S_IFMT) != S_IFDIR) {
                    _err = ENOTDIR;
                    return nullptr;
                }
                if (i + 1 == parts.size()) {
                    if (parent) {
                        *parent = n;
                    }
                    if (name) {
                        *name = parts[i];
                    }
                }
                auto it = n->children.find(parts[i]);
                if (it == n->children.end()) {
                    _err = ENOENT;
                    if (i + 1 != parts.size() && parent) {
                        *parent = nullptr;
                    }
                    return nullptr;
                }
                n = it->second;
            }
            return n;
        }

        static Attrs _attrs(const Node& n) {
            Attrs a;
            a.flags = AttrSize | AttrUidGid | AttrPermissions | AttrAcModTime;
            a.size = (n.mode & S_IFMT) == S_IFLNK ? n.target.size() : n.data.size();
            a.permissions = n.mode;
            a.atime = n.atime;
            a.mtime = n.mtime;
            return a;
        }

        static int _set(Node& n, const Attrs& a) {
            if (a.flags & AttrSize) {
                if ((n.mode & S_IFMT) != S_IFREG) {
                    return EINVAL;
                }
                if (a.size > MaxFile) {
                    return EFBIG;
                }
                n.data.resize(size_t(a.size));
            }
            if (a.flags & AttrPermissions) {
                n.mode = (n.mode & S_IFMT) | (a.permissions & 07777);
            }
            if (a.flags & AttrAcModTime) {
                n.atime = a.atime;
                n.mtime = a.mtime;
            }
            return 0;
        }

        std::shared_ptr<Node> _root;
        std::map<int, Open> _open;
        int _next = 1;
        int _err = 0;
        size_t _nodes = 1;
    };
}
