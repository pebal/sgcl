//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress::snappy: one case and one variant a run, one line with MB/s of the
// uncompressed side and the ratio of the sizes. No Snappy library is on the
// machine this was written on and Go's standard library has none, so the
// other side is LZ4's fast level, for orientation only: liblz4 where the
// build found it (SGCL_BENCH_LZ4), else the library's own compress::lz4.
//
//   snappy <case> <sgcl|lz4> [file]
//
//   encode-block     the library's headers (about 8 MB) as one block
//                    (lz4: compress_block / LZ4_compress_default)
//   decode-block     that block decompressed into a buffer made by the call
//                    (lz4: decompress_block / LZ4_decompress_safe)
//   encode-framed    the framing format in memory (lz4: a frame of 64 KB
//                    blocks, independent, the content checksum: the closest
//                    LZ4 has to Snappy's chunks with their CRC-32C)
//   decode-framed    the same decompressed
//   decode-binary    decode-block over a program (file, or this benchmark's
//                    own executable)
//
// Each case runs for about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/compress/lz4.h"
#include "sgcl/compress/snappy.h"

#if SGCL_BENCH_LZ4
#include <lz4.h>
#include <lz4frame.h>
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

    std::string text(const sgcl::vector<std::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
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

    volatile size_t sink;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: snappy <encode-block|decode-block|encode-framed|decode-framed|decode-binary> <sgcl|lz4> [file]\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
    std::string data = what == "decode-binary" ? slurp(argc > 3 ? argv[3] : argv[0]) : headers();
    const bool ours = variant == "sgcl";
    const bool framed = what == "encode-framed" || what == "decode-framed";
    const compress::lz4::options frame_options {.block_size = compress::lz4::block_size::kb64};
    auto report = [&](double mbs, size_t out_size) {
        std::printf("snappy %s %s MB/s=%.2f ratio=%.5f size=%zu input=%zu\n", what.c_str(), variant.c_str(), mbs, double(out_size) / double(data.size()), out_size, data.size());
    };
    auto encode = [&]() -> std::string {
        if (ours) {
            return text(framed ? compress::snappy::compress(view(data)) : compress::snappy::compress_block(view(data)));
        }
#if SGCL_BENCH_LZ4
        if (!framed) {
            std::string out(LZ4_compressBound(int(data.size())), 0);
            out.resize(size_t(LZ4_compress_default(data.data(), out.data(), int(data.size()), int(out.size()))));
            return out;
        }
        LZ4F_preferences_t p {};
        p.frameInfo.blockSizeID = LZ4F_max64KB;
        p.frameInfo.blockMode = LZ4F_blockIndependent;
        p.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
        std::string out(LZ4F_compressFrameBound(data.size(), &p), 0);
        out.resize(LZ4F_compressFrame(out.data(), out.size(), data.data(), data.size(), &p));
        return out;
#else
        return text(framed ? compress::lz4::compress(view(data), frame_options) : compress::lz4::compress_block(view(data)));
#endif
    };
    if (what.rfind("encode-", 0) == 0) {
        size_t out_size = 0;
        auto f = [&] {
            out_size = encode().size();
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), out_size);
        return 0;
    }
    const std::string c = encode();
    auto f = [&] {
        if (ours) {
            sink = framed ? compress::snappy::decompress(view(c))->size() : compress::snappy::decompress_block(view(c))->size();
            return;
        }
#if SGCL_BENCH_LZ4
        std::vector<char> out(data.size());
        if (!framed) {
            sink = size_t(LZ4_decompress_safe(c.data(), out.data(), int(c.size()), int(out.size())));
            return;
        }
        LZ4F_dctx* d = nullptr;
        LZ4F_createDecompressionContext(&d, LZ4F_VERSION);
        size_t dst = out.size(), src = c.size();
        LZ4F_decompress(d, out.data(), &dst, c.data(), &src, nullptr);
        LZ4F_freeDecompressionContext(d);
        sink = dst;
#else
        sink = framed ? compress::lz4::decompress(view(c))->size() : compress::lz4::decompress_block(view(c), data.size())->size();
#endif
    };
    mb_per_s(f, data.size(), 0.25);
    report(mb_per_s(f, data.size(), 2.0), c.size());
    return sink == data.size() ? 0 : 1;
}
