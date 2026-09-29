//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress::lzma and compress::xz against liblzma (xz's, when the build
// found it), and the branch converters: one case and one variant a run,
// one line with MB/s of the uncompressed side.
//
//   lzma <case> <sgcl|liblzma> [file]
//
//   decode-text      the library's headers one after another (about 8 MB),
//                    compressed as .lzma by liblzma at preset 6 (by the
//                    library without it), decompressed whole in memory into
//                    a buffer made by the call, for both
//   decode-binary    the same over a program (file, or this benchmark's own
//                    executable)
//   decode-random    4 MB of random bytes: literals only
//   encode-<n>[e]    the text compressed as .lzma at level n (-e: extreme):
//                    MB/s and the ratio of the sizes
//   xz-decode-text, xz-decode-binary, xz-decode-random, xz-encode-<n>[e]
//                    the same as .xz (CRC-64): lzma_stream_decoder and
//                    lzma_easy_encoder on liblzma's side
//   bcj-<filter>-encode, bcj-<filter>-decode
//                    a converter (x86, arm64, arm, armt, powerpc, sparc,
//                    ia64, riscv, delta) over the program in place; sgcl only
//
// Each case runs for about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/compress/lzma.h"
#include "sgcl/compress/xz.h"

#if SGCL_BENCH_LIBLZMA
#include <lzma.h>
#endif

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
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

#if SGCL_BENCH_LIBLZMA
    std::string xz_encode(const std::string& in, uint32_t preset, bool container) {
        lzma_stream s = LZMA_STREAM_INIT;
        if (container) {
            (void)lzma_easy_encoder(&s, preset, LZMA_CHECK_CRC64);
        } else {
            lzma_options_lzma o;
            lzma_lzma_preset(&o, preset);
            (void)lzma_alone_encoder(&s, &o);
        }
        std::string out(in.size() + in.size() / 2 + 4096, 0);
        s.next_in = reinterpret_cast<const uint8_t*>(in.data());
        s.avail_in = in.size();
        s.next_out = reinterpret_cast<uint8_t*>(out.data());
        s.avail_out = out.size();
        while (lzma_code(&s, LZMA_FINISH) == LZMA_OK) {
        }
        out.resize(s.total_out);
        lzma_end(&s);
        return out;
    }

    size_t xz_decode(const std::string& in, size_t size, bool container) {
        std::vector<uint8_t> out(size);
        lzma_stream s = LZMA_STREAM_INIT;
        if (container) {
            (void)lzma_stream_decoder(&s, UINT64_MAX, 0);
        } else {
            (void)lzma_alone_decoder(&s, UINT64_MAX);
        }
        s.next_in = reinterpret_cast<const uint8_t*>(in.data());
        s.avail_in = in.size();
        s.next_out = out.data();
        s.avail_out = out.size();
        (void)lzma_code(&s, LZMA_FINISH);
        size_t n = s.total_out;
        lzma_end(&s);
        return n;
    }
#endif

    volatile size_t sink;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: lzma <[xz-]decode-text|[xz-]decode-binary|[xz-]decode-random|[xz-]encode-<0..9>[e]|bcj-<filter>-<encode|decode>> <sgcl|liblzma> [file]\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
#if !SGCL_BENCH_LIBLZMA
    if (variant == "liblzma") {
        std::fprintf(stderr, "liblzma was not found by the build (SGCL_XZ_ROOT)\n");
        return 2;
    }
#endif
    bool container = what.rfind("xz-", 0) == 0;
    std::string kind = container ? what.substr(3) : what;
    std::string data;
    if (kind == "decode-random") {
        std::mt19937_64 rng(1);
        data.resize(size_t(4) << 20);
        for (size_t i = 0; i < data.size(); i += 8) {
            uint64_t v = rng();
            std::memcpy(data.data() + i, &v, 8);
        }
    } else if (kind == "decode-binary" || kind.rfind("bcj-", 0) == 0) {
        data = slurp(argc > 3 ? argv[3] : argv[0]);
    } else {
        data = headers();
    }
    auto report = [&](double mbs, size_t out_size) {
        std::printf("lzma %s %s MB/s=%.2f ratio=%.5f size=%zu input=%zu\n", what.c_str(), variant.c_str(), mbs, double(out_size) / double(data.size()), out_size, data.size());
    };
    if (kind.rfind("decode-", 0) == 0) {
#if SGCL_BENCH_LIBLZMA
        auto c = xz_encode(data, 6, container);
#else
        auto packed = container ? compress::xz::compress(view(data)) : compress::lzma::compress(view(data));
        std::string c(reinterpret_cast<const char*>(packed.data()), packed.size());
#endif
        auto f = [&] {
            if (variant == "sgcl") {
                sink = container ? compress::xz::decompress(view(c))->size() : compress::lzma::decompress(view(c))->size();
            } else {
#if SGCL_BENCH_LIBLZMA
                sink = xz_decode(c, data.size(), container);
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return sink == data.size() ? 0 : 1;
    }
    if (kind.rfind("encode-", 0) == 0) {
        int level = kind[7] - '0';
        bool extreme = kind.size() > 8 && kind[8] == 'e';
        size_t out_size = 0;
        auto f = [&] {
            if (variant == "sgcl") {
                out_size = container ? compress::xz::compress(view(data), {.level = level, .extreme = extreme}).size()
                                     : compress::lzma::compress(view(data), {.level = level, .extreme = extreme}).size();
            } else {
#if SGCL_BENCH_LIBLZMA
                out_size = xz_encode(data, uint32_t(level) | (extreme ? LZMA_PRESET_EXTREME : 0), container).size();
#endif
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), out_size);
        return 0;
    }
    if (kind.rfind("bcj-", 0) == 0) {
        using sgcl::compress::detail::SimpleKind;
        std::string name = kind.substr(4, kind.rfind('-') - 4);
        bool encoder = kind.substr(kind.rfind('-') + 1) == "encode";
        static const std::pair<const char*, SimpleKind> names[] = {{"x86", SimpleKind::x86}, {"arm64", SimpleKind::arm64}, {"arm", SimpleKind::arm}, {"armt", SimpleKind::armt},
                                                                   {"powerpc", SimpleKind::powerpc}, {"sparc", SimpleKind::sparc}, {"ia64", SimpleKind::ia64}, {"riscv", SimpleKind::riscv}, {"delta", SimpleKind::delta}};
        SimpleKind k = SimpleKind::x86;
        for (auto& [n, v] : names) {
            if (name == n) {
                k = v;
            }
        }
        std::vector<uint8_t> buf(data.begin(), data.end());
        auto f = [&] {
            sgcl::compress::detail::SimpleFilter filter;
            filter.init(k, encoder, 0, 4);
            sink = filter.run(buf.data(), buf.size());
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), data.size());
        return 0;
    }
    std::fprintf(stderr, "unknown case %s\n", what.c_str());
    return 2;
}
