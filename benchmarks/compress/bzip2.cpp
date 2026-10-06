//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// compress::bzip2's compressor and decoder against the system's libbz2: one
// case and one variant a run, one line with MB/s of the uncompressed side and
// the ratio of the sizes. Go's standard library reads bzip2 and has no
// compressor.
//
//   bzip2 <case> <sgcl|libbz2> [file]
//
//   encode-<n>       the library's headers one after another (about 8 MB, or
//                    the file) compressed as one stream at level n (1..9),
//                    BZ2_bzBuffToBuffCompress with libbz2's default work
//                    factor beside it
//   decode-<n>       that stream made by libbz2 decompressed whole into a
//                    buffer made by the call, for both
//
// Each case runs for about two seconds after a quarter of a second thrown
// away.
#include "benchmarks/common.h"
#include "sgcl/compress/bzip2.h"
#include "sgcl/io/io.h"

#include <bzlib.h>

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

    std::string theirs_encode(const std::string& in, int level) {
        unsigned size = unsigned(in.size() + in.size() / 100 + 600);
        std::string out(size, 0);
        BZ2_bzBuffToBuffCompress(out.data(), &size, const_cast<char*>(in.data()), unsigned(in.size()), level, 0, 0);
        out.resize(size);
        return out;
    }

    volatile size_t sink;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: bzip2 <encode-<n>|decode-<n>> <sgcl|libbz2> [file]\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
    std::string data = argc > 3 ? slurp(argv[3]) : headers();
    auto report = [&](double mbs, size_t out_size) {
        std::printf("bzip2 %s %s MB/s=%.2f ratio=%.5f size=%zu input=%zu\n", what.c_str(), variant.c_str(), mbs, double(out_size) / double(data.size()), out_size,
                    data.size());
    };
    const bool ours = variant == "sgcl";
    if (what.rfind("decode-", 0) == 0) {
        const int level = std::atoi(what.c_str() + 7);
        auto c = theirs_encode(data, level);
        auto f = [&] {
            if (ours) {
                sink = compress::bzip2::decompress(view(c))->size();
            } else {
                std::vector<char> out(data.size());   // as the library's: made by the call
                unsigned n = unsigned(out.size());
                BZ2_bzBuffToBuffDecompress(out.data(), &n, c.data(), unsigned(c.size()), 0, 0);
                sink = n;
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return sink == data.size() ? 0 : 1;
    }
    if (what.rfind("encode-", 0) == 0) {
        const int level = std::atoi(what.c_str() + 7);
        size_t out_size = 0;
        auto f = [&] {
            if (ours) {
                out_size = compress::bzip2::compress(view(data), {.level = level}).size();
            } else {
                out_size = theirs_encode(data, level).size();
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), out_size);
        return 0;
    }
    std::fprintf(stderr, "unknown case %s\n", what.c_str());
    return 2;
}
