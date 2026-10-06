//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// brotli on any bytes, two ways:
//   - the bytes as a stream: decompress in memory and the reader fed in
//     pieces of every size the last byte picks must agree (the reader stops
//     at the stream's end and leaves what follows); and against libbrotli:
//     where libbrotli decodes a stream to its end with no bytes after it, the
//     library decodes it to the same bytes, and the other way round;
//   - the bytes as data: compressed at a quality and window the first bytes
//     pick, by compress and by the writer in pieces (flushed or not),
//     decoded by the library and by libbrotli to the same.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/brotli/include -L/opt/homebrew/opt/brotli/lib -lbrotlienc -lbrotlidec -lbrotlicommon" \
//       tests/fuzz/run.sh tests/compress/fuzz/brotli_fuzz.cpp 300
#include "sgcl/compress/compress.h"

#include <brotli/decode.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    using brotli = compress::brotli;

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

    // The bytes copied into malloc memory of exactly their size: a read
    // past their end is ASan's to see, which a managed buffer or a
    // vector's spare capacity hides
    struct exact {
        uint8_t* p;
        size_t n;

        exact(const void* src, size_t size)
        : p(static_cast<uint8_t*>(std::malloc(size ? size : 1))), n(size) {
            if (size) {
                std::memcpy(p, src, size);
            }
        }

        exact(const exact&) = delete;
        exact& operator=(const exact&) = delete;

        ~exact() {
            std::free(p);
        }

        sgcl::slice<const std::byte> bytes() const {
            return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(p), n);
        }
    };

    std::string str(const vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    slice<const std::byte> view(const uint8_t* p, size_t n) {
        return slice<const std::byte>(reinterpret_cast<const std::byte*>(p), n);
    }

    // libbrotli's reading: true when the stream ends with the input, up to Cap
    bool theirs(const uint8_t* data, size_t size, std::string& out) {
        BrotliDecoderState* s = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
        out.clear();
        std::string buf(1 << 16, '\0');
        const uint8_t* in = data;
        size_t in_left = size;
        BrotliDecoderResult r;
        do {
            uint8_t* o = reinterpret_cast<uint8_t*>(buf.data());
            size_t o_left = buf.size();
            r = BrotliDecoderDecompressStream(s, &in_left, &in, &o_left, &o, nullptr);
            out.append(buf.data(), buf.size() - o_left);
        } while (r == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT && out.size() <= Cap);
        BrotliDecoderDestroyInstance(s);
        return r == BROTLI_DECODER_RESULT_SUCCESS && in_left == 0 && out.size() <= Cap;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    const size_t step = 1 + data[size - 1] % 13;
    compress::limits l {Cap, uint64_t(1) << 25};

    // as a stream
    auto whole = brotli::decompress(view(data, size), l);
    {
        brotli::reader r(pieces {data, size, step}, l);
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
        }
    }
    std::string ref;
    const bool ok = theirs(data, size, ref);
    if (ok) {
        check(whole && str(*whole) == ref);
    } else if (whole) {
        check(false);   // the library read a stream libbrotli refuses
    }

    // as data
    if (size >= 3) {
        const int q = data[0] % 12;
        const uint8_t flags = data[1];
        const uint8_t* p = data + 2;
        const size_t n = size - 2;
        brotli::options o {.level = q, .window_log = uint8_t(10 + flags % 15)};
        const std::string plain(reinterpret_cast<const char*>(p), n);
        auto c = brotli::compress(view(p, n), o);
        auto back = brotli::decompress(exact(c.data(), c.size()).bytes(), l);
        check(back && str(*back) == plain);
        std::string out;
        check(theirs(reinterpret_cast<const uint8_t*>(c.data()), c.size(), out) && out == plain);
        io::buffer sink;
        {
            brotli::writer w(sink, o);
            size_t at = 0;
            while (at < n) {
                size_t k = std::min(n - at, step * 97);
                check(bool(w.write(view(p + at, k))));
                at += k;
                if (flags & 0x80) {
                    check(bool(w.flush()));
                }
            }
            check(bool(w.close()));
        }
        auto d = sink.data();
        auto streamed = brotli::decompress(exact(d.data(), d.size()).bytes(), l);
        check(streamed && str(*streamed) == plain);
        check(theirs(reinterpret_cast<const uint8_t*>(d.data()), d.size(), out) && out == plain);
    }
    return 0;
}
