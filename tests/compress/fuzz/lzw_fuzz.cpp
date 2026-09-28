//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// LZW on any bytes: the first byte picks the order and the literal width,
// the rest is decoded in memory and through the reader fed in pieces of
// every size the last byte picks; and the rest, cut to the width, is
// compressed (in memory and through the writer in pieces) and must come
// back as it was.
#include "sgcl/compress/compress.h"

#include <cstring>
#include <string>

namespace {
    using namespace sgcl;
    using compress::lzw;

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

    std::string text(const vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    auto o = data[0] & 1 ? lzw::order::msb : lzw::order::lsb;
    int width = 2 + (data[0] >> 1) % 7;
    size_t step = 1 + data[size - 1] % 13;
    ++data;
    --size;
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    auto whole = lzw::decompress(in, o, width, compress::limits{uint64_t(1) << 24});
    {
        lzw::reader r(pieces{data, size, step}, o, width);
        std::byte buf[1000];
        std::string got;
        bool failed = false;
        bool ended = false;
        // bounded by what it gives, not by the reads: a read may give a byte
        while (got.size() <= (size_t(1) << 24)) {
            auto n = r.read(buf);
            if (!n) {
                failed = true;
                break;
            }
            if (*n == 0) {
                ended = true;
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        if (whole && (failed || (ended && got != text(*whole)))) {
            __builtin_trap();
        }
    }
    // there and back
    std::string plain(reinterpret_cast<const char*>(data), size);
    for (auto& c : plain) {
        c = char(uint8_t(c) & ((1u << width) - 1));
    }
    slice<const std::byte> p(reinterpret_cast<const std::byte*>(plain.data()), plain.size());
    auto c = lzw::compress(p, o, width);
    auto back = lzw::decompress(slice<const std::byte>(c.data(), c.size()), o, width, compress::limits{UINT64_MAX});
    if (!back || text(*back) != plain) {
        __builtin_trap();
    }
    io::buffer sink;
    lzw::writer w(sink, o, width);
    for (size_t i = 0; i < plain.size(); i += step) {
        if (!w.write(slice<const std::byte>(p.data() + i, std::min(step, plain.size() - i)))) {
            __builtin_trap();
        }
    }
    if (!w.close() || sink.size() != c.size() || std::memcmp(sink.data().data(), c.data(), c.size()) != 0) {
        __builtin_trap();
    }
    return 0;
}
