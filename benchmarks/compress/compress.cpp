//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The compress module against the system's zlib and Go (benchmarks/go/compress,
// the same cases over the same bytes): 8 MB of text made of words drawn by
// splitmix64 from a fixed list, the same in both programs. One case and one
// variant a run; prints one line with MB/s of the uncompressed side.
//
//   compress <case> <sgcl|zlib>
//
//   deflate-1 deflate-6 deflate-9   the text compressed, raw DEFLATE, at that level
//   inflate                         level 6's stream decompressed whole in memory
//                                   (into a buffer made by the call, for both)
//   gunzip-stream                   a gzip stream read through the reader, 64 KB a read
//   zip-open                        an archive of 10 000 small entries opened (its
//                                   central directory read); ms per open
//
// Each case runs for about two seconds after a quarter of a second thrown away.
#include "benchmarks/common.h"
#include "sgcl/compress/compress.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
    namespace compress = sgcl::compress;

    uint64_t splitmix(uint64_t& s) {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    std::string text(size_t n) {
        static const char* words[] = {"the", "of", "and", "a", "to", "in", "is", "you", "that", "it", "he", "was", "for", "on", "are", "as",
                                      "with", "his", "they", "I", "at", "be", "this", "have", "from", "or", "one", "had", "by", "word", "but", "not",
                                      "what", "all", "were", "we", "when", "your", "can", "said", "there", "use", "an", "each", "which", "she", "do", "how",
                                      "collector", "pointer", "compress", "archive", "stream", "window", "symbol", "length", "distance", "block", "table", "entry", "header", "checksum", "data", "value"};
        std::string s;
        uint64_t state = 42;
        while (s.size() < n) {
            uint64_t r = splitmix(state);
            s += words[r % 64];
            s += (r >> 8) % 13 == 0 ? ".\n" : " ";
        }
        s.resize(n);
        return s;
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

    std::string z_raw(const std::string& in, int level) {
        z_stream z{};
        deflateInit2(&z, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        std::string out(deflateBound(&z, uLong(in.size())), 0);
        z.next_in = (Bytef*)in.data();
        z.avail_in = uInt(in.size());
        z.next_out = (Bytef*)out.data();
        z.avail_out = uInt(out.size());
        deflate(&z, Z_FINISH);
        out.resize(z.total_out);
        deflateEnd(&z);
        return out;
    }

    sgcl::slice<const std::byte> view(const std::string& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    volatile size_t sink;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: compress <deflate-1|deflate-6|deflate-9|inflate|gunzip-stream|zip-open> <sgcl|zlib>\n");
        return 2;
    }
    std::string what = argv[1], variant = argv[2];
    auto data = text(size_t(8) << 20);
    auto report = [&](double mbs, size_t out_size) {
        std::printf("compress %s %s MB/s=%.1f ratio=%.4f\n", what.c_str(), variant.c_str(), mbs, out_size ? double(out_size) / double(data.size()) : 0.0);
    };
    if (what.rfind("deflate-", 0) == 0) {
        int level = what.back() - '0';
        size_t out_size = 0;
        auto f = [&] {
            if (variant == "sgcl") {
                out_size = compress::flate::compress(view(data), {.level = level}).size();
            } else {
                out_size = z_raw(data, level).size();
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), out_size);
        return 0;
    }
    if (what == "inflate") {
        auto c = z_raw(data, 6);
        auto f = [&] {
            if (variant == "sgcl") {
                sink = compress::flate::decompress(view(c))->size();
            } else {
                // a buffer of the output made each call, as the library's result is
                std::vector<uint8_t> out(data.size());
                z_stream d{};
                inflateInit2(&d, -15);
                d.next_in = (Bytef*)c.data();
                d.avail_in = uInt(c.size());
                d.next_out = (Bytef*)out.data();
                d.avail_out = uInt(out.size());
                inflate(&d, Z_FINISH);
                sink = d.total_out;
                inflateEnd(&d);
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return 0;
    }
    if (what == "gunzip-stream") {
        auto gz = compress::gzip::compress(view(data));
        std::string c(reinterpret_cast<const char*>(gz.data()), gz.size());
        std::vector<std::byte> buf(65536);
        auto f = [&] {
            if (variant == "sgcl") {
                // the compressed bytes read in place, 64 KB a read, as zlib takes them
                size_t at = 0;
                compress::gzip::reader r([&](sgcl::slice<std::byte> b) -> size_t {
                    size_t n = std::min({b.size(), c.size() - at, size_t(65536)});
                    std::memcpy(b.data(), c.data() + at, n);
                    at += n;
                    return n;
                });
                size_t total = 0;
                for (;;) {
                    auto n = r.read(sgcl::slice<std::byte>(buf.data(), buf.size()));
                    if (!n || *n == 0) break;
                    total += *n;
                }
                sink = total;
            } else {
                z_stream d{};
                inflateInit2(&d, 31);
                d.next_in = (Bytef*)c.data();
                d.avail_in = uInt(c.size());
                size_t total = 0;
                int st;
                do {
                    d.next_out = reinterpret_cast<Bytef*>(buf.data());
                    d.avail_out = uInt(buf.size());
                    st = inflate(&d, Z_NO_FLUSH);
                    total += buf.size() - d.avail_out;
                } while (st == Z_OK);
                inflateEnd(&d);
                sink = total;
            }
        };
        mb_per_s(f, data.size(), 0.25);
        report(mb_per_s(f, data.size(), 2.0), c.size());
        return 0;
    }
    if (what == "zip-open") {
        if (variant != "sgcl") {
            std::fprintf(stderr, "zip-open: sgcl only (zlib has no archive)\n");
            return 2;
        }
        sgcl::io::buffer sinkbuf;
        compress::zip::writer w(sinkbuf);
        for (int i = 0; i < 10000; ++i) {
            (void)w.add(sgcl::string("dir/file-" + std::to_string(i) + ".txt"), view(data.substr(size_t(i) * 7, 64)));
        }
        (void)w.close();
        std::string z(reinterpret_cast<const char*>(sinkbuf.data().data()), sinkbuf.size());
        auto t0 = bench::Clock::now();
        uint64_t opens = 0;
        do {
            sink = compress::zip::archive::from(view(z))->entries().size();
            ++opens;
        } while (bench::seconds_since(t0) < 2.0);
        std::printf("compress zip-open sgcl ms/open=%.3f entries=10000\n", bench::seconds_since(t0) * 1000.0 / double(opens));
        return 0;
    }
    std::fprintf(stderr, "unknown case %s\n", what.c_str());
    return 2;
}
