//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The writers of compress, round trip: any data written in pieces of the
// sizes the input gives (flushes between them where a format has them),
// at the level and with the options it gives, into an io::buffer, then
// read back by the format's reader in pieces of other sizes: the bytes
// must be the data. For the archives, entries (files written in pieces,
// directories, links) and their data must come back as written.
//
//   deflate, gzip (a header with a name and a comment; a name of the
//   data's first bytes reads back as written when it is UTF-8), zlib (a
//   dictionary from the input, both sides), lzma (small dictionaries, lc/lp/pb), xz (a
//   check, a BCJ filter or Delta), tar, zip (stored and deflated entries)
//
// The 7z writer has its own harness (sevenzip_writer_fuzz.cpp). The input:
// the format, the level, a byte of flags, four piece sizes (1..2041), then
// the data (for the archives: entries of a kind byte, a length and data).
//
//   tests/fuzz/run.sh tests/compress/fuzz/writers_fuzz.cpp 300
#include "sgcl/compress/flate.h"
#include "sgcl/compress/gzip.h"
#include "sgcl/compress/lzma.h"
#include "sgcl/compress/tar.h"
#include "sgcl/compress/xz.h"
#include "sgcl/compress/zip.h"
#include "sgcl/compress/zlib.h"
#include "sgcl/core/utf8.h"
#include "tests/fuzz/input.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    struct Pieces {
        size_t size[4];
        size_t at = 0;

        size_t next() {
            return size[at++ % 4];
        }
    };

    // The data into w in pieces, a flush now and then where the writer
    // has one; then closed
    template<class W>
    void write_all(W& w, const uint8_t* p, size_t n, Pieces pz, uint8_t flags) {
        size_t piece = 0;
        for (size_t i = 0; i < n;) {
            const size_t take = std::min(pz.next(), n - i);
            check(w.write(view(p + i, take)).has_value());
            i += take;
            if constexpr (requires { w.flush(); }) {
                if ((flags >> (piece++ % 8)) & 1) {
                    check(w.flush().has_value());
                }
            }
        }
        check(w.close().has_value());
    }

    // r read to its end in pieces, compared with the data
    template<class R>
    void read_back(R& r, const uint8_t* p, size_t n, Pieces pz) {
        std::vector<uint8_t> got;
        std::vector<uint8_t> b(2048);
        for (;;) {
            const size_t want = pz.next();
            auto k = r.read(slice<byte>(reinterpret_cast<byte*>(b.data()), want));
            check(k.has_value());
            if (*k == 0) {
                break;
            }
            got.insert(got.end(), b.begin(), b.begin() + *k);
            check(got.size() <= n);
        }
        check(got.size() == n && (n == 0 || std::memcmp(got.data(), p, n) == 0));
    }

    struct Input {
        const uint8_t* p;
        size_t n;
        size_t at = 0;

        uint8_t byte() {
            return at < n ? p[at++] : 0;
        }

        std::string take(size_t k) {
            k = std::min(k, n - at);
            std::string s(reinterpret_cast<const char*>(p + at), k);
            at += k;
            return s;
        }
    };

    struct Entry {
        int kind;   // 0 file, 1 directory, 2 symlink
        std::string name, data;
    };

    std::vector<Entry> entries_of(Input& in, bool zip) {
        std::vector<Entry> out;
        while (in.at < in.n && out.size() < 32) {
            const uint8_t k = in.byte();
            const size_t len = size_t(in.byte()) << (k & 0x30 ? 4 : 0);
            Entry e;
            e.kind = k % 3;
            const std::string id = std::to_string(out.size());
            if (e.kind == 1) {
                e.name = "dir" + id + (zip ? "/" : "");
            } else {
                e.name = (k & 0x80 ? "sub/" : "") + std::string("entry") + id + ".bin";
                e.data = in.take(e.kind == 2 ? std::min<size_t>(len, 100) + 1 : len);
                if (e.kind == 2) {
                    // a link's target: text without NUL
                    std::replace(e.data.begin(), e.data.end(), '\0', 'x');
                }
            }
            out.push_back(std::move(e));
        }
        return out;
    }

    void run_tar(Input& in, Pieces pz) {
        auto entries = entries_of(in, false);
        io::buffer b;
        {
            compress::tar::writer w(b);
            for (auto& e : entries) {
                compress::tar::entry h;
                h.name = string(e.name);
                if (e.kind == 0) {
                    h.type = compress::tar::kind::file;
                    h.size = e.data.size();
                } else if (e.kind == 1) {
                    h.type = compress::tar::kind::directory;
                } else {
                    h.type = compress::tar::kind::symlink;
                    h.link_name = string(e.data);
                }
                check(w.write_header(h).has_value());
                if (e.kind == 0) {
                    for (size_t i = 0; i < e.data.size();) {
                        const size_t take = std::min(pz.next(), e.data.size() - i);
                        check(w.write(view(reinterpret_cast<const uint8_t*>(e.data.data()) + i, take)).has_value());
                        i += take;
                    }
                }
            }
            check(w.close().has_value());
        }
        compress::tar::reader r(b);
        for (auto& e : entries) {
            auto h = r.next();
            check(h.has_value() && bool(*h));
            const auto& got = **h;
            std::string name(got.name.view());
            if (e.kind == 1) {
                check(got.type == compress::tar::kind::directory && (name == e.name || name == e.name + "/"));
                continue;
            }
            check(name == e.name);
            if (e.kind == 2) {
                check(got.type == compress::tar::kind::symlink && std::string(got.link_name.view()) == e.data);
                continue;
            }
            check(got.type == compress::tar::kind::file && got.size == e.data.size());
            read_back(r, reinterpret_cast<const uint8_t*>(e.data.data()), e.data.size(), pz);
        }
        auto end = r.next();
        check(end.has_value() && !*end);
    }

    void run_zip(Input& in, Pieces pz, uint8_t flags) {
        auto entries = entries_of(in, true);
        io::buffer b;
        {
            compress::zip::writer w(b);
            size_t i = 0;
            for (auto& e : entries) {
                if (e.kind == 2) {
                    e.kind = 0;   // a link: written as a plain file here
                }
                const bool stored = (flags >> (i++ % 8)) & 1;
                if (e.kind == 1) {
                    check(w.create(string(e.name)).has_value());
                } else if (stored && !e.data.empty() && e.data.size() % 2) {
                    const sgcl_fuzz::exact whole(e.data);   // a buffer of the data's own size (tests/fuzz/input.h)
                    check(w.add(string(e.name), whole.bytes()).has_value());
                } else {
                    compress::zip::entry h;
                    h.name = string(e.name);
                    h.method = stored ? compress::zip::method::store : compress::zip::method::deflate;
                    auto out = w.create(h);
                    check(out.has_value());
                    for (size_t k = 0; k < e.data.size();) {
                        const size_t take = std::min(pz.next(), e.data.size() - k);
                        check(out->write(view(reinterpret_cast<const uint8_t*>(e.data.data()) + k, take)).has_value());
                        k += take;
                    }
                    check(out->close().has_value());
                }
            }
            check(w.close().has_value());
        }
        auto bytes = b.release();
        auto a = compress::zip::archive::from(bytes.as_slice());
        check(a.has_value());
        auto all = a->entries();
        check(all.size() == entries.size());
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            check(std::string(all[i].name.view()) == e.name);
            if (e.kind == 1) {
                check(all[i].is_directory());
                continue;
            }
            auto data = a->read(all[i]);
            check(data.has_value() && data->size() == e.data.size() && (e.data.empty() || std::memcmp(data->data(), e.data.data(), e.data.size()) == 0));
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 7) {
        return 0;
    }
    const uint8_t format = data[0] % 7;
    const uint8_t lvl = data[1];
    const uint8_t flags = data[2];
    Pieces write_pz, read_pz;
    for (int i = 0; i < 4; ++i) {
        write_pz.size[i] = size_t(data[3 + i]) * 8 + 1;
        read_pz.size[i] = size_t(data[6 - i]) * 8 + 1;
    }
    const uint8_t* p = data + 7;
    const size_t n = size - 7;
    const int level = lvl % 11 == 10 ? compress::level::huffman_only : lvl % 10;

    switch (format) {
    case 0: {
        io::buffer b;
        {
            compress::flate::writer w(b, {.level = level});
            write_all(w, p, n, write_pz, flags);
        }
        compress::flate::reader r(b);
        read_back(r, p, n, read_pz);
        break;
    }
    case 1: {
        io::buffer b;
        compress::gzip::options o;
        o.level = level;
        o.header.name = string("name.txt");
        o.header.comment = string(flags & 1 ? "a comment" : "");
        if (flags & 2) {
            // a name of the data's first bytes, NULs made 'x': UTF-8 reads
            // back as it was written, past ISO 8859-1 too
            std::string name(reinterpret_cast<const char*>(p), std::min<size_t>(n, 1 + lvl % 24));
            std::replace(name.begin(), name.end(), '\0', 'x');
            o.header.name = string(name);
        }
        {
            compress::gzip::writer w(b, o);
            write_all(w, p, n, write_pz, flags);
        }
        compress::gzip::reader r(b);
        if (flags & 2) {
            auto h = r.header();
            check(h.has_value());
            if (utf8::valid(o.header.name.view())) {
                check(h->name == o.header.name);
            }
        }
        read_back(r, p, n, read_pz);
        break;
    }
    case 2: {
        // a dictionary of the data's first bytes, on both sides
        const size_t dn = (flags & 1) ? std::min<size_t>(n, lvl) : 0;
        io::buffer b;
        compress::zlib::options o;
        o.level = level;
        o.dictionary = view(p, dn);
        {
            compress::zlib::writer w(b, o);
            write_all(w, p, n, write_pz, flags);
        }
        compress::zlib::reader r(b, o);
        read_back(r, p, n, read_pz);
        break;
    }
    case 3: {
        compress::lzma::options o;
        o.level = lvl % 4;
        o.dictionary = uint32_t(4096) << (flags % 5);
        o.lc = (flags >> 3) % 5;
        o.lp = (flags >> 6) % 3;
        o.pb = lvl % 3;
        o.extreme = (lvl & 0x80) != 0;
        io::buffer b;
        {
            compress::lzma::writer w(b, o);
            write_all(w, p, n, write_pz, flags);
        }
        compress::lzma::reader r(b);
        read_back(r, p, n, read_pz);
        break;
    }
    case 4: {
        compress::xz::options o;
        o.level = lvl % 4;
        o.dictionary = uint32_t(4096) << (flags % 5);
        static constexpr compress::xz::check checks[] = {compress::xz::check::none, compress::xz::check::crc32, compress::xz::check::crc64, compress::xz::check::sha256};
        o.check = checks[(flags >> 3) % 4];
        if (flags & 0x20) {
            o.bcj = compress::xz::filter((lvl >> 4) % 8);
        } else if (flags & 0x40) {
            o.delta = uint16_t((lvl >> 2) % 255 + 1);
        }
        io::buffer b;
        {
            compress::xz::writer w(b, o);
            write_all(w, p, n, write_pz, flags);
        }
        compress::xz::reader r(b);
        read_back(r, p, n, read_pz);
        break;
    }
    case 5: {
        Input in{p, n};
        run_tar(in, write_pz);
        break;
    }
    default: {
        Input in{p, n};
        run_zip(in, write_pz, flags);
        break;
    }
    }
    return 0;
}
