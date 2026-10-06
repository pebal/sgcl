//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/slice.h"
#include "../core/string.h"
#include "detail/cldr.h"
#include "detail/cldr_lists.h"
#include "locale.h"

#include <cstdint>
#include <initializer_list>
#include <string_view>

// A list as a locale writes it (LDML Part 2, list patterns): "Ala, Ola i
// Ela", "red, green, or blue", "3 h, 5 min". The header is list_format.h
// because txt::list is the list of values of a stencil.
namespace sgcl::txt {
    enum class list_type : uint8_t {
        conjunction,   // a, b, and c
        disjunction,   // a, b, or c
        unit,          // 3 h, 5 min: the parts of one measure
    };

    namespace detail::cldr {
        // Spanish writes "e" for "y" before the sound i, and "u" for "o"
        // before the sound o; Hebrew a hyphen after "ו" before a word that
        // is not written in Hebrew. ICU does the same.
        inline bool spanish_i(std::string_view s) noexcept {
            auto lower = [](std::string_view t, size_t i) -> char {
                char c = i < t.size() ? t[i] : 0;
                return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
            };
            size_t k = 0;
            bool h = lower(s, 0) == 'h';
            if (h) {
                k = 1;
            }
            bool i = lower(s, k) == 'i' || (s.substr(k, 2) == "\xC3\xAD" || s.substr(k, 2) == "\xC3\x8D");
            if (!i) {
                return false;
            }
            if (!h) {
                return true;
            }
            size_t after = k + (lower(s, k) == 'i' ? 1 : 2);
            char next = lower(s, after);
            return next != 'a' && next != 'e';   // hielo, hiato: a diphthong, y
        }

        inline bool spanish_o(std::string_view s) noexcept {
            auto lower = [](std::string_view t, size_t i) -> char {
                char c = i < t.size() ? t[i] : 0;
                return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
            };
            size_t k = lower(s, 0) == 'h' ? 1 : 0;
            if (lower(s, k) == 'o' || s.substr(k, 2) == "\xC3\xB3" || s.substr(k, 2) == "\xC3\x93") {
                return true;
            }
            if (!s.empty() && s[0] == '8') {
                return true;
            }
            // 11, 11.000, 11 000: once, once mil
            if (s.substr(0, 2) == "11") {
                size_t i = 2;
                while (i < s.size()) {
                    if (s[i] == '.') {
                        ++i;
                    }
                    size_t d = 0;
                    while (i + d < s.size() && s[i + d] >= '0' && s[i + d] <= '9') {
                        ++d;
                    }
                    if (d == 0) {
                        break;
                    }
                    if (d != 3) {
                        return false;
                    }
                    i += d;
                }
                return i == s.size() || s[i] == ' ';
            }
            return false;
        }

        inline bool hebrew_start(std::string_view s) noexcept {
            // U+0590..U+05FF: D6 90 .. D7 BF
            return s.size() >= 2 && (uint8_t(s[0]) == 0xD6 || uint8_t(s[0]) == 0xD7)
                && (uint8_t(s[0]) == 0xD7 || uint8_t(s[1]) >= 0x90);
        }

        struct ListSink {
            char* at;
            char* end;
            size_t size = 0;

            void put(std::string_view s) noexcept {
                size_t room = size_t(end - at);
                size_t n = s.size() < room ? s.size() : room;
                for (size_t i = 0; i < n; ++i) {
                    at[i] = s[i];
                }
                at += n;
                size += s.size();
            }
        };

        // A pattern of two places, {0} and {1}, written around the first
        // item and what follows; what follows is either one item or the
        // rest of the list, written by more
        template<class More>
        void list_pattern(ListSink& out, std::string_view pattern, std::string_view first, More&& more,
                          std::string_view second_start, uint16_t language_kind) {
            // the Spanish and Hebrew forms of the connector
            char fixed[64];
            if (language_kind == 1 && pattern.size() < sizeof fixed) {
                size_t y = pattern.find(" y {1}");
                size_t o = pattern.find(" o {1}");
                if ((y != std::string_view::npos && spanish_i(second_start))
                    || (o != std::string_view::npos && spanish_o(second_start))) {
                    size_t at = y != std::string_view::npos ? y : o;
                    for (size_t i = 0; i < pattern.size(); ++i) {
                        fixed[i] = pattern[i];
                    }
                    fixed[at + 1] = y != std::string_view::npos ? 'e' : 'u';
                    pattern = std::string_view(fixed, pattern.size());
                }
            } else if (language_kind == 2 && pattern.size() + 1 < sizeof fixed) {
                size_t v = pattern.find("\xD7\x95{1}");
                if (v != std::string_view::npos && !second_start.empty() && !hebrew_start(second_start)) {
                    size_t n = 0;
                    for (size_t i = 0; i < pattern.size(); ++i) {
                        fixed[n++] = pattern[i];
                        if (i == v + 1) {
                            fixed[n++] = '-';
                        }
                    }
                    pattern = std::string_view(fixed, n);
                }
            }
            size_t i = 0;
            while (i < pattern.size()) {
                if (pattern[i] == '{' && i + 2 < pattern.size() && pattern[i + 2] == '}'
                    && (pattern[i + 1] == '0' || pattern[i + 1] == '1')) {
                    if (pattern[i + 1] == '0') {
                        out.put(first);
                    } else {
                        more();
                    }
                    i += 3;
                    continue;
                }
                size_t j = pattern.find('{', i + 1);
                if (j == std::string_view::npos) {
                    j = pattern.size();
                }
                out.put(pattern.substr(i, j - i));
                i = j;
            }
        }

