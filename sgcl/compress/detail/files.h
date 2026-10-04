//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../io/detail/path.h"
#include "../error.h"
#include "../../io/fs.h"
#include "../../io/file.h"
#include "../../io/functions.h"
#include "../../time/datetime.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <string_view>
#include <vector>

// What the archives' one-line forms share (tar, zip and sevenzip's
// extract and create, DESIGN 285/312): the tree of a directory as the
// entries of an archive, and the writing of entries under a directory with
// the links made last. Plain memory: std strings of paths and names, no
// tracked word, on the stack of the call.
namespace sgcl::compress::detail {
    // One thing under a directory, as an archive holds it: its name from
    // the directory, '/' between the parts, without a trailing '/'
    struct TreeEntry {
        std::string name;
        std::string path;     // on disk
        io::file_type type = io::file_type::unknown;
        io::permissions mode = io::permissions::none;
        io::file_time modified;
        uint64_t size = 0;
        std::string target;   // a symlink's
    };

    // An io error of a create's step as the archive's: a code of this
    // module carried by a writer (an entry it refuses) back as itself, a
    // failure of a file as errc::io; neither has a place in any data
    inline error archive_error(const io::error& e) noexcept {
        if (&e.code().category() == &compress_category()) {
            return no_place(errc(e.code().value()), string(e.message()));
        }
        return no_place(e);
    }

    SGCL_INLINE_HOT time::datetime datetime_of(io::file_time t) noexcept {
        return time::datetime::from_unix_nano(int64_t(t.time_since_epoch().count()), time::zone::utc());
    }

    // The file a writer's add_file adds, open, with what its entry takes
    // from it: the mode, the time, the size, read from the file opened
    struct SourceFile {
        io::file file;
        io::file_info info;
    };

    // The file at path opened for an entry named `name` of `format`
    // ("zip", "tar", "7z"), links followed; nothing of the archive is
    // touched yet, so none of these errors is the archive's: a name that
    // ends in '/' (a directory's), a path that is not a regular file
    // (checked before the open, which on a FIFO would wait for a writer,
    // and again on the file opened), and the errors of stat and open
    inline expected<SourceFile, error> open_source(const string& path, const string& name, const char* format) noexcept {
        if (!name.empty() && name.view().back() == '/') {
            return unexpected(no_place(errc::invalid_argument, string(std::string(format) + ": a file's entry named as a directory: " + std::string(name.view()))));
        }
        auto not_regular = [&] {
            return unexpected(no_place(errc::invalid_argument, string(std::string(format) + ": not a regular file: " + std::string(path.view()))));
        };
        auto st = io::stat(path);
        if (!st) {
            return unexpected(no_place(st.error()));
        }
        if (!st->is_regular()) {
            return not_regular();
        }
        auto f = io::open(path);
        if (!f) {
            return unexpected(no_place(f.error()));
        }
        auto info = f->stat();
        if (!info || !info->is_regular()) {
            (void)f->close();
            if (!info) {
                return unexpected(no_place(info.error()));
            }
            return not_regular();
        }
        return SourceFile{*f, *info};
    }

    // The tree under dir in lexical order, a directory before what it
    // holds: directories, regular files and symbolic links (their targets
    // read); what is none of them (a socket, a device, a fifo) left out,
    // as tar -c of GNU warns and zip leaves them. Links are not followed
    inline expected<std::vector<TreeEntry>, error> list_tree(const string& dir) noexcept {
        std::vector<TreeEntry> out;
        std::string root(dir.view());
        while (root.size() > 1 && root.back() == '/') {
            root.pop_back();
        }
        optional<error> failed;
        auto walked = io::walk_dir(string(root), [&](const io::directory_entry& d, const optional<io::error>& e) noexcept {
            if (e) {
                failed = no_place(*e);
                return io::walk_action::stop;
            }
            if (d.type != io::file_type::directory && d.type != io::file_type::regular && d.type != io::file_type::symlink) {
                return io::walk_action::next;
            }
            auto info = d.info();
            if (!info) {
                failed = no_place(info.error());
                return io::walk_action::stop;
            }
            TreeEntry t;
            t.path = std::string(d.path.view());
            t.name = t.path.substr(root.size() + 1);
            t.type = d.type;
            t.mode = info->mode;
            t.modified = info->modified;
            t.size = d.type == io::file_type::regular ? info->size : 0;
            if (d.type == io::file_type::symlink) {
                auto target = io::read_link(d.path);
                if (!target) {
                    failed = no_place(target.error());
                    return io::walk_action::stop;
                }
                t.target = std::string(target->view());
            }
            out.push_back(std::move(t));
            return io::walk_action::next;
        });
        if (failed) {
            return unexpected(*failed);
        }
        if (!walked) {
            return unexpected(no_place(walked.error()));
        }
        return out;
    }

