//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "path.h"
#include "../async/blocking.h"
#include "../async/coroutine.h"
#include "../containers/vector.h"
#include "../core/aliases.h"
#include "../core/string.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

namespace sgcl::io {
    // The file system (os, io/fs): what is at a path, and making,
    // moving and removing things there. Paths are strings; the
    // operations are std::filesystem's and the platform's under a
    // thin layer that returns result<T> and speaks in this module's
    // types. A function that takes a path follows symlinks, its l-
    // variant does not, as on POSIX.

    // The mode bits of a file (rwxrwxrwx and the set-id and sticky
    // bits), as an octal in the code: permissions(0644)
    enum class permissions : unsigned {
        none = 0,
        owner_read = 0400, owner_write = 0200, owner_exec = 0100,
        group_read = 040, group_write = 020, group_exec = 010,
        others_read = 04, others_write = 02, others_exec = 01,
        set_uid = 04000, set_gid = 02000, sticky = 01000,
        all = 0777
    };

    constexpr permissions operator|(permissions a, permissions b) noexcept {
        return static_cast<permissions>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    constexpr permissions operator&(permissions a, permissions b) noexcept {
        return static_cast<permissions>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
    }

    enum class file_type { unknown, regular, directory, symlink, block, character, fifo, socket };

    using file_time = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;

    // What stat says about a path: its name (the last element of the
    // path), size, type, permissions and modification time
    struct file_info {
        string name;
        uint64_t size = 0;
        file_type type = file_type::unknown;
        permissions mode = permissions::none;
        file_time modified;

        bool is_regular() const noexcept { return type == file_type::regular; }
        bool is_directory() const noexcept { return type == file_type::directory; }
        bool is_symlink() const noexcept { return type == file_type::symlink; }
    };

    result<file_info> stat(const string& path);
    result<file_info> lstat(const string& path);

    // An entry of a directory listing: the name and the type come from
    // the listing itself, the rest (info()) from a stat when asked
    struct dir_entry {
        string name;
        string path;   // dir joined with name
        file_type type = file_type::unknown;

        bool is_directory() const noexcept { return type == file_type::directory; }

        result<file_info> info() const {
            return lstat(path);
        }
    };

    namespace detail {
        namespace fs = std::filesystem;

        inline file_type type_of(mode_t m) noexcept {
            switch (m & S_IFMT) {
                case S_IFREG: return file_type::regular;
                case S_IFDIR: return file_type::directory;
                case S_IFLNK: return file_type::symlink;
                case S_IFBLK: return file_type::block;
                case S_IFCHR: return file_type::character;
                case S_IFIFO: return file_type::fifo;
                case S_IFSOCK: return file_type::socket;
            }
            return file_type::unknown;
        }

        inline file_type type_of(fs::file_type t) noexcept {
            switch (t) {
                case fs::file_type::regular: return file_type::regular;
                case fs::file_type::directory: return file_type::directory;
                case fs::file_type::symlink: return file_type::symlink;
                case fs::file_type::block: return file_type::block;
                case fs::file_type::character: return file_type::character;
                case fs::file_type::fifo: return file_type::fifo;
                case fs::file_type::socket: return file_type::socket;
                default: return file_type::unknown;
            }
        }

        inline file_info info_of(const struct ::stat& st, const string& path) {
            file_info i;
            i.name = io::path::base(path);
            i.size = static_cast<uint64_t>(st.st_size);
            i.type = type_of(st.st_mode);
            i.mode = static_cast<permissions>(st.st_mode & 07777);
#if defined(__APPLE__)
            auto& ts = st.st_mtimespec;
#else
            auto& ts = st.st_mtim;
#endif
            i.modified = file_time(std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec));
            return i;
        }

        inline error fs_error(error_code ec, const char* op, const string& path) {
            return error(ec, op, path);
        }

        inline fs::path fs_path(const string& p) {
            return fs::path(p.str());
        }
    }

    inline result<file_info> stat(const string& path) {
        struct ::stat st;
        if (::stat(path.c_str(), &st) != 0) {
            return detail::fail(last_error("stat", path));
        }
        return detail::info_of(st, path);
    }

    inline result<file_info> lstat(const string& path) {
        struct ::stat st;
        if (::lstat(path.c_str(), &st) != 0) {
            return detail::fail(last_error("lstat", path));
        }
        return detail::info_of(st, path);
    }

    // Whether something is at the path; whether it is a directory or a
    // regular file (false when nothing is there or on any error: the
    // question is the shortcut, stat() is the answer with the error)
    inline bool exists(const string& path) noexcept {
        struct ::stat st;
        return ::stat(path.c_str(), &st) == 0;
    }

