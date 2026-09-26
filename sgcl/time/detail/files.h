//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

// What the zones read of the file system: a small file whole, the target
// of a link, the names under a directory. On the thread that asks and at
// once — a zone is read once for the program and is some kilobytes — with
// nothing of io's streams, tasks or reactor in the way, so that the time
// of day does not depend on how io waits.
namespace sgcl::time::detail {
    struct file_bytes {
        std::string bytes;
        int error = 0;          // errno of what failed, 0 when nothing did
    };

    // The whole file, when it is a regular file of at most `cap` bytes: a
    // TZ variable may name any path, /dev/zero or a directory included,
    // and reading one of those must end
    inline file_bytes read_small_file(const std::string& path, size_t cap = size_t(1) << 20) {
        file_bytes out;
        int fd;
        do {
            fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
        } while (fd < 0 && errno == EINTR);
        if (fd < 0) {
            out.error = errno;
            return out;
        }
        char buffer[16384];
        for (;;) {
            ssize_t n = ::read(fd, buffer, sizeof buffer);
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n < 0) {
                out.error = errno;
                break;
            }
            if (n == 0) {
                break;
            }
            if (out.bytes.size() + size_t(n) > cap) {
                out.error = EFBIG;
                break;
            }
            out.bytes.append(buffer, size_t(n));
        }
        ::close(fd);
        return out;
    }

    // Whether a file starts with these bytes, reading no more of it
    inline bool starts_with(const std::string& path, std::string_view head) {
        int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
        if (fd < 0) {
            return false;
        }
        char buffer[16];
        size_t got = 0;
        while (got < head.size() && got < sizeof buffer) {
            ssize_t n = ::read(fd, buffer + got, head.size() - got);
            if (n < 0 && errno == EINTR) {
                continue;
            }
            if (n <= 0) {
                break;
            }
            got += size_t(n);
        }
        ::close(fd);
        return got == head.size() && std::string_view(buffer, got) == head;
    }

    // The error of a read as a sentence: "No such file or directory"
    inline std::string error_text(int error) {
        return std::system_category().message(error);
    }

    // What a symbolic link points at, nothing when the path is not one
    inline optional<std::string> link_target(const std::string& path) {
        std::error_code ec;
        auto target = std::filesystem::read_symlink(path, ec);
        if (ec) {
            return nullopt;
        }
        return target.string();
    }

    // The files under a directory, as paths relative to it, depth first,
    // the directories named in `skip` at the top left out; a directory
    // that is a link is not followed
    inline std::vector<std::string> files_under(const std::string& root, const std::vector<std::string>& skip) {
        std::vector<std::string> out;
        std::error_code ec;
        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; !ec && it != end; it.increment(ec)) {
            std::error_code tec;
            auto status = it->symlink_status(tec);
            std::string relative = std::filesystem::relative(it->path(), root, tec).generic_string();
            if (std::filesystem::is_directory(status)) {
                if (it.depth() == 0 && std::find(skip.begin(), skip.end(), relative) != skip.end()) {
                    it.disable_recursion_pending();
                }
                continue;
            }
            if (!std::filesystem::is_symlink(status) || !std::filesystem::is_directory(it->status(tec))) {
                out.push_back(relative);
            }
        }
        return out;
    }
}
