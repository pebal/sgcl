//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// PPMd var.H (7z's) with no oracle at hand: the first two bytes pick the
// order (2..64) and the model's memory (2 KiB .. 1 MiB: small, so that the
// allocator glues and the model restarts often), the third the size of
// the output pieces; the rest is taken both as coded data — decoded in
// one call and again fed a few bytes at a time into pieces of output,
// the two agreeing to the byte and on the failure — and as plain data,
// which the encoder codes and the decoder must give back byte for byte.
// Under ASan and UBSan every step of the model runs on bytes from outside.
//
//   tests/fuzz/run.sh tests/compress/fuzz/ppmd_fuzz.cpp 300
#include "sgcl/compress/detail/ppmd7.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace sgcl::compress::detail;

    // The data decoded to `size` bytes in one call; ok when done
    std::string whole(uint32_t order, uint32_t memory, const uint8_t* p, size_t n, size_t size, bool& ok) {
        auto d = std::make_unique<Ppmd7Decoder>();
        d->reset(order, memory, size);
        std::string out(size, 0);
        size_t pos = 0;
        const uint8_t* in = p;
        auto st = d->decode(reinterpret_cast<uint8_t*>(out.data()), pos, size, in, p + n, true);
        out.resize(pos);
        ok = st == LzmaStatus::done;
        return out;
    }

    // The same with the input handed over `feed` bytes at a time (kept
    // until the decoder takes it) and the output taken in pieces of `piece`
    std::string pieces(uint32_t order, uint32_t memory, const uint8_t* p, size_t n, size_t size, size_t feed, size_t piece, bool& ok) {
        auto d = std::make_unique<Ppmd7Decoder>();
        d->reset(order, memory, size);
        std::string out(size, 0);
        std::vector<uint8_t> buf;
        size_t given = 0;
        size_t pos = 0;
        for (size_t guard = 0; guard < 10000000; ++guard) {
            const uint8_t* in = buf.data();
            size_t limit = std::min(size, pos + piece);
            auto st = d->decode(reinterpret_cast<uint8_t*>(out.data()), pos, limit, in, buf.data() + buf.size(), given == n);
            buf.erase(buf.begin(), buf.begin() + (in - buf.data()));
            if (st == LzmaStatus::done || st == LzmaStatus::failed) {
                ok = st == LzmaStatus::done;
                out.resize(pos);
                return out;
            }
            if (st == LzmaStatus::need_input) {
                size_t k = std::min(feed, n - given);
                buf.insert(buf.end(), p + given, p + given + k);
                given += k;
            }
        }
        __builtin_trap();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    uint32_t order = ppmd::MinOrder + data[0] % (ppmd::MaxOrder - ppmd::MinOrder + 1);
    uint32_t memory = ppmd::MinMemory << (data[1] % 10);
    size_t piece = 1 + data[2] % 97;
    const uint8_t* p = data + 3;
    size_t n = size - 3;
    // as coded data: to twice its size or so, one call against pieces
    size_t out_size = n * 3 + 16;
    bool ok1 = false, ok2 = false;
    auto a = whole(order, memory, p, n, out_size, ok1);
    auto b = pieces(order, memory, p, n, out_size, 1 + data[2] % 7, piece, ok2);
    if (ok1 != ok2 || a != b) {
        __builtin_trap();
    }
    // as plain data: coded and decoded back
    auto e = std::make_unique<Ppmd7Encoder>(order, memory);
    std::vector<uint8_t> c;
    e->encode(p, n, c);
    e->finish(c, data[2] & 1);
    bool ok3 = false;
    auto back = whole(order, memory, c.data(), c.size(), n, ok3);
    if (!ok3 || back != std::string(reinterpret_cast<const char*>(p), n)) {
        __builtin_trap();
    }
    bool ok4 = false;
    auto back2 = pieces(order, memory, c.data(), c.size(), n, 3, piece, ok4);
    if (!ok4 || back2 != back) {
        __builtin_trap();
    }
    return 0;
}
