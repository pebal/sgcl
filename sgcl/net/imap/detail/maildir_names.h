//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "syntax.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>

// The texts of a Maildir as maildir_backend reads them: a message file's
// name (the unique base, ":2," and the flags' letters; Courier's ",S=" and
// Dovecot's ",W=" sizes in the base), a folder's directory name (Maildir++
// ".a.b", each level in modified UTF-7 with "." written "&AC4-"), the
// sgcl-uidlist and sgcl-keywords files, a user's name as a directory.
// Whatever cannot be read is refused or passed over, never trusted.
namespace sgcl::net::imap::detail {
    struct MdMessage;

    // A file name of cur/: the base and the flags' letters (the standard
    // "2," info; names without one, or of the experimental "1," info,
    // have no flags); letters sorted, repeats and characters outside A-Z
    // and a-z dropped
    inline void split_file_name(std::string_view name, std::string& base, std::string& letters) {
        base.clear();
        letters.clear();
        if (name.empty() || name.size() > 1024 || name.find('/') != std::string_view::npos) {
            return;
        }
        const size_t colon = name.rfind(':');
        if (colon == std::string_view::npos) {
            base = std::string(name);
            return;
        }
        base = std::string(name.substr(0, colon));
        std::string_view info = name.substr(colon + 1);
        if (info.size() >= 2 && info[0] == '2' && info[1] == ',') {
            for (char c : info.substr(2)) {
                if (((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) && letters.find(c) == std::string::npos) {
                    letters += c;
                }
            }
            std::sort(letters.begin(), letters.end());
        }
    }

    // The size a base names: ",W=" (the size with CRLF line ends, what
    // IMAP counts), else ",S=", else the file's
    inline uint64_t size_in_name(std::string_view base, uint64_t file_size) noexcept {
        auto field = [&](std::string_view key) -> int64_t {
            const size_t at = base.find(key);
            if (at == std::string_view::npos) {
                return -1;
            }
            uint64_t v = 0;
            size_t i = at + key.size();
            const size_t start = i;
            while (i < base.size() && base[i] >= '0' && base[i] <= '9' && i - start < 18) {
                v = v * 10 + uint64_t(base[i] - '0');
                ++i;
            }
            return i > start ? int64_t(v) : -1;
        };
        const int64_t w = field(",W=");
        if (w >= 0) {
            return uint64_t(w);
        }
        const int64_t s = field(",S=");
        return s >= 0 ? uint64_t(s) : file_size;
    }

    // A user's name as a directory: no "/", not "." or "..", no leading
    // ".", no NUL or control characters
    inline bool valid_user_name(std::string_view u) noexcept {
        if (u.empty() || u.size() > 255 || u[0] == '.') {
            return false;
        }
        for (unsigned char c : u) {
            if (c < 0x20 || c == '/' || c == 0x7f || c == '\\') {
                return false;
            }
        }
        return valid_utf8(u);
    }

    // A mailbox name ("a/b") as its directory's name without the leading
    // dot ("a.b"): each level in modified UTF-7, "." inside written
    // "&AC4-"
    inline std::string folder_dir_name(std::string_view name) {
        std::string out;
        size_t i = 0;
        while (i <= name.size()) {
            size_t e = name.find('/', i);
            if (e == std::string_view::npos) {
                e = name.size();
            }
            std::string level = utf7_encode(name.substr(i, e - i));
            std::string escaped;
            for (char c : level) {
                if (c == '.') {
                    escaped += "&AC4-";
                } else {
                    escaped += c;
                }
            }
            if (!out.empty() || i > 0) {
                out += '.';
            }
            out += escaped;
            i = e + 1;
        }
        return out;
    }

    // A directory's name (without the leading dot) as a mailbox name;
    // false for one that is not of a folder
    inline bool folder_name_of(std::string_view dir, std::string& name) {
        name.clear();
        if (dir.empty() || dir.size() > 1024 || dir.front() == '.' || dir.back() == '.') {
            return false;
        }
        size_t i = 0;
        while (i <= dir.size()) {
            size_t e = dir.find('.', i);
            if (e == std::string_view::npos) {
                e = dir.size();
            }
            if (e == i) {
                return false;   // an empty level
            }
            std::string level;
            if (!utf7_decode(dir.substr(i, e - i), level, true)) {
                return false;
            }
            if (level.find('/') != std::string::npos || level.empty()) {
                return false;
            }
            if (!name.empty() || i > 0) {
                name += '/';
            }
            name += level;
            i = e + 1;
        }
        return !iequal(name, "INBOX");
    }

    // sgcl-uidlist: "SGCL1 validity next modseq", then a line per message
    // "uid modseq letters base" ("-" for no letters); false for a file of
    // another shape (UIDs not ascending, a UID at or past next, a line cut)
    template<class M>
    inline bool parse_uidlist(std::string_view text, uint32_t& validity, uint32_t& next, uint64_t& modseq, std::vector<M>& out) {
        out.clear();
        size_t i = 0;
        auto line = [&](std::string_view& l) {
            if (i >= text.size()) {
                return false;
            }
            size_t e = text.find('\n', i);
            if (e == std::string_view::npos) {
                return false;   // a list always ends with its newline
            }
            l = text.substr(i, e - i);
            i = e + 1;
            return true;
        };
        std::string_view head;
        if (!line(head)) {
            return false;
        }
        Lexer x(head);
        uint64_t v, n, m;
        if (!x.word("SGCL1") || !x.sp() || !x.number(v, UINT32_MAX) || !x.sp() || !x.number(n, UINT32_MAX) || !x.sp() || !x.number(m) || !x.at_end() || v == 0 || n == 0) {
            return false;
        }
        validity = uint32_t(v);
        next = uint32_t(n);
        modseq = m;
        uint32_t last = 0;
        std::string_view l;
        while (line(l)) {
            Lexer y(l);
            uint32_t uid;
            uint64_t ms;
            if (!y.nz_number(uid) || !y.sp() || !y.number(ms) || !y.sp()) {
                return false;
            }
            std::string_view letters = y.run([](unsigned char c) { return c != ' '; });
            if (letters.empty() || !y.sp()) {
                return false;
            }
            std::string_view base = y.rest();
            if (base.empty() || base.size() > 1024 || base.find('/') != std::string_view::npos || base.find(':') != std::string_view::npos) {
                return false;
            }
            if (uid <= last || uid >= next || ms > modseq) {
                return false;
            }
            last = uid;
            M msg;
            msg.uid = uid;
            msg.modseq = ms;
            msg.base = std::string(base);
            if (letters != "-") {
                for (char c : letters) {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) {
                        return false;
                    }
                }
                msg.letters = std::string(letters);
            }
            out.push_back(std::move(msg));
            if (out.size() > 50000000) {
                return false;
            }
        }
        return i == text.size();
    }

    // sgcl-keywords: "index keyword" lines, the indexes 0 to 25 in order
    inline void parse_keywords(std::string_view text, std::vector<std::string>& out) {
        out.clear();
        size_t i = 0;
        while (i < text.size() && out.size() < 26) {
            size_t e = text.find('\n', i);
            if (e == std::string_view::npos) {
                e = text.size();
            }
            std::string_view l = text.substr(i, e - i);
            i = e + 1;
            Lexer x(l);
            uint64_t idx;
            if (!x.number(idx, 25) || idx != out.size() || !x.sp()) {
                break;
            }
            std::string_view k = x.atom();
            if (k.empty() || !x.at_end()) {
                break;
            }
            out.emplace_back(k);
        }
    }

    inline int64_t stat_mtime_ns(const char* path, int64_t* size = nullptr) noexcept {
        struct stat st;
        if (::stat(path, &st) != 0) {
            return -1;
        }
        if (size) {
            *size = int64_t(st.st_size);
        }
#if defined(__APPLE__)
        return int64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
        return int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
    }
}
