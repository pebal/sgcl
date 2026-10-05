//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::email's parser on any bytes, and the writer over what it read.
// The first byte picks the write options (8bit, UTF-8, the Bcc) and the
// limits (small or the defaults). What must hold:
//   - a parse succeeds or fails with a limit (limit_exceeded, depth_limit);
//   - a message read is written, and what is written parses;
//   - once written by the library a message is stable: the message read
//     from the first writing and the one read from its writing again say
//     the same (the subject, the addresses, the text, the HTML, the parts
//     with their types, names and contents);
//   - every line written ends with CRLF.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/email_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/email.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void tree(const email::part& p, std::string& out, int depth) {
        out += std::string(p.content_type().view());
        out += '|';
        out += std::string(p.filename().view());
        out += '|';
        out += std::string(p.content_id().view());
        out += '|';
        if (p.is_multipart() && !p.parts().empty()) {
            out += '{';
            for (auto& c : p.parts()) {
                tree(c, out, depth + 1);
            }
            out += '}';
        } else if (auto m = p.message()) {
            out += '[';
            tree(m->body(), out, depth + 1);
            out += ']';
        } else {
            out += std::string(p.text().view());
        }
        out += ';';
    }

    std::string summary(const email& m) {
        std::string out = std::string(m.subject().view()) + "\n";
        if (auto f = m.from()) {
            out += std::string(f->to_string().view());
        }
        out += "\n";
        for (auto& a : m.to()) {
            out += std::string(a.name().view()) + "<" + std::string(a.addr().view()) + ">";
        }
        out += "\n" + std::string(m.text().view()) + "\n" + std::string(m.html().view()) + "\n";
        out += std::to_string(m.attachments().size()) + "\n";
        tree(m.body(), out, 0);
        return out;
    }

    void lines_end_with_crlf(std::string_view t) {
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] == '\n') {
                check(i > 0 && t[i - 1] == '\r');
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const uint8_t mode = data[0];
    email::write_options o;
    o.allow_8bit = mode & 1;
    o.allow_utf8 = mode & 2;
    o.write_bcc = !(mode & 4);
    email::limits l;
    if (mode & 8) {
        l.max_header_bytes = 512;
        l.max_depth = 4;
        l.max_parts = 16;
    }
    std::string_view in(reinterpret_cast<const char*>(data + 1), size - 1);
    auto m = email::parse(slice<const byte>(reinterpret_cast<const byte*>(in.data()), in.size()), l);
    if (!m) {
        check(m.error().code() == errc::limit_exceeded || m.error().code() == errc::depth_limit);
        return 0;
    }
    (void)summary(*m);
    auto first = m->to_string(o);
    email::limits big;
    big.max_depth = 512;
    big.max_parts = 1u << 20;
    big.max_header_bytes = 1u << 30;
    auto a = email::parse(first, big);
    check(a.has_value());
    auto second = a->to_string(o);
    auto b = email::parse(second, big);
    check(b.has_value());
    check(summary(*a) == summary(*b));
    if (!o.allow_8bit && !o.allow_utf8) {
        lines_end_with_crlf(second.view());
    }
    return 0;
}
