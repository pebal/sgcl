//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The 7z writer on what the input says: its first bytes pick the method,
// the level (0..3, to keep a run short), solid or not, a small solid block
// and the automatic filters; the rest is a list of entries — a kind byte
// (file, directory, link, a file written in pieces), a name and data of
// lengths the input gives. The archive, written into a buffer, is read
// back and must hold every entry as written, data and kinds, unless the
// writer kept an error (a name it refuses), which close() then returns.
//
//   tests/fuzz/run.sh tests/compress/fuzz/sevenzip_writer_fuzz.cpp 300
#include "sgcl/compress/sevenzip.h"
#include "tests/fuzz/input.h"

#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    struct Input {
        const uint8_t* p;
        size_t n;
        size_t at = 0;

        uint8_t byte() {
            return at < n ? p[at++] : 0;
        }

        std::string bytes(size_t k) {
            k = std::min(k, n - at);
            std::string s(reinterpret_cast<const char*>(p + at), k);
            at += k;
            return s;
        }
    };

    struct Written {
        std::string name;
        std::string data;
        int kind;   // 0 file, 1 directory, 2 link
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    Input in{data, size};
    uint8_t flags = in.byte();
    compress::sevenzip::options o;
    o.method = compress::sevenzip::method(flags % 5);
    o.level = (flags >> 3) & 3;
    o.solid = !(flags & 0x20);
    o.auto_filters = flags & 0x40;
    o.solid_block = (flags & 0x80) ? 3000 : 0;
    io::buffer b;
    std::vector<Written> written;
    {
        compress::sevenzip::writer w(b, o);
        while (in.at < in.n && written.size() < 64) {
            uint8_t kind = in.byte() % 4;
            std::string name = in.bytes(in.byte() % 24);
            size_t len = size_t(in.byte()) * 17 + in.byte();
            std::string body = in.bytes(len);
            // the names the reader gives back: no trailing '/', and one name
            // once (the test compares by position)
            while (name.size() > 1 && name.back() == '/') {
                name.pop_back();
            }
            // the data from a buffer of its own size outside the managed
            // heap (tests/fuzz/input.h), so that ASan sees an overread of
            // the encoder; the name is a string (the writer takes no other)
            const sgcl_fuzz::exact data(body);
            if (kind == 1) {
                w.add_directory(string(name));
                written.push_back({name, "", 1});
            } else if (kind == 2) {
                compress::sevenzip::entry_info info;
                info.symlink = true;
                w.add(string(name), data.bytes(), info);
                written.push_back({name, body, 2});
            } else if (kind == 3) {
                io::writer e = w.create(string(name));
                for (size_t i = 0; i < body.size(); i += 7) {
                    const sgcl_fuzz::exact piece(std::string_view(body).substr(i, 7));
                    (void)e.write(piece.bytes());
                }
                written.push_back({name, body, 0});
            } else {
                w.add(string(name), data.bytes());
                written.push_back({name, body, 0});
            }
        }
        auto r = w.close();
        if (!r) {
            // a name the writer refuses (empty, a NUL, not UTF-8) is its first error
            if (r.error().code() != compress::errc::invalid_argument) {
                __builtin_trap();
            }
            return 0;
        }
    }
    auto a = compress::sevenzip::archive::from(b.data());
    if (!a) {
        __builtin_trap();
    }
    auto entries = a->entries();
    if (entries.size() != written.size()) {
        __builtin_trap();
    }
    for (size_t i = 0; i < written.size(); ++i) {
        const auto& e = entries[i];
        const auto& x = written[i];
        if (std::string(e.name.view()) != x.name || e.is_directory != (x.kind == 1) || e.is_symlink() != (x.kind == 2)) {
            __builtin_trap();
        }
        auto d = a->read(e);
        if (!d || std::string(reinterpret_cast<const char*>(d->data()), d->size()) != x.data) {
            __builtin_trap();
        }
    }
    return 0;
}
