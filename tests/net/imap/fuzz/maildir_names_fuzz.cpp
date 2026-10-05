//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The texts maildir_backend reads from disk and from clients, on any bytes
// (detail/maildir_names.h, detail/syntax.h): a message file's name, a
// folder's directory name, sgcl-uidlist, sgcl-keywords, modified UTF-7.
// The first byte picks the text. What must hold:
//   - a file name's letters are sorted, without repeats, letters only; its
//     base holds no "/";
//   - a folder's directory name read as a mailbox name, written again and
//     read again, is the same name, and a mailbox name that is valid
//     writes as a directory name that reads back as itself (one short
//     enough for a file system: maildir_backend refuses the others);
//   - a uidlist read has its UIDs ascending below the next UID, each
//     mod-sequence at most the highest, bases without "/" or ":";
//   - modified UTF-7 decoded is UTF-8 and encodes back to the same text.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/imap/fuzz/maildir_names_fuzz.cpp)
// or replayed by the library's own driver.
#include "sgcl/net/imap/maildir.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;
    namespace d = sgcl::net::imap::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct M {
        uint32_t uid = 0;
        uint64_t modseq = 0;
        std::string base, letters;
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (data[0] % 5) {
        case 0: {
            std::string base, letters;
            d::split_file_name(in, base, letters);
            check(base.find('/') == std::string::npos);
            for (size_t i = 0; i < letters.size(); ++i) {
                const char c = letters[i];
                check((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
                check(i == 0 || letters[i - 1] < c);
            }
            (void)d::size_in_name(in, 7);
            break;
        }
        case 1: {
            std::string name;
            if (d::folder_name_of(in, name)) {
                check(d::valid_utf8(name));
                check(!name.empty() && name.front() != '/' && name.back() != '/');
                std::string back;
                check(d::folder_name_of(d::folder_dir_name(name), back) && back == name);
            }
            if (d::valid_mailbox_name(in) && !d::iequal(in, "INBOX")) {
                std::string back;
                const std::string dir = d::folder_dir_name(in);
                check(dir.find('/') == std::string::npos);
                // a directory name a file system holds (maildir_backend
                // refuses longer ones) reads back as the name
                if (dir.size() < 255) {
                    check(d::folder_name_of(dir, back) && back == in);
                }
            }
            break;
        }
        case 2: {
            uint32_t v = 0, n = 0;
            uint64_t ms = 0;
            std::vector<M> out;
            if (d::parse_uidlist(in, v, n, ms, out)) {
                check(v != 0 && n != 0);
                uint32_t last = 0;
                for (const auto& m : out) {
                    check(m.uid > last && m.uid < n && m.modseq <= ms);
                    check(m.base.find('/') == std::string::npos && m.base.find(':') == std::string::npos);
                    last = m.uid;
                }
            }
            break;
        }
        case 3: {
            std::vector<std::string> kw;
            d::parse_keywords(in, kw);
            check(kw.size() <= 26);
            for (const auto& k : kw) {
                check(!k.empty());
            }
            (void)d::valid_user_name(in);
            break;
        }
        case 4: {
            std::string out;
            if (d::utf7_decode(in, out)) {
                check(d::valid_utf8(out));
                check(d::utf7_encode(out) == in);
            }
            std::string lenient;
            if (d::utf7_decode(in, lenient, true)) {
                std::string again;
                check(d::utf7_decode(d::utf7_encode(lenient), again) && again == lenient);
            }
            if (d::valid_utf8(in)) {
                std::string back;
                check(d::utf7_decode(d::utf7_encode(in), back) && back == in);
            }
            break;
        }
    }
    return 0;
}
