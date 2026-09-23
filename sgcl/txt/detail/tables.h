//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>

// The shape of the generated tables and the search over them.
//
// A question asked about one character in the middle of a loop — its
// category, where a word may break, the weight of a letter — is answered
// below U+10000 by a two-stage table: an index that says which block of
// values a code point falls in, the blocks themselves, and every block
// that is the same as another stored once. Two reads and no branch. It
// replaced a binary search over sorted ranges, which was a dozen steps
// that no processor can predict: 21.8 ns against 0.54 over Latin text.
//
// Above U+10000 the ranges stay. An index over a million code points
// would cost more than the search it saves, and hardly any text goes
// there; the tables are split at the end of the Basic Multilingual Plane
// so that neither half ever has to look at the other, and a bound needs
// sixteen bits below it rather than thirty-two.
//
// The generator picks the block size and the width of the index and of a
// value per table, whichever comes out smallest — three of the ten came
// out smaller than the ranges they replaced, and the whole change cost
// 50.1 KB against a module of 559.
//
// What is read once rather than per character keeps its ranges: the
// decimal digits, the decompositions, the case mappings, the encodings.

namespace sgcl::txt::detail {
    struct Range16 {
        uint16_t lo;
        uint16_t hi;
    };

    struct Range32 {
        char32_t lo;
        char32_t hi;
    };

    struct ValueRange16 {
        uint16_t lo;
        uint16_t hi;
        uint16_t value;
    };

    struct ValueRange32 {
        char32_t lo;
        char32_t hi;
        uint16_t value;
    };

    // A set of code points, in the same two stages as a table of
    // values: below U+10000 a bit to the code point, the blocks that are
    // alike stored once, and above it the ranges. A set is asked about
    // one character at a time in the middle of a loop — whether it is an
    // emoji, whether it is cased, whether a contraction can begin with
    // it — and there the search was costing more than the question.
    template<class Index, unsigned Shift>
    struct Set {
        const Index* index;
        const uint64_t* blocks;
        const Range32* high;
        size_t high_size;
    };

    // The tables whose value is asked for one code point at a time keep
    // the Basic Multilingual Plane as a two-stage table instead: an index
    // that says which block of values a code point falls in, the blocks
    // themselves, and every block that is the same as another stored
    // once. It answers in two reads and no branch, where the search it
    // replaces was a dozen steps that no processor can predict — 21.8 ns
    // against 0.54 measured over Latin text, forty times — and it costs
    // 50.1 KB over the whole module, three of the ten tables coming out
    // smaller than the ranges they replace. The generator picks the block
    // size per table, whichever comes out smallest, and the width of the
    // index and of a value with it.
    //
    // Above U+10000 the ranges stay: an index over a million code points
    // would cost more than the search it saves, and hardly any text goes
    // there. Runs says the value counts from the first code point of the
    // range rather than standing for all of it, which is how the
    // collation weights are written.
    template<class Value, class Index, unsigned Shift, bool Runs = false>
    struct Table {
        const Index* index;
        const Value* blocks;
        const ValueRange32* high;
        size_t high_size;
    };

    // The tables that are read once rather than per character stay as
    // they were: a range costs six bytes where a block costs a hundred
    struct RangeTable {
        const ValueRange16* bmp;
        size_t bmp_size;
        const ValueRange32* high;
        size_t high_size;
    };

    // The range that holds c, for the tables whose value counts from the
    // first code point of the range (the decimal digits)
    struct Found {
        char32_t lo = 0;
        uint16_t value = 0;
        bool ok = false;
    };

