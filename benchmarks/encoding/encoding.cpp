//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The byte codecs of the encoding module against Go's (benchmarks/go/encoding,
// the same ops over the same bytes).
//   encoding sgcl [op=b64enc] [size=1024] [count]
//
//   b64enc    base64::standard.encode, a string made each time
//             (Go: StdEncoding.EncodeToString)
//   b64dec    base64::standard.decode, a vector made each time
//             (Go: StdEncoding.DecodeString)
//   b64to     encode_to into a buffer the caller keeps (Go: Encode)
//   b64decto  decode_to into a buffer the caller keeps (Go: Decode)
//   b32enc, b32dec   base32::standard (Go: base32.StdEncoding)
//   hexenc, hexdec   hex::encode, hex::decode (Go: hex.EncodeToString,
//                    hex.DecodeString)
//
// The input is `size` bytes from a generator, the same on both sides; the
// text decoded is its encoding. A run takes about two seconds by default
// (the count is 2 GB over the size). Prints nanoseconds per call and
// megabytes of input per second.
#include "benchmarks/common.h"
#include "sgcl/encoding/encoding.h"

using namespace sgcl::encoding;

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    std::vector<sgcl::byte> input(size_t n) {
        uint64_t state = uint64_t(n) * 0x9E3779B97F4A7C15ull + 1;
        std::vector<sgcl::byte> out(n);
        for (auto& b : out) {
            state += 0x9E3779B97F4A7C15ull;
            uint64_t z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            z ^= z >> 31;
            b = sgcl::byte(uint8_t(z >> 17));
        }
        return out;
    }

    volatile size_t sink = 0;

    // The first case in a process reads about twice high: the road is
    // walked before it is timed
    template<class F>
    double timed(long count, F&& f) {
        for (long i = 0; i < std::min(count, 1000L); ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f();
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }

    double run_sgcl(const char* op, size_t size, long count) {
        using namespace sgcl;
        auto bytes = input(size);
        auto data = slice<const byte>(bytes.data(), bytes.size());
        if (!std::strcmp(op, "b64enc")) {
            return timed(count, [&] { sink += base64::standard.encode(data).size(); });
        }
        if (!std::strcmp(op, "b64dec")) {
            string text = base64::standard.encode(data);
            return timed(count, [&] { sink += base64::standard.decode(text)->size(); });
        }
        if (!std::strcmp(op, "b64to")) {
            std::vector<char> out(base64::standard.encoded_size(size));
            auto room = slice<char>(out.data(), out.size());
            return timed(count, [&] { sink += base64::standard.encode_to(room, data); });
        }
        if (!std::strcmp(op, "b64decto")) {
            string text = base64::standard.encode(data);
            std::vector<byte> out(base64::standard.max_decoded_size(text.size()));
            auto room = slice<byte>(out.data(), out.size());
            return timed(count, [&] { sink += *base64::standard.decode_to(room, text); });
        }
        if (!std::strcmp(op, "b32enc")) {
            return timed(count, [&] { sink += base32::standard.encode(data).size(); });
        }
        if (!std::strcmp(op, "b32dec")) {
            string text = base32::standard.encode(data);
            return timed(count, [&] { sink += base32::standard.decode(text)->size(); });
        }
        if (!std::strcmp(op, "hexenc")) {
            return timed(count, [&] { sink += hex::encode(data).size(); });
        }
        string text = hex::encode(data);
        return timed(count, [&] { sink += hex::decode(text)->size(); });
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    if (!bench::has_variant(variant, {"sgcl"})) {
        std::fprintf(stderr, "usage: encoding sgcl [b64enc|b64dec|b64to|b64decto|b32enc|b32dec|hexenc|hexdec] [size] [count]\n");
        return 2;
    }
    const char* op = argc > 2 ? argv[2] : "b64enc";
    if (!bench::has_variant(op, {"b64enc", "b64dec", "b64to", "b64decto", "b32enc", "b32dec", "hexenc", "hexdec"})) {
        std::fprintf(stderr, "encoding: no op called %s\n", op);
        return 2;
    }
    size_t size = argc > 3 ? size_t(std::atol(argv[3])) : 1024;
    long count = argc > 4 ? std::atol(argv[4]) : long(2'000'000'000 / (size ? size : 1));
    double ns = run_sgcl(op, size, count);
    std::printf("%s op=%s size=%zu count=%ld ns/op=%.1f MB/s=%.0f\n", variant, op, size, count, ns, double(size) / ns * 1e3);
    return 0;
}
