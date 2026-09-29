//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The decoder of .lzma on any bytes, in memory and through the reader fed
// in pieces of every size the last byte picks; and against liblzma's
// lzma_alone_decoder: a stream liblzma decodes to its end, the library
// decodes too, to the same bytes. The header's dictionary is first made one
// of the sizes decoders allocate (2^n or 3 * 2^(n-1), at most 1 MiB):
// liblzma rounds it up that way and takes distances up to its rounded
// size, where the specification, and the library, stop at the header's.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/xz/include -L/opt/homebrew/opt/xz/lib -llzma" \
//       tests/fuzz/run.sh tests/compress/fuzz/lzma_fuzz.cpp 300
#include "sgcl/compress/compress.h"

#include <lzma.h>

#include <cstring>
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

    constexpr size_t Cap = size_t(1) << 24;

    // liblzma's decoding; ok when it reaches the end of the stream
    std::string xz_decompress(const uint8_t* data, size_t size, bool& ok) {
        lzma_stream s = LZMA_STREAM_INIT;
        ok = false;
        if (lzma_alone_decoder(&s, uint64_t(64) << 20) != LZMA_OK) {
            return {};
        }
        std::string out;
        uint8_t buf[1 << 14];
        s.next_in = data;
        s.avail_in = size;
        lzma_ret r;
        do {
            s.next_out = buf;
            s.avail_out = sizeof(buf);
            r = lzma_code(&s, LZMA_FINISH);
            out.append(reinterpret_cast<const char*>(buf), sizeof(buf) - s.avail_out);
        } while (r == LZMA_OK && out.size() < Cap);
        ok = r == LZMA_STREAM_END;
        lzma_end(&s);
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* raw, size_t size) {
    if (size == 0) {
        return 0;
    }
    size_t step = 1 + raw[size - 1] % 13;
    std::vector<uint8_t> input(raw, raw + size);
    if (size >= 5) {
        uint32_t dict = uint32_t(input[1]) | uint32_t(input[2]) << 8 | uint32_t(input[3]) << 16 | uint32_t(input[4]) << 24;
        uint32_t d = 4096;
        while (d < dict && d < (1u << 20)) {
            d = (d & (d - 1)) ? (d & (d - 1)) << 1 : d / 2 * 3;
        }
        for (int i = 0; i < 4; ++i) {
            input[1 + i] = uint8_t(d >> (8 * i));
        }
    }
    const uint8_t* data = input.data();
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    compress::limits l{Cap, uint64_t(64) << 20};
    auto whole = compress::lzma::decompress(in, l);
    {
        compress::lzma::reader r(pieces{data, size, step}, l);
        std::byte buf[4096];
        std::string got;
        bool failed = false;
        for (size_t guard = 0; guard < 8192; ++guard) {
            auto n = r.read(buf);
            if (!n) {
                failed = true;
                break;
            }
            if (*n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
        }
        // the reader and the whole-memory form agree on data that decodes
        if (whole && got.size() < Cap) {
            if (failed || got != std::string(reinterpret_cast<const char*>(whole->data()), whole->size())) {
                __builtin_trap();
            }
        }
        // and on data that does not, the reader fails too (unless the
        // limit of the whole form stopped it: the reader has none)
        if (!whole && whole.error().code() != compress::errc::too_large && !failed && got.size() < Cap) {
            __builtin_trap();
        }
    }
    // liblzma takes it: so does the library, with the same bytes
    bool ok = false;
    auto x = xz_decompress(data, size, ok);
    if (ok && x.size() < Cap) {
        if (!whole) {
            __builtin_trap();
        }
        if (std::string(reinterpret_cast<const char*>(whole->data()), whole->size()) != x) {
            __builtin_trap();
        }
    }
    return 0;
}