    // The writing of entries under a directory: the directory made, each
    // path the directory's joined with the entry's name, the links kept
    // for the end (no file is written through a link the archive made;
    // a hard link's target is written by then)
    class TreeWriter {
    public:
        SGCL_INLINE_HOT expected<void, error> start(const string& directory) noexcept {
            _root = std::string(directory.view());
            if (!_root.empty() && _root.back() != '/') {
                _root += '/';
            }
            if (auto made = io::mkdir_all(string(_root)); !made) {
                return unexpected(no_place(made.error()));
            }
            return {};
        }

        std::string path_of(std::string_view name) const noexcept {
            while (!name.empty() && name.back() == '/') {
                name.remove_suffix(1);
            }
            return _root + std::string(name);
        }

        SGCL_INLINE_HOT expected<void, error> directory(std::string_view name, io::permissions mode) noexcept {
            unsigned m = unsigned(mode) & 07777;
            if (auto made = io::mkdir_all(string(path_of(name)), io::permissions(m ? m | 0700 : 0755)); !made) {
                return unexpected(no_place(made.error()));
            }
            return {};
        }

        // A file's data from the reader into a file made at the name;
        // potentially throwing, as the copy's read and the file's write are.
        // A failure of the reading is the reader's kept error when it keeps
        // one (where it was found), else a code of this module the reader
        // gave, at the entry's offset; a failure of the file has no place
        template<class R>
        expected<void, error> file(std::string_view name, io::permissions mode, optional<time::datetime> modified, R& from, uint64_t offset) {
            const std::string path = path_of(name);
            if (auto made = io::mkdir_all(string(path.substr(0, path.rfind('/')))); !made) {
                return unexpected(no_place(made.error()));
            }
            unsigned m = unsigned(mode) & 07777;
            auto f = io::create(string(path), io::permissions(m ? m : 0644));
            if (!f) {
                return unexpected(no_place(f.error()));
            }
            auto copied = io::copy(*f, from);
            if (!copied) {
                (void)f->close();
                if constexpr (requires { from.last_error(); }) {
                    if (from.last_error()) {
                        return unexpected(*from.last_error());
                    }
                }
                const io::error& e = copied.error();
                if (&e.code().category() == &compress_category()) {
                    return unexpected(error(errc(e.code().value()), offset, string(e.message())));
                }
                return unexpected(no_place(e));
            }
            if (auto closed = f->close(); !closed) {
                return unexpected(no_place(closed.error()));
            }
            if (modified) {
                (void)io::set_modified(string(path), io::file_time(modified->to_sys()));
            }
            return {};
        }

        SGCL_INLINE_HOT void symlink(std::string_view name, std::string_view target) noexcept {
            _links.push_back(Link{path_of(name), std::string(target), false});
        }

        // A hard link: the name of the archive's entry it is the same file as
        SGCL_INLINE_HOT void hardlink(std::string_view name, std::string_view same_as) noexcept {
            _links.push_back(Link{path_of(name), path_of(same_as), true});
        }

        // The links, last
        expected<void, error> finish() noexcept {
            for (const auto& l : _links) {
                if (auto made = io::mkdir_all(string(l.path.substr(0, l.path.rfind('/')))); !made) {
                    return unexpected(no_place(made.error()));
                }
                (void)io::remove(string(l.path));   // a file there is replaced, as a file is
                if (l.hard) {
                    std::error_code ec;
                    std::filesystem::create_hard_link(l.target, l.path, ec);
                    if (ec) {
                        return unexpected(no_place(io::error(ec, "link", string(l.path))));
                    }
                    continue;
                }
                if (auto made = io::symlink(string(l.target), string(l.path)); !made) {
                    return unexpected(no_place(made.error()));
                }
            }
            return {};
        }

    private:
        struct Link {
            std::string path;
            std::string target;
            bool hard;
        };

        std::string _root;
        std::vector<Link> _links;
    };
}
