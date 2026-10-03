//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress's Deflater on any data, round trip: the first byte picks the
// level (0..9, Huffman only) and the strategy (default or filtered, zlib's
// Z_FILTERED), the second the size of the pieces written (1..256, a flush
// after every fourth piece when its bit is set); the rest is the data. The
// stream inflated must be the data. Without the filtered strategy, the
// one-shot flate::compress (the thread's Deflater, reset from the inputs
// and levels before) must make what a new Deflater makes of the data.
//
//   tests/fuzz/run.sh tests/compress/fuzz/deflater_fuzz.cpp 300
#include "sgcl/compress/flate.h"

#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    using namespace sgcl;
    const int levels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, compress::level::huffman_only};
    const int level = levels[(data[0] & 0x7F) % 11];
    const bool filtered = data[0] & 0x80;
    const size_t piece = 1 + (data[1] & 0x7F) * 2;
    const bool flushes = data[1] & 0x80;
    data += 2;
    size -= 2;
    compress::detail::Deflater d(level, filtered);
    std::vector<uint8_t> out;
    size_t pieces = 0;
    for (size_t at = 0; at < size; at += piece) {
        d.write(data + at, std::min(piece, size - at), out);
        if (flushes && ++pieces % 4 == 0) {
            d.flush(out);
        }
    }
    d.finish(out);
    auto back = compress::flate::decompress(slice<const std::byte>(reinterpret_cast<const std::byte*>(out.data()), out.size()));
    if (!back || back->size() != size || (size && std::memcmp(back->data(), data, size) != 0)) {
        std::abort();
    }
    if (!filtered) {
        compress::detail::Deflater f(level);
        std::vector<uint8_t> want;
        f.write(data, size, want);
        f.finish(want);
        auto got = compress::flate::compress(slice<const std::byte>(reinterpret_cast<const std::byte*>(data), size), {.level = level});
        if (got.size() != want.size() || (!want.empty() && std::memcmp(got.data(), want.data(), want.size()) != 0)) {
            std::abort();
        }
    }
    return 0;
}
