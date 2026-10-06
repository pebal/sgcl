//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// zstd on any bytes, two ways:
//   - the bytes as frames: decompress in memory and the reader fed in
//     pieces of every size the last byte picks must agree; and against
//     libzstd (its window held to the same 16 MB): what libzstd decodes,
//     the library decodes too, to the same bytes;
//   - the bytes as data: compressed at a level and with options the first
//     bytes pick (now and then against a raw dictionary cut from the data),
//     by compress and by the writer in pieces, decoded by the library and by
//     libzstd to the same.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/zstd/include -L/opt/homebrew/opt/zstd/lib -lzstd" \
//       tests/fuzz/run.sh tests/compress/fuzz/zstd_fuzz.cpp 300
#include "sgcl/compress/compress.h"
#include "tests/fuzz/input.h"

#include <zstd.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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

    // A copy of exactly n bytes in memory of malloc's, which ASan guards:
    // the decoder never reads frames from a managed buffer here (ASan does
    // not see a read past the end of one)
    struct exact {
        uint8_t* p;
        size_t n;

        template<class B>
        explicit exact(const B& b)
        : p(static_cast<uint8_t*>(std::malloc(b.size() ? b.size() : 1)))
        , n(b.size()) {
            std::memcpy(p, b.data(), n);
        }

        ~exact() {
            std::free(p);
        }

        exact(const exact&) = delete;
        exact& operator=(const exact&) = delete;
    };

    constexpr size_t Cap = size_t(1) << 22;

    constexpr unsigned WindowLog = 24;

    std::string str(const vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    slice<const std::byte> view(const uint8_t* p, size_t n) {
        return slice<const std::byte>(reinterpret_cast<const std::byte*>(p), n);
    }

    // libzstd's reading of every frame, its window held to 2^WindowLog
    bool theirs(const uint8_t* data, size_t size, const std::string& dict, std::string& out) {
        ZSTD_DCtx* d = ZSTD_createDCtx();
        ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, int(WindowLog));
        if (!dict.empty()) {
            ZSTD_DCtx_refPrefix(d, dict.data(), dict.size());   // raw content, as the library's raw dictionary
        }
        out.assign(Cap, '\0');
        const size_t r = ZSTD_decompressDCtx(d, out.data(), out.size(), data, size);
        ZSTD_freeDCtx(d);
        if (ZSTD_isError(r)) {
            return false;
        }
        out.resize(r);
        return true;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const size_t step = 1 + data[size - 1] % 13;
    compress::limits l {Cap, uint64_t(1) << WindowLog};

    // as frames
    auto whole = zstd::decompress(view(data, size), l);
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
    // what libzstd decodes, the library decodes alike (a frame naming a
    // dictionary libzstd reads without one when no match reaches before
    // the data; the library asks for it; Huffman streams that end before
    // their symbols do, which libzstd's fast loop does not check and the
    // library refuses: see zstd_decode_fuzz.cpp)
    std::string ref;
    const bool lenient = !whole && (whole.error().code() == compress::errc::dictionary_required ||
                                    whole.error().message().find("Huffman-coded literals that do not decode") != std::string::npos);
    if (theirs(data, size, "", ref) && !lenient) {
        check(whole && str(*whole) == ref);
    }

    // as data
    if (size >= 3) {
        static const int levels[] = {1, 2, 3, 4, 5, 6, 7, 9, 12, 13, 15, 16, 17, 19, 22, -1, -5, -100};
        const int level = levels[data[0] % 18];
        const uint8_t flags = data[1];
        const uint8_t* p = data + 2;
        const size_t n = size - 2;
        std::string dict;
        if ((flags & 64) && n >= 8) {
            dict.assign(reinterpret_cast<const char*>(p), n / 3);
        }
        zstd::options o {.level = level, .checksum = bool(flags & 1), .content_size = bool(flags & 2)};
        if (flags & 4) {
            o.window_log = uint8_t(10 + (flags >> 3) % 8);
        } else if (level >= 20) {
            o.window_log = uint8_t(WindowLog);   // the writer's window of --ultra is past the limit
        }
        if (!dict.empty()) {
            // parsed from a buffer of its own size (tests/fuzz/input.h)
            const sgcl_fuzz::exact bytes(dict);
            o.dictionary = zstd::dictionary(bytes.bytes());
        }
        const std::string plain(reinterpret_cast<const char*>(p), n);
        const exact c(zstd::compress(view(p, n), o));
        auto back = zstd::decompress(view(c.p, c.n), o, l);
        check(back && str(*back) == plain);
        std::string out;
        check(theirs(c.p, c.n, dict, out) && out == plain);
        io::buffer sink;
        {
            zstd::writer w(sink, o);
            size_t at = 0;
            while (at < n) {
                size_t k = std::min(n - at, step * 97);
                check(bool(w.write(view(p + at, k))));
                at += k;
                if (flags & 32) {
                    check(bool(w.flush()));
                }
            }
            check(bool(w.close()));
        }
        const exact d(sink.data());
        auto streamed = zstd::decompress(view(d.p, d.n), o, l);
        check(streamed && str(*streamed) == plain);
        check(theirs(d.p, d.n, dict, out) && out == plain);
    }
    return 0;
}
