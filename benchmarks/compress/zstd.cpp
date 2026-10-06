//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress::zstd against libzstd (when the build found it): one case and one
// variant a run, one line with MB/s of the uncompressed side and the ratio
// of the sizes. Go's standard library has no zstd.
//
//   zstd <case> <sgcl|libzstd> [file]
//
//   decode-<n>       the library's headers one after another (about 8 MB) as
//                    one frame made by libzstd at level n (by the library
//                    without it), with the content's size and checksum,
//                    decompressed whole into a buffer made by the call, for
//                    both
//   decode-binary    the same at level 3 over a program (file, or this
//                    benchmark's own executable)
//   stream-decode    the level-3 frame through zstd::reader over a reader of
//                    the bytes in memory, read 64 KB at a time, against
//                    ZSTD_decompressStream into 64 KB
//   encode-<n>       the text compressed as one frame at level n (1..22),
//                    the content checksum on for both (zstd's command's
//                    default)
//   encode-f<n>      the same at level -n (zstd --fast=n)
//
// Each case runs for about two seconds after a quarter of a second thrown
// away (the slowest levels: one call at least).
#include "benchmarks/common.h"
#include "sgcl/compress/zstd.h"
#include "sgcl/io/io.h"

#if SGCL_BENCH_ZSTD
#include <zstd.h>
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

#if SGCL_BENCH_ZSTD
    std::string theirs_encode(ZSTD_CCtx* c, const std::string& in, int level) {
        ZSTD_CCtx_reset(c, ZSTD_reset_session_and_parameters);
        ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, level);
        ZSTD_CCtx_setParameter(c, ZSTD_c_checksumFlag, 1);
        std::string out(ZSTD_compressBound(in.size()), 0);
        out.resize(ZSTD_compress2(c, out.data(), out.size(), in.data(), in.size()));
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
        std::fprintf(stderr, "usage: zstd <decode-<n>|decode-binary|stream-decode|encode-<n>|encode-f<n>> <sgcl|libzstd> [file]\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
#if !SGCL_BENCH_ZSTD
    if (variant == "libzstd") {
        std::fprintf(stderr, "libzstd was not found by the build (SGCL_REFERENCE_ROOT)\n");
        return 2;
    }
#else
    ZSTD_CCtx* cctx = ZSTD_createCCtx();
#endif
    std::string data = what == "decode-binary" ? slurp(argc > 3 ? argv[3] : argv[0]) : headers();
    auto report = [&](double mbs, size_t out_size) {
        std::printf("zstd %s %s MB/s=%.2f ratio=%.5f size=%zu input=%zu\n", what.c_str(), variant.c_str(), mbs, double(out_size) / double(data.size()), out_size,
                    data.size());
    };
    const bool ours = variant == "sgcl";
    if (what.rfind("decode-", 0) == 0 || what == "stream-decode") {
        const int level = what == "decode-binary" || what == "stream-decode" ? 3 : std::atoi(what.c_str() + 7);
#if SGCL_BENCH_ZSTD
        auto c = theirs_encode(cctx, data, level);
#else
        auto packed = compress::zstd::compress(view(data), {.level = level});
        std::string c(reinterpret_cast<const char*>(packed.data()), packed.size());
#endif
        const bool stream = what == "stream-decode";
        std::vector<std::byte> chunk(size_t(64) << 10);
#if SGCL_BENCH_ZSTD
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
#endif
        auto f = [&] {
            if (ours) {
                if (stream) {
                    compress::zstd::reader r(memory_reader {&c});
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
                    sink = compress::zstd::decompress(view(c))->size();
                }
            } else {
#if SGCL_BENCH_ZSTD
                if (stream) {
                    ZSTD_DCtx_reset(dctx, ZSTD_reset_session_only);
                    ZSTD_inBuffer in {c.data(), c.size(), 0};
                    size_t total = 0;
                    for (;;) {
                        ZSTD_outBuffer o {chunk.data(), chunk.size(), 0};
                        const size_t r = ZSTD_decompressStream(dctx, &o, &in);
                        total += o.pos;
                        if (ZSTD_isError(r) || (r == 0 && in.pos == in.size)) {
                            break;
                        }
                    }
                    sink = total;
                } else {
                    std::vector<char> out(data.size());   // as the library's: made by the call
                    sink = ZSTD_decompressDCtx(dctx, out.data(), out.size(), c.data(), c.size());
                }
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return sink == data.size() ? 0 : 1;
    }
    if (what.rfind("encode-", 0) == 0) {
        const bool fast = what[7] == 'f';
        const int n = std::atoi(what.c_str() + (fast ? 8 : 7));
        const int level = fast ? -n : n;
        size_t out_size = 0;
        auto f = [&] {
            if (ours) {
                out_size = compress::zstd::compress(view(data), {.level = level}).size();
            } else {
#if SGCL_BENCH_ZSTD
                out_size = theirs_encode(cctx, data, level).size();
#endif
            }
        };
        const bool slow = level >= 13;
        mb_per_s(f, data.size(), slow ? 0.0 : 0.25);
        report(mb_per_s(f, data.size(), slow ? 0.0 : 2.0), out_size);
        return 0;
    }
    std::fprintf(stderr, "unknown case %s\n", what.c_str());
    return 2;
}