        inline void write_list(ListSink& out, const slice<const string>& items, uint16_t locale, list_type t,
                               width w, uint16_t language_kind) {
            size_t n = items.size();
            if (n == 0) {
                return;
            }
            if (n == 1) {
                out.put(items[0].view());
                return;
            }
            uint32_t type = uint32_t(t) * 3 + uint32_t(w);
            uint32_t row = ListLocales[ListLocale[locale] * ListLocalesWidth + type];
            auto part = [&](int k) {
                return ListTexts[sparse(ListPatterns, ListPatternsStart, row, uint32_t(k))];
            };
            if (n == 2) {
                list_pattern(out, part(3), items[0].view(), [&] { out.put(items[1].view()); }, items[1].view(),
                             language_kind);
                return;
            }
            if (n == 3 && !part(4).empty()) {
                // a pattern of three: {0}, {1} and {2} in one
                std::string_view p = part(4);
                size_t i = 0;
                while (i < p.size()) {
                    if (p[i] == '{' && i + 2 < p.size() && p[i + 2] == '}' && p[i + 1] >= '0' && p[i + 1] <= '2') {
                        out.put(items[size_t(p[i + 1] - '0')].view());
                        i += 3;
                        continue;
                    }
                    out.put(p.substr(i, 1));
                    ++i;
                }
                return;
            }
            // start(item 0, middle(item 1, ... end(item n-2, item n-1)))
            struct Rest {
                ListSink& out;
                const slice<const string>& items;
                size_t n;
                std::string_view middle, end;
                uint16_t kind;
                void operator()(size_t i) const {
                    if (i == n - 2) {
                        list_pattern(out, end, items[i].view(), [&] { out.put(items[i + 1].view()); },
                                     items[i + 1].view(), kind);
                        return;
                    }
                    list_pattern(out, middle, items[i].view(), [&] { (*this)(i + 1); }, items[i + 1].view(), kind);
                }
            } rest{out, items, n, part(1), part(2), language_kind};
            list_pattern(out, part(0), items[0].view(), [&] { rest(1); }, items[1].view(), language_kind);
        }

        inline uint16_t list_language_kind(const locale& l) noexcept {
            uint32_t language = l.subtag();
            return language == (uint32_t('e') << 8 | 's') ? 1 : language == (uint32_t('h') << 8 | 'e') ? 2 : 0;
        }
    }

    // The items joined as the locale joins them: "Ala, Ola i Ela" in
    // Polish; an empty text for no items, the item itself for one
    inline string format_list(const slice<const string>& items, const locale& l = {},
                              list_type t = list_type::conjunction, width w = width::wide) {
        uint16_t index = detail::cldr::locale_index(l);
        uint16_t kind = detail::cldr::list_language_kind(l);
        char room[256];
        detail::cldr::ListSink out{room, room + sizeof room};
        detail::cldr::write_list(out, items, index, t, w, kind);
        if (out.size <= sizeof room) {
            return string(std::string_view(room, out.size));
        }
        size_t n = out.size;
        return sgcl::detail::StringAccess::bounded<string>(n, [&](char* chars) {
            detail::cldr::ListSink again{chars, chars + n};
            detail::cldr::write_list(again, items, index, t, w, kind);
            return again.size < n ? again.size : n;
        });
    }

    // The same for a list written in the call: format_list({"a", "b"}, l)
    inline string format_list(std::initializer_list<string> items, const locale& l = {},
                              list_type t = list_type::conjunction, width w = width::wide) {
        return format_list(slice<const string>(items.begin(), items.size()), l, t, w);
    }
}