    inline bool is_directory(const string& path) noexcept {
        struct ::stat st;
        return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    inline bool is_regular(const string& path) noexcept {
        struct ::stat st;
        return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
    }

    // One directory (an error when the parent is missing or it exists);
    // the whole chain, existing ones left alone (mkdir -p)
    inline result<void> mkdir(const string& path, permissions p = permissions(0777)) {
        if (::mkdir(path.c_str(), static_cast<mode_t>(p)) != 0) {
            return detail::fail(last_error("mkdir", path));
        }
        return {};
    }

    inline result<void> mkdir_all(const string& path, permissions p = permissions(0777)) {
        string clean = io::path::clean(path);
        size_t pos = 0;
        while (pos < clean.size()) {
            size_t next = clean.find('/', pos + 1);
            string prefix = next == string::npos ? clean : clean.substr(0, next);
            if (::mkdir(prefix.c_str(), static_cast<mode_t>(p)) != 0 && errno != EEXIST) {
                return detail::fail(last_error("mkdir", prefix));
            }
            if (next == string::npos) {
                break;
            }
            pos = next;
        }
        if (!is_directory(clean)) {
            return detail::fail(error(std::make_error_code(std::errc::not_a_directory), "mkdir", path));
        }
        return {};
    }

    // A file, a symlink or an empty directory; everything under the path
    // and the path itself, nothing there being no error for remove_all
    inline result<void> remove(const string& path) {
        std::error_code ec;
        bool removed = detail::fs::remove(detail::fs_path(path), ec);
        if (ec) {
            return detail::fail(detail::fs_error(ec, "remove", path));
        }
        if (!removed) {
            return detail::fail(error(std::make_error_code(std::errc::no_such_file_or_directory), "remove", path));
        }
        return {};
    }

    inline result<void> remove_all(const string& path) {
        std::error_code ec;
        detail::fs::remove_all(detail::fs_path(path), ec);
        if (ec) {
            return detail::fail(detail::fs_error(ec, "remove_all", path));
        }
        return {};
    }

    // Moves, replacing what is at the new path (rename(2)); copies the
    // bytes and the permissions of a regular file, replacing the target
    inline result<void> rename(const string& from, const string& to) {
        if (::rename(from.c_str(), to.c_str()) != 0) {
            return detail::fail(last_error("rename", from));
        }
        return {};
    }

    inline result<void> copy_file(const string& from, const string& to) {
        std::error_code ec;
        detail::fs::copy_file(detail::fs_path(from), detail::fs_path(to), detail::fs::copy_options::overwrite_existing, ec);
        if (ec) {
            return detail::fail(detail::fs_error(ec, "copy_file", from));
        }
        return {};
    }

    inline result<void> symlink(const string& target, const string& link) {
        if (::symlink(target.c_str(), link.c_str()) != 0) {
            return detail::fail(last_error("symlink", link));
        }
        return {};
    }

    inline result<string> read_link(const string& link) {
        std::error_code ec;
        auto target = detail::fs::read_symlink(detail::fs_path(link), ec);
        if (ec) {
            return detail::fail(detail::fs_error(ec, "read_link", link));
        }
        return string(target.native());
    }

    inline result<void> chmod(const string& path, permissions p) {
        if (::chmod(path.c_str(), static_cast<mode_t>(p)) != 0) {
            return detail::fail(last_error("chmod", path));
        }
        return {};
    }

    inline result<void> set_modified(const string& path, file_time t) {
        auto ns = t.time_since_epoch();
        struct timespec times[2];
        times[0].tv_sec = 0;
        times[0].tv_nsec = UTIME_OMIT;
        times[1].tv_sec = static_cast<time_t>(std::chrono::duration_cast<std::chrono::seconds>(ns).count());
        times[1].tv_nsec = static_cast<long>((ns % std::chrono::seconds(1)).count());
        if (::utimensat(AT_FDCWD, path.c_str(), times, 0) != 0) {
            return detail::fail(last_error("set_modified", path));
        }
        return {};
    }

    // The entries of a directory, sorted by name, "." and ".." left out
    inline result<vector<dir_entry>> read_dir(const string& path) {
        std::error_code ec;
        detail::fs::directory_iterator it(detail::fs_path(path), ec);
        if (ec) {
            return detail::fail(detail::fs_error(ec, "read_dir", path));
        }
        vector<dir_entry> entries;
        for (; it != detail::fs::directory_iterator(); it.increment(ec)) {
            if (ec) {
                return detail::fail(detail::fs_error(ec, "read_dir", path));
            }
            dir_entry e;
            e.name = string(it->path().filename().native());
            e.path = io::path::join(path, e.name);
            std::error_code tec;
            auto st = it->symlink_status(tec);
            e.type = tec ? file_type::unknown : detail::type_of(st.type());
            entries.push_back(std::move(e));
        }
        std::sort(entries.begin(), entries.end(), [](const dir_entry& a, const dir_entry& b) { return a.name < b.name; });
        return entries;
    }

    inline task<result<vector<dir_entry>>> async_read_dir(const string& path) {
        co_return co_await spawn_blocking([path] { return read_dir(path); });
    }

    // What the function given to walk_dir returns for an entry: go on,
    // do not enter this directory, stop the walk
    enum class walk_action { next, skip_dir, stop };

    namespace detail {
        template<class F>
        walk_action walk(const string& dir, F& f) {
            auto entries = read_dir(dir);
            if (!entries) {
                return f(dir_entry{io::path::base(dir), dir, file_type::directory}, optional<error>(entries.error())) == walk_action::stop ? walk_action::stop : walk_action::next;
            }
            for (auto& e : *entries) {
                auto a = f(e, optional<error>());
                if (a == walk_action::stop) {
                    return a;
                }
                if (e.is_directory() && a != walk_action::skip_dir) {
                    if (walk(e.path, f) == walk_action::stop) {
                        return walk_action::stop;
                    }
                }
            }
            return walk_action::next;
        }
    }

    // Every entry under root, in lexical order, the directory before its
    // contents, f called with each: walk_action(const dir_entry&, const
    // optional<error>&). A directory that cannot be read is reported
    // once, as the error with its entry, and the walk goes on; root
    // itself is not reported. Symlinks are not followed.
    template<class F>
    result<void> walk_dir(const string& root, F f) {
        auto info = lstat(root);
        if (!info) {
            return detail::fail(info);
        }
        if (!info->is_directory()) {
            return detail::fail(error(std::make_error_code(std::errc::not_a_directory), "walk_dir", root));
        }
        detail::walk(root, f);
        return {};
    }
}
