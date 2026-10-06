//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress::lz4 against liblz4 (when the build found it): one case and one
// variant a run, one line with MB/s of the uncompressed side and the ratio
// of the sizes. Go's standard library has no LZ4.
//
//   lz4 <case> <sgcl|liblz4> [file]
//
//   decode-text      the library's headers one after another (about 8 MB)
//                    as one frame (blocks of 4 MB, independent, the content
//                    checksum) made by liblz4 at level 1 (by the library
//                    without it), decompressed whole into a buffer made by
//                    the call, for both
//   decode-binary    the same over a program (file, or this benchmark's own
//                    executable)
//   decode-raw       the text as one raw block: decompress_block against
//                    LZ4_decompress_safe
//   stream-decode    the frame through lz4::reader over a reader of the bytes
//                    in memory, read 64 KB at a time, against LZ4F_decompress
//                    into 64 KB
//   encode-<n>       the text compressed as a frame at level n (1..12)
//   encode-f<n>      the same with acceleration n (lz4 --fast=n)
//
// Each case runs for about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/compress/lz4.h"
#include "sgcl/io/io.h"

#if SGCL_BENCH_LZ4
#include <lz4.h>
#include <lz4frame.h>
#include <lz4hc.h>
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
        return all;
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

#if SGCL_BENCH_LZ4
    std::string lz4f_encode(const std::string& in, int level) {
        LZ4F_preferences_t p {};
        p.frameInfo.blockSizeID = LZ4F_max4MB;
        p.frameInfo.blockMode = LZ4F_blockIndependent;
        p.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
        p.frameInfo.contentSize = in.size();
        p.compressionLevel = level;
        std::string out(LZ4F_compressFrameBound(in.size(), &p), 0);
        out.resize(LZ4F_compressFrame(out.data(), out.size(), in.data(), in.size(), &p));
        return out;
    }

    // into a buffer of the content's size, made by the call
    size_t lz4f_decode(const std::string& in, size_t size, size_t chunk) {
        std::vector<char> out(chunk ? chunk : size);
        LZ4F_dctx* d = nullptr;
        LZ4F_createDecompressionContext(&d, LZ4F_VERSION);
        size_t at = 0, total = 0;
        for (;;) {
            size_t dst = out.size() - (chunk ? 0 : total), src = in.size() - at;
            size_t r = LZ4F_decompress(d, out.data() + (chunk ? 0 : total), &dst, in.data() + at, &src, nullptr);
            at += src;
            total += dst;
            if (r == 0 || LZ4F_isError(r) || (src == 0 && dst == 0)) {
                break;
            }
        }
        LZ4F_freeDecompressionContext(d);
        return total;
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
        std::fprintf(stderr, "usage: lz4 <decode-text|decode-binary|decode-raw|stream-decode|encode-<1..12>|encode-f<n>> <sgcl|liblz4> [file]\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
#if !SGCL_BENCH_LZ4
    if (variant == "liblz4") {
        std::fprintf(stderr, "liblz4 was not found by the build (SGCL_REFERENCE_ROOT)\n");
        return 2;
    }
#endif
    std::string data = what == "decode-binary" ? slurp(argc > 3 ? argv[3] : argv[0]) : headers();
    auto report = [&](double mbs, size_t out_size) {
        std::printf("lz4 %s %s MB/s=%.2f ratio=%.5f size=%zu input=%zu\n", what.c_str(), variant.c_str(), mbs, double(out_size) / double(data.size()), out_size, data.size());
    };
    const bool ours = variant == "sgcl";
    if (what == "decode-text" || what == "decode-binary" || what == "stream-decode") {
#if SGCL_BENCH_LZ4
        auto c = lz4f_encode(data, 1);
#else
        auto packed = compress::lz4::compress(view(data));
        std::string c(reinterpret_cast<const char*>(packed.data()), packed.size());
#endif
        const bool stream = what == "stream-decode";
        std::vector<std::byte> chunk(size_t(64) << 10);
        auto f = [&] {
            if (ours) {
                if (stream) {
                    compress::lz4::reader r(memory_reader {&c});
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
                    sink = compress::lz4::decompress(view(c))->size();
                }
            } else {
#if SGCL_BENCH_LZ4
                sink = lz4f_decode(c, data.size(), stream ? chunk.size() : 0);
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return sink == data.size() ? 0 : 1;
    }
    if (what == "decode-raw") {
        auto packed = compress::lz4::compress_block(view(data));
        std::string c(reinterpret_cast<const char*>(packed.data()), packed.size());
        auto f = [&] {
            if (ours) {
                sink = compress::lz4::decompress_block(view(c), data.size())->size();
            } else {
#if SGCL_BENCH_LZ4
                std::vector<char> out(data.size());
                sink = size_t(LZ4_decompress_safe(c.data(), out.data(), int(c.size()), int(out.size())));
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return sink == data.size() ? 0 : 1;
    }
    if (what.rfind("encode-", 0) == 0) {
        const bool accel = what[7] == 'f';
        const int n = std::atoi(what.c_str() + (accel ? 8 : 7));
        const int level = accel ? -n : n;
        size_t out_size = 0;
        auto f = [&] {
            if (ours) {
                out_size = compress::lz4::compress(view(data), {.level = level}).size();
            } else {
#if SGCL_BENCH_LZ4
                out_size = lz4f_encode(data, level).size();
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), out_size);
        return 0;
    }
    std::fprintf(stderr, "unknown case %s\n", what.c_str());
    return 2;
}
