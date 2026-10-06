//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// bloom_filter, hyperloglog and count_min_sketch: their byte formats read
// from any input (an error has its byte within the input; a sketch read
// writes the input back, byte for byte: the format is canonical), and
// sequences of operations against models: a Bloom filter never says no to
// a key added (a std::set of the keys), a count-min estimate never below
// the true count (a std::map), and a HyperLogLog of the keys split between
// two sketches and merged is the sketch of them all (the registers are a
// maximum, whatever the order). Every sketch after the operations survives
// to_bytes and from_bytes unchanged.
//
// The input: the first byte picks the case (0..2 the formats read as they
// come, 3 the operations), the rest is the bytes, or operations of three
// bytes (what, two of key).
//
// Built with libFuzzer (tests/fuzz/run.sh tests/concurrent/fuzz/sketches_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/concurrent/bloom_filter.h"
#include "sgcl/concurrent/count_min_sketch.h"
#include "sgcl/concurrent/hyperloglog.h"

#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <source_location>

namespace {
    using namespace sgcl;

    void check(bool ok, std::source_location at = std::source_location::current()) {
        if (!ok) {
            std::fprintf(stderr, "sketches_fuzz: check failed at line %u\n", (unsigned)at.line());
            __builtin_trap();
        }
    }

    template<class S>
    void read_back(const slice<const byte>& in) {
        auto r = S::from_bytes(in);
        if (!r) {
            check(r.error().offset() <= in.size());
            return;
        }
        vector<byte> out = r->to_bytes();
        check(out.size() == in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            check(out[i] == in[i]);
        }
    }

    template<class S>
    void round_trip(const S& s) {
        vector<byte> b = s.to_bytes();
        auto r = S::from_bytes(b);
        check(r.has_value());
        check(r->to_bytes() == b);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    slice<const byte> rest(reinterpret_cast<const byte*>(data + 1), size - 1);
    switch (data[0] % 4) {
    case 0:
        read_back<concurrent::bloom_filter>(rest);
        return 0;
    case 1:
        read_back<concurrent::hyperloglog>(rest);
        return 0;
    case 2:
        read_back<concurrent::count_min_sketch>(rest);
        return 0;
    default:
        break;
    }
    unsigned shape = data[0] >> 2;
    auto bloom = concurrent::bloom_filter::with_size(64 + 64 * (shape & 7), 1 + (shape >> 3) % 8);
    auto cms = concurrent::count_min_sketch::with_size(1 + (shape & 15) * 3, 1 + (shape >> 4) % 4);
    unsigned precision = 4 + shape % 8;
    concurrent::hyperloglog all(precision), left(precision), right(precision);
    std::set<uint32_t> keys;
    std::map<uint32_t, uint64_t> counts;
    for (size_t i = 1; i + 2 < size; i += 3) {
        uint32_t key = uint32_t(data[i + 1]) << 8 | data[i + 2];
        switch (data[i] % 6) {
        case 0:
            bloom.add(uint64_t(key));
            keys.insert(key);
            break;
        case 1:
            if (keys.count(key)) {
                check(bloom.contains(uint64_t(key)));
            }
            break;
        case 2: {
            uint64_t n = data[i] >> 3;
            cms.add(uint64_t(key), n);
            counts[key] += n;
            break;
        }
        case 3:
            check(cms.estimate(uint64_t(key)) >= counts[key]);
            break;
        case 4:
            all.add(uint64_t(key));
            (data[i] & 8 ? left : right).add(uint64_t(key));
            break;
        case 5: {
            char text[3] = {char(data[i + 1]), char(data[i + 2]), 0};
            bloom.add(std::string_view(text, 2));
            check(bloom.contains(std::string_view(text, 2)));
            break;
        }
        }
    }
    for (uint32_t k : keys) {
        check(bloom.contains(uint64_t(k)));
    }
    uint64_t total = 0;
    for (auto& [k, n] : counts) {
        check(cms.estimate(uint64_t(k)) >= n);
        total += n;
    }
    check(cms.total() == total);
    concurrent::hyperloglog merged = left.clone();
    merged.merge(right);
    check(merged.to_bytes() == all.to_bytes());
    check(merged.estimate() == all.estimate());
    round_trip(bloom);
    round_trip(cms);
    round_trip(all);
    return 0;
}
