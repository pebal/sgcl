//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The decoder of .xz on any bytes — every filter chain, check and number
// of blocks and streams — in memory and through the reader fed in pieces
// of every size the last byte picks; and against liblzma's
// lzma_stream_decoder (concatenated streams): what liblzma decodes to the
// end, the library decodes too, to the same bytes, and the other way
// round. Checks liblzma does not define it only warns about, where the
// library refuses them; such streams are left out of the comparison.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/xz/include -L/opt/homebrew/opt/xz/lib -llzma" \
//       tests/fuzz/run.sh tests/compress/fuzz/xz_fuzz.cpp 300
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
    constexpr uint64_t Memory = uint64_t(64) << 20;

    std::string xz_decompress(const uint8_t* data, size_t size, bool& ok, bool& memory) {
        lzma_stream s = LZMA_STREAM_INIT;
        ok = false;
        memory = false;
        if (lzma_stream_decoder(&s, Memory, LZMA_CONCATENATED | LZMA_TELL_UNSUPPORTED_CHECK) != LZMA_OK) {
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
        memory = r == LZMA_MEMLIMIT_ERROR;
        lzma_end(&s);
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    size_t step = 1 + data[size - 1] % 13;
    slice<const std::byte> in(reinterpret_cast<const std::byte*>(data), size);
    compress::limits l{Cap, Memory};
    auto whole = compress::xz::decompress(in, l);
    {
        compress::xz::reader r(pieces{data, size, step}, l);
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
        if (whole && got.size() < Cap) {
            if (failed || got != std::string(reinterpret_cast<const char*>(whole->data()), whole->size())) {
                __builtin_trap();
            }
        }
        if (!whole && whole.error().code() != compress::errc::too_large && !failed && got.size() < Cap) {
            __builtin_trap();
        }
    }
    bool ok = false, memory = false;
    auto x = xz_decompress(data, size, ok, memory);
    if (x.size() >= Cap || memory) {
        return 0;
    }
    if (ok && !whole) {
        if (whole.error().code() == compress::errc::too_large) {
            return 0;
        }
        __builtin_trap();
    }
    if (whole && !ok) {
        __builtin_trap();
    }
    if (ok && std::string(reinterpret_cast<const char*>(whole->data()), whole->size()) != x) {
        __builtin_trap();
    }
    return 0;
}