    template<class R>
    constexpr const R* find_range(char32_t c, const R* table, size_t n) noexcept {
        size_t lo = 0, hi = n;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (c < table[mid].lo) {
                hi = mid;
            } else if (c > table[mid].hi) {
                lo = mid + 1;
            } else {
                return &table[mid];
            }
        }
        return nullptr;
    }

    template<class Index, unsigned Shift>
    constexpr bool in_set(char32_t c, const Set<Index, Shift>& s) noexcept {
        if (c < 0x10000) {
            size_t block = size_t(s.index[c >> Shift]) << (Shift - 6);
            return (s.blocks[block + ((c >> 6) & ((1u << (Shift - 6)) - 1))] >> (c & 63)) & 1;
        }
        return find_range(c, s.high, s.high_size) != nullptr;
    }

    template<class Value, class Index, unsigned Shift, bool Runs>
    constexpr uint16_t value_of(char32_t c, const Table<Value, Index, Shift, Runs>& t) noexcept {
        if (c < 0x10000) {
            return t.blocks[(size_t(t.index[c >> Shift]) << Shift) | (c & ((1u << Shift) - 1))];
        }
        auto r = find_range(c, t.high, t.high_size);
        if (!r) {
            return 0;
        }
        return Runs ? uint16_t(r->value + (c - r->lo)) : r->value;
    }

    constexpr Found find(char32_t c, const RangeTable& table) noexcept {
        if (c < 0x10000) {
            auto r = find_range(c, table.bmp, table.bmp_size);
            return r ? Found{r->lo, r->value, true} : Found{};
        }
        auto r = find_range(c, table.high, table.high_size);
        return r ? Found{r->lo, r->value, true} : Found{};
    }

    constexpr uint16_t value_of(char32_t c, const RangeTable& table) noexcept {
        return find(c, table).value;
    }

    // The decomposition of a code point (UAX #15): where it begins in the
    // pool and how many UTF-16 units it is. The pool is UTF-16 rather
    // than UTF-32 because all but a handful of the code points in it are
    // in the Basic Multilingual Plane, and the few that are not cost a
    // surrogate pair — half the bytes for the price of a decode the walk
    // was doing anyway.
    struct Decomp16 {
        uint16_t cp;
        uint16_t at;
        uint8_t size;
    };

    struct Decomp32 {
        char32_t cp;
        uint16_t at;
        uint8_t size;
    };

    struct DecompTable {
        const Decomp16* bmp;
        size_t bmp_size;
        const Decomp32* high;
        size_t high_size;
        const char16_t* pool;
    };

    struct Decomposition {
        const char16_t* units = nullptr;
        size_t size = 0;

        constexpr explicit operator bool() const noexcept {
            return units != nullptr;
        }
    };

    template<class R>
    constexpr const R* find_point(char32_t c, const R* table, size_t n) noexcept {
        size_t lo = 0, hi = n;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (c < table[mid].cp) {
                hi = mid;
            } else if (c > table[mid].cp) {
                lo = mid + 1;
            } else {
                return &table[mid];
            }
        }
        return nullptr;
    }

    constexpr Decomposition decomposition_of(char32_t c, const DecompTable& table) noexcept {
        if (c < 0x10000) {
            auto r = find_point(c, table.bmp, table.bmp_size);
            return r ? Decomposition{table.pool + r->at, r->size} : Decomposition{};
        }
        auto r = find_point(c, table.high, table.high_size);
        return r ? Decomposition{table.pool + r->at, r->size} : Decomposition{};
    }

    // A primary composite and the pair that makes it, searched by the
    // pair; the table is sorted by the first code point and then by the
    // second
    struct Composed16 {
        uint16_t a;
        uint16_t b;
        uint16_t composite;
    };

    struct Composed32 {
        char32_t a;
        char32_t b;
        char32_t composite;
    };

    struct CompositionTable {
        const Composed16* bmp;
        size_t bmp_size;
        const Composed32* high;
        size_t high_size;
    };

    template<class R>
    constexpr char32_t find_pair(char32_t a, char32_t b, const R* table, size_t n) noexcept {
        size_t lo = 0, hi = n;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            const auto& r = table[mid];
            if (a < r.a || (a == r.a && b < r.b)) {
                hi = mid;
            } else if (a > r.a || (a == r.a && b > r.b)) {
                lo = mid + 1;
            } else {
                return r.composite;
            }
        }
        return 0;
    }

    // The code point a and b compose to, or zero when they do not
    constexpr char32_t composed(char32_t a, char32_t b, const CompositionTable& table) noexcept {
        return a < 0x10000 ? find_pair(a, b, table.bmp, table.bmp_size)
                           : find_pair(a, b, table.high, table.high_size);
    }
}
