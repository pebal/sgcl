//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../detail/cldr.h"

#include <atomic>
#include <cstdint>

// The registry of the display names (DESIGN 493, 494): each header of
// sgcl/txt/names/ holds one locale's names as constant tables and registers
// them when it is included — an inline variable whose initialization pushes a
// node onto a lock-free list, once a program however many translation units
// include the header. No managed object and no call of the program's: a
// locale whose header is not included is simply not found, and the names fall
// back on the codes.
namespace sgcl::txt::detail::names {
    using cldr::Packed;
    using cldr::Text;

    // One locale's names, what differs from its parent's (whose header this
    // one includes). A row of names is its key's: the currencies' seven (the
    // name, then zero one two few many other), a zone's seven (the exemplar
    // city, then long generic standard daylight, short generic standard
    // daylight), a metazone's six (long, short). The formats: the locale
    // pattern and separator, the region formats (generic, standard,
    // daylight), the fallback format, and the unit patterns of a currency's
    // long name by plural category.
    struct Table {
        uint64_t locale;   // the locale, packed as txt::locale packs it (its default script dropped)
        uint64_t parent;   // its parent's, 0 for root
        Text texts;
        Packed language_keys;          // a language alone: its letters at five bits each, sorted
        Packed language_names;
        uint32_t languages;
        const uint64_t* tagged_keys;   // a language with a script or a region (en_GB): packed, sorted
        Packed tagged_names;
        uint32_t tagged;
        Packed region_keys;
        Packed region_names;
        uint32_t regions;
        Packed script_keys;
        Packed script_names;
        uint32_t scripts;
        Packed alone_keys;            // the scripts with a name of their own when named alone ("Simplified Han")
        Packed alone_names;
        uint32_t alones;
        Packed currency_keys;
        Packed currency_names;
        uint32_t currencies;
        Packed zone_keys;
        Packed zone_names;
        uint32_t zones;
        Packed meta_keys;
        Packed meta_names;
        uint32_t metas;
        const uint16_t* formats;
    };

    enum : uint32_t {
        FmtPattern, FmtSeparator, FmtRegion, FmtRegionStandard, FmtRegionDaylight, FmtFallback, FmtUnit,
    };

    struct Node {
        const Table* table;
        Node* next;
    };

    inline std::atomic<Node*>& head() noexcept {
        static std::atomic<Node*> list{nullptr};
        return list;
    }

    struct Registration {
        Node node;

        explicit Registration(const Table* table) noexcept
        : node{table, nullptr} {
            auto& h = head();
            Node* first = h.load(std::memory_order_relaxed);
            do {
                node.next = first;
            } while (!h.compare_exchange_weak(first, &node, std::memory_order_release, std::memory_order_relaxed));
        }

        Registration(const Registration&) = delete;
        Registration& operator=(const Registration&) = delete;
    };

    // The table of a locale's key, or none
    inline const Table* table_of(uint64_t key) noexcept {
        for (Node* n = head().load(std::memory_order_acquire); n; n = n->next) {
            if (n->table->locale == key) {
                return n->table;
            }
        }
        return nullptr;
    }

    // The index of key in n sorted packed keys, or n
    inline uint32_t find_key(const Packed& keys, uint32_t n, uint32_t key) noexcept {
        uint32_t lo = 0, count = n;
        while (count > 0) {
            uint32_t half = count / 2;
            if (keys[lo + half] < key) {
                lo += half + 1;
                count -= half + 1;
            } else {
                count = half;
            }
        }
        return lo < n && keys[lo] == key ? lo : n;
    }

    inline uint32_t find_key(const uint64_t* keys, uint32_t n, uint64_t key) noexcept {
        size_t i = n ? cldr::lower_bound(keys, n, key) : 0;
        return i < n && keys[i] == key ? uint32_t(i) : n;
    }
}
