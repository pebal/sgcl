//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The decoders of DEFLATE on any bytes: raw, zlib and gzip, in memory and
// through the readers fed in pieces of every size the last byte picks; and
// against zlib: what zlib takes, the library takes, with the same bytes
// (the other way is not asked: RFC 1951 leaves decoders some room, and
// zlib takes an incomplete code of one symbol where we do too). The last
// byte's top bit switches to Deflate64 (zip's method 9, 7z's 040109):
// its decoder in memory and through the reader fed in pieces, the two
// agreeing where both end.
#include "sgcl/compress/compress.h"

#include <zlib.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    struct pieces {
        const uint8_t* data;
        size_t size;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(slice<std::byte> b) {
            size_t n = std::min({step, b.size(), size - at});
            std::memcpy(b.data(), data + at, n);
            at += n;
            return n;
        }
    };

    // Deflate64 whole in memory, the output grown up to 16 MB; false when it does not end
    bool inflate64_all(const uint8_t* data, size_t size, std::string& out) {
        auto s = std::make_unique<compress::detail::InflateState>();
        s->reset();
        std::vector<uint8_t> buf(1024);
        size_t pos = 0;
        const uint8_t* in = data;
        for (;;) {
            auto st = compress::detail::inflate64(*s, in, data + size, buf.data(), pos, buf.size());
            if (st == compress::detail::InflateStatus::done) {
                out.assign(reinterpret_cast<const char*>(buf.data()), pos);
                return true;
            }
            if (st != compress::detail::InflateStatus::need_room || buf.size() >= (size_t(1) << 24)) {
                return false;
            }
            buf.resize(buf.size() * 2);
        }
    }

    void deflate64(const uint8_t* data, size_t size, size_t step) {
        std::string whole;
        bool ok = inflate64_all(data, size, whole);
        compress::flate::reader r(pieces{data, size, step});
        compress::detail::use_deflate64(r);
        std::byte buf[4096];
        std::string got;
        bool ended = false;
        while (got.size() <= (1u << 24)) {
            auto n = r.read(buf);
            if (!n) {
                break;
            }
            if (*n == 0) {
                ended = true;
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        if (ok != ended || (ok && got != whole)) {
            __builtin_trap();
        }
    }

    std::string zlib_inflate(const uint8_t* data, size_t size, int bits, bool& ok) {
        z_stream d{};
        inflateInit2(&d, bits);
        std::string out;
        char buf[1 << 14];
        d.next_in = const_cast<Bytef*>(data);
        d.avail_in = uInt(size);
        int st;
        do {
            d.next_out = reinterpret_cast<Bytef*>(buf);
            d.avail_out = sizeof(buf);
            st = inflate(&d, Z_NO_FLUSH);
            out.append(buf, sizeof(buf) - d.avail_out);
        } while (st == Z_OK && out.size() < (1u << 24));
        ok = st == Z_STREAM_END;
        inflateEnd(&d);
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    size_t step = 1 + data[size - 1] % 13;
    if (data[size - 1] & 0x80) {
        deflate64(data, size, step);
        return 0;
    }
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    compress::limits l{uint64_t(1) << 24};
    auto raw = compress::flate::decompress(in, l);
    (void)compress::zlib::decompress(in, l);
    (void)compress::gzip::decompress(in, l);
    {
        compress::flate::reader r(pieces{data, size, step});
        std::byte buf[4096];
        std::string got;
        bool ended = false;
        while (got.size() <= (1u << 22)) {
            auto n = r.read(buf);
            if (!n) {
                break;
            }
            if (*n == 0) {
                ended = true;
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        // the reader and the whole-memory form agree on data that decodes
        if (raw && ended) {
            if (got != std::string(reinterpret_cast<const char*>(raw->data()), raw->size())) {
                __builtin_trap();
            }
        }
    }
    {
        compress::gzip::reader r(pieces{data, size, step});
        std::byte buf[4096];
        for (size_t guard = 0; guard < (1u << 20); ++guard) {
            auto n = r.read(buf);
            if (!n || *n == 0) {
                break;
            }
        }
    }
    // the encoder at the level the first byte picks, on the input taken
    // as data: zlib and the library both read back what it made
    {
        int level = data[0] % 11 == 10 ? compress::level::huffman_only : data[0] % 11;
        auto c = compress::flate::compress(in, {.level = level});
        bool ok = false;
        auto z = zlib_inflate(reinterpret_cast<const uint8_t*>(c.data()), c.size(), -15, ok);
        auto ours = compress::flate::decompress(slice<const std::byte>(c.data(), c.size()), compress::limits{UINT64_MAX});
        if (!ok || z != std::string(reinterpret_cast<const char*>(data), size) || !ours || ours->size() != size || std::memcmp(ours->data(), data, size) != 0) {
            __builtin_trap();
        }
    }
    // zlib takes it: so does the library, with the same bytes
    bool zok = false;
    auto z = zlib_inflate(data, size, -15, zok);
    if (zok && z.size() < (1u << 24)) {
        if (!raw || std::string(reinterpret_cast<const char*>(raw->data()), raw->size()) != z) {
            __builtin_trap();
        }
    }
    return 0;
}
