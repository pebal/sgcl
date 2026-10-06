//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress::brotli against libbrotli (when the build found it): one case and
// one variant a run, one line with MB/s of the uncompressed side and the
// ratio of the sizes. Go's standard library has no brotli.
//
//   brotli <case> <sgcl|libbrotli> [file]
//
//   decode-<q>       the library's headers one after another (about 8 MB) as
//                    one stream made by libbrotli at quality q (by the
//                    library without it), window 22, decompressed whole into
//                    a buffer made by the call, for both
//   decode-binary    the same at quality 5 over a program (file, or this
//                    benchmark's own executable)
//   stream-decode    the quality-5 stream through brotli::reader over a
//                    reader of the bytes in memory, read 64 KB at a time,
//                    against BrotliDecoderDecompressStream into 64 KB
//   encode-<q>       the text compressed as one stream at quality q (0..11),
//                    window 22 for both
//
// Each case runs for about two seconds after a quarter of a second thrown
// away (qualities 10 and 11: one call at least).
#include "benchmarks/common.h"
#include "sgcl/compress/brotli.h"
#include "sgcl/io/io.h"

#if SGCL_BENCH_BROTLI
#include <brotli/decode.h>
#include <brotli/encode.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
    namespace compress = sgcl::compress;

    std::string slurp(const std::filesystem::path& f) {
        std::ifstream is(f, std::ios::binary);
        std::stringstream ss;
        ss << is.rdbuf();
        return ss.str();
    }

    std::string headers() {
        auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() / "sgcl";
        std::vector<std::filesystem::path> files;
        for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
            if (e.path().extension() == ".h") {
                files.push_back(e.path());
            }
        }
        std::sort(files.begin(), files.end());
        std::string all;
        for (auto& f : files) {
            all += slurp(f);
        }
        return all.substr(0, size_t(8) << 20);
    }

    sgcl::slice<const std::byte> view(const std::string& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    template<class F>
    double mb_per_s(F&& f, size_t bytes, double seconds) {
        auto t0 = bench::Clock::now();
        uint64_t calls = 0;
        do {
            f();
            ++calls;
        } while (bench::seconds_since(t0) < seconds);
        return double(bytes) * double(calls) / bench::seconds_since(t0) / 1e6;
    }

#if SGCL_BENCH_BROTLI
    std::string theirs_encode(const std::string& in, int q) {
        std::string out(BrotliEncoderMaxCompressedSize(in.size()) + 64, 0);
        size_t n = out.size();
        BrotliEncoderCompress(q, 22, BROTLI_MODE_GENERIC, in.size(), reinterpret_cast<const uint8_t*>(in.data()), &n,
                              reinterpret_cast<uint8_t*>(out.data()));
        out.resize(n);
        return out;
    }
#endif

    // A reader over bytes in memory, read from the start again for each run
    struct memory_reader {
        const std::string* data;
        size_t at = 0;

        sgcl::expected<size_t, sgcl::io::error> read(sgcl::slice<std::byte> b) {
            const size_t n = std::min(b.size(), data->size() - at);
            sgcl::detail::copy_bytes(b.data(), data->data() + at, n);
            at += n;
            return n;
        }
    };

    volatile size_t sink;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: brotli <decode-<q>|decode-binary|stream-decode|encode-<q>> <sgcl|libbrotli> [file]\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
#if !SGCL_BENCH_BROTLI
    if (variant == "libbrotli") {
        std::fprintf(stderr, "libbrotli was not found by the build (SGCL_REFERENCE_ROOT)\n");
        return 2;
    }
#endif
    std::string data = what == "decode-binary" ? slurp(argc > 3 ? argv[3] : argv[0]) : headers();
    auto report = [&](double mbs, size_t out_size) {
        std::printf("brotli %s %s MB/s=%.2f ratio=%.5f size=%zu input=%zu\n", what.c_str(), variant.c_str(), mbs, double(out_size) / double(data.size()), out_size,
                    data.size());
    };
    const bool ours = variant == "sgcl";
    if (what.rfind("decode-", 0) == 0 || what == "stream-decode") {
        const int q = what == "decode-binary" || what == "stream-decode" ? 5 : std::atoi(what.c_str() + 7);
#if SGCL_BENCH_BROTLI
        auto c = theirs_encode(data, q);
#else
        auto packed = compress::brotli::compress(view(data), {.level = q});
        std::string c(reinterpret_cast<const char*>(packed.data()), packed.size());
#endif
        const bool stream = what == "stream-decode";
        std::vector<std::byte> chunk(size_t(64) << 10);
        auto f = [&] {
            if (ours) {
                if (stream) {
                    compress::brotli::reader r(memory_reader {&c});
                    size_t total = 0;
                    for (;;) {
                        auto n = r.read(sgcl::slice<std::byte>(chunk.data(), chunk.size()));
                        if (!n || *n == 0) {
                            break;
                        }
                        total += *n;
                    }
                    sink = total;
                } else {
                    sink = compress::brotli::decompress(view(c))->size();
                }
            } else {
#if SGCL_BENCH_BROTLI
                if (stream) {
                    BrotliDecoderState* s = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
                    const uint8_t* in = reinterpret_cast<const uint8_t*>(c.data());
                    size_t in_left = c.size();
                    size_t total = 0;
                    BrotliDecoderResult r;
                    do {
                        uint8_t* o = reinterpret_cast<uint8_t*>(chunk.data());
                        size_t o_left = chunk.size();
                        r = BrotliDecoderDecompressStream(s, &in_left, &in, &o_left, &o, nullptr);
                        total += chunk.size() - o_left;
                    } while (r == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT);
                    BrotliDecoderDestroyInstance(s);
                    sink = total;
                } else {
                    std::vector<char> out(data.size());   // as the library's: made by the call
                    size_t n = out.size();
                    BrotliDecoderDecompress(c.size(), reinterpret_cast<const uint8_t*>(c.data()), &n, reinterpret_cast<uint8_t*>(out.data()));
                    sink = n;
                }
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return sink == data.size() ? 0 : 1;
    }
    if (what.rfind("encode-", 0) == 0) {
        const int q = std::atoi(what.c_str() + 7);
        size_t out_size = 0;
        auto f = [&] {
            if (ours) {
                out_size = compress::brotli::compress(view(data), {.level = q}).size();
            } else {
#if SGCL_BENCH_BROTLI
                out_size = theirs_encode(data, q).size();
#endif
            }
        };
        const bool slow = q >= 10;
        mb_per_s(f, data.size(), slow ? 0.0 : 0.25);
        report(mb_per_s(f, data.size(), slow ? 0.0 : 2.0), out_size);
        return 0;
    }
    std::fprintf(stderr, "unknown case %s\n", what.c_str());
    return 2;
}
