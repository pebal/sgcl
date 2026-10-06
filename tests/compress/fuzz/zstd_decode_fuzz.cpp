//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The zstd decoder alone on any bytes (zstd_fuzz spends most of its time
// compressing; this one only reads frames, many more of them a second):
// decompress in memory and the reader fed in pieces of every size the last
// byte picks must agree, and what libzstd decodes (its window held to the
// same 16 MB), the library decodes too, to the same bytes. The input is
// libFuzzer's own buffer, of exactly its size, which ASan guards.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/zstd/include -L/opt/homebrew/opt/zstd/lib -lzstd" \
//       tests/fuzz/run.sh tests/compress/fuzz/zstd_decode_fuzz.cpp 300
#include "sgcl/compress/compress.h"

#include <zstd.h>

#include <cstring>
#include <string>

namespace {
    using namespace sgcl;
    using zstd = compress::zstd;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

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

    constexpr size_t Cap = size_t(1) << 22;
    constexpr unsigned WindowLog = 24;

    std::string str(const vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const size_t step = 1 + data[size - 1] % 13 * 997 % 4099;
    compress::limits l {Cap, uint64_t(1) << WindowLog};
    auto whole = zstd::decompress(slice<const std::byte>(reinterpret_cast<const std::byte*>(data), size), l);
    {
        zstd::reader r(pieces {data, size, step}, l);
        std::byte buf[4096];
        std::string got;
        bool failed = false;
        for (;;) {
            auto n = r.read(slice<std::byte>(buf, sizeof buf));
            if (!n) {
                failed = true;
                break;
            }
            if (*n == 0) {
                break;
            }
            got.append(reinterpret_cast<const char*>(buf), *n);
            if (got.size() > Cap) {
                failed = true;
                break;
            }
        }
        if (whole) {
            check(!failed && got == str(*whole));
        } else if (whole.error().code() != compress::errc::too_large) {
            check(failed);
        }
    }
    // what libzstd decodes, the library decodes alike; but a frame naming a
    // dictionary, which libzstd reads without one, and Huffman streams that
    // end before their symbols do: libzstd's fast loop over four streams
    // never checks that each was read to its start and no further (it reads
    // zeros past it), the library refuses them (seeds/zstd_decode/
    // regression_huffman_past_start: the first stream read 6 bits past its start)
    ZSTD_DCtx* d = ZSTD_createDCtx();
    ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, int(WindowLog));
    std::string ref(Cap, '\0');
    const size_t r = ZSTD_decompressDCtx(d, ref.data(), ref.size(), data, size);
    ZSTD_freeDCtx(d);
    const bool lenient = !whole && (whole.error().code() == compress::errc::dictionary_required ||
                                    whole.error().message().find("Huffman-coded literals that do not decode") != std::string::npos);
    if (!ZSTD_isError(r) && !lenient) {
        ref.resize(r);
        check(whole && str(*whole) == ref);
    }
    return 0;
}
