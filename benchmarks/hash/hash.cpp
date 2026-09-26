//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The hash module: what a checksum of a buffer costs, one algorithm and one
// length a run (benchmarks/go/hash has the Go side, the same cases over the
// same bytes). Prints one line: ns per call and GB/s.
//
//   hash <case> sgcl [length=1024]
//
//   crc32 crc32c crc64 crc64_iso   the CRCs, the path the build has (on arm64
//                                  folding by PMULL, the CRC-32 instructions
//                                  under 128 bytes)
//   crc32-portable crc64-portable  slicing by eight, the path x86 takes today:
//                                  the same build, the other road
//   adler32                        the block loop the compiler vectorizes
//   fnv32 fnv32a fnv64 fnv64a fnv128 fnv128a
//   xxh3_64 xxh3_128               XXH3, seed 0 (the default secret)
//   xxh3_64-seeded                 XXH3 with a seed: past 240 bytes the
//                                  seed's secret is made in each call
//   maphash                        the process's seed (its secret made once)
//   siphash                        SipHash-2-4, a fixed key
//   string-hash                    the core's keyed hash of a string's bytes
//                                  (core/detail/hash_bytes.h), for comparing
//                                  with maphash: not a hasher, one call
//   combine                        crc32::combine over a second piece of
//                                  `length` bytes: one multiplication modulo P
//                                  per set bit of the length
//
// A call is `type::of(data)` over `length` bytes of splitmix64 output, the
// calls independent of each other (the result summed into a sink, the
// pointer read through a volatile so that nothing is hoisted), as Go's
// testing.B loop has them. The loop runs for about two seconds after a
// quarter of a second that is thrown away; the first case in a process
// reads high, so a process runs one case.
#include "benchmarks/common.h"
#include "sgcl/hash/hash.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    volatile uint64_t sink;

    uint64_t fold(uint32_t v) { return v; }
    uint64_t fold(uint64_t v) { return v; }
    uint64_t fold(const sgcl::array<sgcl::byte, 16>& d) { return uint64_t(d[0]) ^ uint64_t(d[15]) << 8; }

    sgcl::array<sgcl::byte, 16> sip_key() {
        sgcl::array<sgcl::byte, 16> k;
        for (size_t i = 0; i < 16; ++i) {
            k[i] = sgcl::byte(i);
        }
        return k;
    }

    std::vector<unsigned char> random_bytes(size_t n) {
        std::vector<unsigned char> b(n + 8);
        uint64_t s = 1;
        for (size_t i = 0; i < n; i += 8) {
            s += 0x9e3779b97f4a7c15ull;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            z ^= z >> 31;
            for (int k = 0; k < 8; ++k) {
                b[i + k] = (unsigned char)(z >> (8 * k));
            }
        }
        return b;
    }

    // Runs f over the data for about `seconds`: calls and the time they took
    template<class F>
    std::pair<uint64_t, double> run_for(F&& f, const unsigned char* data, size_t n, double seconds) {
        uint64_t calls = 0;
        uint64_t acc = 0;
        size_t batch = n >= 65536 ? 16 : n >= 1024 ? 1024 : 16384;
        // Read back through a volatile each call: the compiler cannot know it
        // is the same pointer, so it cannot hoist the hash out of the loop
        const unsigned char* volatile source = data;
        auto t0 = bench::Clock::now();
        double wall = 0;
        do {
            for (size_t i = 0; i < batch; ++i) {
                acc += f(source, n);
            }
            calls += batch;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    template<class H>
    auto one_shot() {
        return [](const unsigned char* p, size_t n) {
            return fold(H::of(sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n)));
        };
    }
}

int main(int argc, char** argv) {
    if (argc < 3 || std::string(argv[2]) != "sgcl") {
        std::fprintf(stderr, "usage: hash <crc32|crc32c|crc64|crc64_iso|crc32-portable|crc64-portable|adler32|fnv32|fnv32a|fnv64|fnv64a|fnv128|fnv128a|xxh3_64|xxh3_128|xxh3_64-seeded|maphash|siphash|string-hash|combine> sgcl [length]\n");
        return 2;
    }
    namespace hash = sgcl::hash;
    std::string what = argv[1];
    size_t n = argc > 3 ? size_t(std::atoll(argv[3])) : 1024;
    auto data = random_bytes(what == "combine" ? 16 : n);
    auto measure = [&](auto f) {
        run_for(f, data.data(), n, 0.25);   // thrown away
        auto [calls, wall] = run_for(f, data.data(), n, 2.0);
        double ns = wall * 1e9 / double(calls);
        double gbs = what == "combine" ? 0 : double(n) * double(calls) / wall / 1e9;
        std::printf("hash %s length=%zu ns/op=%.2f GB/s=%.2f wall=%.2fs\n", what.c_str(), n, ns, gbs, wall);
    };
    if (what == "crc32") {
        measure(one_shot<hash::crc32>());
    } else if (what == "crc32c") {
        measure(one_shot<hash::crc32c>());
    } else if (what == "crc64") {
        measure(one_shot<hash::crc64>());
    } else if (what == "crc64_iso") {
        measure(one_shot<hash::crc64_iso>());
    } else if (what == "crc32-portable") {
        measure([](const unsigned char* p, size_t n) {
            return uint64_t(~hash::detail::crc_update_portable<uint32_t, 0xEDB88320u>(~0u, p, n));
        });
    } else if (what == "crc64-portable") {
        measure([](const unsigned char* p, size_t n) {
            return ~hash::detail::crc_update_portable<uint64_t, 0xC96C5795D7870F42ull>(~0ull, p, n);
        });
    } else if (what == "adler32") {
        measure(one_shot<hash::adler32>());
    } else if (what == "fnv32") {
        measure(one_shot<hash::fnv32>());
    } else if (what == "fnv32a") {
        measure(one_shot<hash::fnv32a>());
    } else if (what == "fnv64") {
        measure(one_shot<hash::fnv64>());
    } else if (what == "fnv64a") {
        measure(one_shot<hash::fnv64a>());
    } else if (what == "fnv128") {
        measure(one_shot<hash::fnv128>());
    } else if (what == "fnv128a") {
        measure(one_shot<hash::fnv128a>());
    } else if (what == "xxh3_64") {
        measure(one_shot<hash::xxh3_64>());
    } else if (what == "xxh3_128") {
        measure(one_shot<hash::xxh3_128>());
    } else if (what == "xxh3_64-seeded") {
        measure([](const unsigned char* p, size_t n) {
            return hash::xxh3_64::of(sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n), 0x9e3779b97f4a7c15ull);
        });
    } else if (what == "maphash") {
        measure(one_shot<hash::maphash>());
    } else if (what == "siphash") {
        static const auto key = sip_key();
        measure([](const unsigned char* p, size_t n) {
            return hash::siphash::of(sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n), key);
        });
    } else if (what == "string-hash") {
        measure([](const unsigned char* p, size_t n) {
            return sgcl::detail::hash_bytes(p, n);
        });
    } else if (what == "combine") {
        measure([](const unsigned char* p, size_t n) {
            uint32_t a;
            std::memcpy(&a, p, 4);
            return uint64_t(hash::crc32::combine(a, a ^ 0x5a5a5a5au, n));
        });
    } else {
        std::fprintf(stderr, "unknown case %s\n", what.c_str());
        return 2;
    }
    return 0;
}
