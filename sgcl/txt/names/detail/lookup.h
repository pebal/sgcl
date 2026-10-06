//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "registry.h"

#include <atomic>

#include <cstdint>
#include <cstdio>
#include <string_view>

// How a name is found among the registered tables: the locale's own table
// (its key normalized as the data's locales are), then the tables of its CLDR
// parents, each holding what differs from the next. A table that is not
// registered — its header not included — is skipped; when no table of the
// chain is, the names fall back on the codes, and a Debug build says once a
// locale which header would give them.
namespace sgcl::txt::detail::names {
    // The tables of a locale's chain, the most specific first: at most eight
    struct Chain {
        const Table* tables[8];
        uint32_t n = 0;

        SGCL_INLINE_HOT bool empty() const noexcept {
            return n == 0;
        }
    };

    // the chain from a table by its parents (each header includes its
    // parent's, so a registered table's parents are registered too)
    inline void follow(Chain& c, const Table* t) noexcept {
        while (t && c.n < 8) {
            c.tables[c.n++] = t;
            t = t->parent ? table_of(t->parent) : nullptr;
        }
    }

    // The language of a table's names: a single-language name in a table
    // of the chain, the first that has it
    template<class F>
    inline std::string_view first_of(const Chain& c, F&& f) noexcept {
        for (uint32_t i = 0; i < c.n; ++i) {
            std::string_view v = f(*c.tables[i]);
            if (!v.empty()) {
                return v;
            }
        }
        return {};
    }

    SGCL_INLINE_HOT std::string_view text(const Table& t, uint32_t i) noexcept {
        return t.texts[i];
    }

    inline std::string_view language_name(const Table& t, uint64_t key, uint32_t language15) noexcept {
        if (key && t.tagged) {
            uint32_t i = find_key(t.tagged_keys, t.tagged, key);
            if (i < t.tagged) {
                return text(t, t.tagged_names[i]);
            }
        }
        if (!key) {
            uint32_t i = find_key(t.language_keys, t.languages, language15);
            if (i < t.languages) {
                return text(t, t.language_names[i]);
            }
        }
        return {};
    }

    inline std::string_view region_name(const Table& t, uint32_t region) noexcept {
        uint32_t i = find_key(t.region_keys, t.regions, region);
        return i < t.regions ? text(t, t.region_names[i]) : std::string_view();
    }

    // a script's name inside a locale's name, or (alone) its name standing alone
    inline std::string_view script_name(const Table& t, uint32_t script, bool alone = false) noexcept {
        if (alone) {
            uint32_t i = find_key(t.alone_keys, t.alones, script);
            if (i < t.alones) {
                return text(t, t.alone_names[i]);
            }
        }
        uint32_t i = find_key(t.script_keys, t.scripts, script);
        return i < t.scripts ? text(t, t.script_names[i]) : std::string_view();
    }

    // a currency's name (form 0) or its name in a plural form (1 + the
    // category)
    inline std::string_view currency_name(const Table& t, uint32_t code, uint32_t form) noexcept {
        uint32_t i = find_key(t.currency_keys, t.currencies, code);
        return i < t.currencies ? text(t, t.currency_names[i * 7 + form]) : std::string_view();
    }

    // a zone's name: 0 the exemplar city, 1-3 long generic standard
    // daylight, 4-6 short
    inline std::string_view zone_name(const Table& t, uint32_t zone, uint32_t which) noexcept {
        uint32_t i = find_key(t.zone_keys, t.zones, zone);
        return i < t.zones ? text(t, t.zone_names[i * 7 + which]) : std::string_view();
    }

    // a metazone's name: 0-2 long generic standard daylight, 3-5 short
    inline std::string_view meta_name(const Table& t, uint32_t meta, uint32_t which) noexcept {
        uint32_t i = find_key(t.meta_keys, t.metas, meta);
        return i < t.metas ? text(t, t.meta_names[i * 6 + which]) : std::string_view();
    }

    inline std::string_view format(const Table& t, uint32_t which) noexcept {
        return text(t, t.formats[which]);
    }

    // Once a locale, in a Debug build: the names asked for in a locale whose
    // header is not included fall back on the codes
    inline void missing(uint64_t key, std::string_view tag) noexcept {
#ifndef NDEBUG
        static std::atomic<uint64_t> said[16] = {};
        for (auto& s : said) {
            uint64_t v = s.load(std::memory_order_relaxed);
            if (v == key + 1) {
                return;
            }
            if (v == 0 && s.compare_exchange_strong(v, key + 1)) {
                std::fprintf(stderr, "sgcl::txt: no display names for %.*s: include \"sgcl/txt/names/%.*s.h\"\n",
                             int(tag.size()), tag.data(), int(tag.size()), tag.data());
                return;
            }
        }
#else
        (void)key;
        (void)tag;
#endif
    }
}
