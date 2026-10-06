//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"
#include "detail/cldr.h"
#include "detail/cldr_currencies.h"
#include "locale.h"
#include "plural.h"

#include <cstdint>
#include <string_view>

// A currency: the code of ISO 4217 in two bytes, with what CLDR says of it —
// how many digits an amount has (two; none for the yen; three for the
// Kuwaiti dinar), how cash is rounded (Swiss francs to 0.05), and what a
// locale writes for it ("zł" in Polish, "PLN" in English, "$" for dollars
// in a narrow space).
namespace sgcl::txt {
    class currency;

    namespace detail::cldr {
        struct CurrencyAccess;
        inline string currency_symbol(const currency& c, uint16_t locale_index, bool narrow);
        inline string currency_name(const currency& c, const locale& in, uint32_t form);
    }

    class currency {
    public:
        constexpr currency() noexcept = default;

        // A code of three ASCII letters, in any case; anything else
        // throws bad_expected_access<code_error> (parse's error): a code
        // the program itself writes
        explicit currency(const string& code)
        : currency(parse(code).value()) {
        }

        // A code read from outside the program: three ASCII letters in
        // any case, "pln" is PLN; a code ISO 4217 does not list is still a
        // currency, with two digits and its code for a symbol
        static expected<currency, code_error> parse(const string& code) noexcept {
            return _parse(code.view());
        }

        // The currency of a locale's region today, the region filled from
        // the likely subtags when the locale has none: "pl" is PLN,
        // "de-CH" CHF, "en" USD; no currency for a region without one
        static currency of(const locale& l) noexcept {
            using A = detail::cldr::LocaleAccess;
            uint64_t key = A::key(l);
            if (!A::region(key)) {
                key = detail::cldr::maximize(key);
            }
            size_t i = detail::cldr::find(detail::cldr::RegionCurrencyKeys, uint16_t(A::region(key)));
            if (i == std::size(detail::cldr::RegionCurrencyKeys)) {
                return currency();
            }
            currency c;
            c._code = detail::cldr::CurrencyCodes[detail::cldr::RegionCurrencies[i]];
            return c;
        }

        constexpr bool operator==(const currency&) const noexcept = default;

        SGCL_INLINE_HOT constexpr explicit operator bool() const noexcept {
            return _code != 0;
        }

        // The code, "PLN"; empty for no currency
        string code() const {
            char out[3];
            return string(std::string_view(out, _text(out)));
        }

        // The digits of an amount (CLDR's currencyData; two for a currency
        // it does not list)
        int digits() const noexcept {
            return _info() & 15;
        }

        // The digits of an amount paid in cash: the Hungarian forint has
        // two, and none in cash
        int cash_digits() const noexcept {
            return (_info() >> 4) & 15;
        }

        // The step cash is rounded to, in units of the last cash digit:
        // 5 for the Swiss franc (0.05), 0 for none
        int cash_increment() const noexcept {
            return _info() >> 8;
        }

        // What the locale writes for the currency: "zł" in pl, "PLN" in
        // en, "US$" in pl for dollars; the code where it has no symbol
        string symbol(const locale& l) const {
            return string(detail::cldr::currency_symbol(*this, detail::cldr::locale_index(l), false));
        }

        // The narrow symbol, where a reader knows from the context which
        // dollar is meant: "$", "zł"; the symbol where there is none
        string narrow_symbol(const locale& l) const {
            return string(detail::cldr::currency_symbol(*this, detail::cldr::locale_index(l), true));
        }

        // The currency's name in the language of in, from in's display
        // names (sgcl/txt/names/<in>.h): "złoty polski"; the code where they
        // are not included
        string display_name(const locale& in) const {
            return detail::cldr::currency_name(*this, in, 0);
        }

        // Its name in a plural form, as it follows a number: "złotych" for
        // many; the form other where the language has no name of that form
        string display_name(const locale& in, plural p) const {
            return detail::cldr::currency_name(*this, in, 1 + uint32_t(p));
        }

    private:
        friend struct detail::cldr::CurrencyAccess;

        // the reading itself, of text where it lies (CurrencyAccess::parse)
        static expected<currency, code_error> _parse(std::string_view s) noexcept {
            for (size_t i = 0; i < s.size() && i < 3; ++i) {
                char c = s[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
                    return unexpected(code_error(code_error::kind::currency, i));
                }
            }
            if (s.size() != 3) {
                return unexpected(code_error(code_error::kind::currency, s.size() < 3 ? s.size() : 3));
            }
            currency c;
            c._code = _pack(s);
            return c;
        }

        static constexpr uint16_t _pack(std::string_view s) noexcept {
            if (s.size() != 3) {
                return 0;
            }
            uint16_t v = 0;
            for (char c : s) {
                if (c >= 'a' && c <= 'z') {
                    c = char(c - 32);
                }
                if (c < 'A' || c > 'Z') {
                    return 0;
                }
                v = uint16_t((v << 5) | (c - 'A' + 1));
            }
            return v;
        }

        size_t _text(char* out) const noexcept {
            if (!_code) {
                return 0;
            }
            for (int i = 0; i < 3; ++i) {
                out[i] = char('A' - 1 + ((_code >> (5 * (2 - i))) & 31));
            }
            return 3;
        }

        uint16_t _info() const noexcept {
            size_t i = detail::cldr::find(detail::cldr::CurrencyCodes, _code);
            return i == std::size(detail::cldr::CurrencyCodes) ? 2 | (2 << 4) : detail::cldr::CurrencyInfo[i];
        }

        uint16_t _code = 0;
    };

    namespace detail::cldr {
        struct CurrencyAccess {
            // a code where it lies (a fuzzer's buffer): what parse reads
            static expected<currency, code_error> parse(std::string_view s) noexcept {
                return currency::_parse(s);
            }

            SGCL_INLINE_HOT static constexpr uint16_t packed(const currency& c) noexcept {
                return c._code;
            }

            SGCL_INLINE_HOT static size_t text(const currency& c, char* out) noexcept {
                return c._text(out);
            }

            SGCL_INLINE_HOT static uint16_t info(const currency& c) noexcept {
                return c._info();
            }
        };

        // A symbol as the formatter needs it: the text, and whether its
        // first and last characters are letters, which currencySpacing
        // asks (bit 0 the last, bit 1 the first)
        struct CurrencyText {
            std::string_view text;
            uint8_t spacing = 3;
        };

        inline uint32_t symbol_entry(uint32_t run, uint16_t code_index) noexcept {
            size_t first = run >> 16;
            size_t n = run & 0xFFFF;
            size_t lo = 0;
            while (n > 0) {
                size_t half = n / 2;
                if ((CurrencySymbols[first + lo + half] & 0x1FF) < code_index) {
                    lo += half + 1;
                    n -= half + 1;
                } else {
                    n = half;
                }
            }
            if (lo >= (run & 0xFFFF)) {
                return UINT32_MAX;
            }
            uint32_t e = CurrencySymbols[first + lo];
            return (e & 0x1FF) == code_index ? e : UINT32_MAX;
        }

        // The run of a locale and then of its base; UINT32_MAX when
        // neither names the currency
        inline uint32_t locale_symbol(const uint32_t* runs, uint16_t locale_index, uint16_t code_index) noexcept {
            uint32_t e = symbol_entry(runs[locale_index], code_index);
            if (e == UINT32_MAX && CurrencyBase[locale_index]) {
                e = symbol_entry(runs[CurrencyBase[locale_index]], code_index);
            }
            return e;
        }

        inline CurrencyText currency_text(const currency& c, uint16_t locale_index, bool narrow) noexcept {
            uint16_t code = CurrencyAccess::packed(c);
            size_t ci = find(CurrencyCodes, code);
            if (ci != std::size(CurrencyCodes)) {
                if (narrow) {
                    uint32_t e = locale_symbol(CurrencyNarrowRuns, locale_index, uint16_t(ci));
                    if (e != UINT32_MAX && (e >> 16)) {
                        return {CurrencyTexts[e >> 16], uint8_t((e >> 9) & 3)};
                    }
                    if (CurrencyNarrow[ci]) {
                        return {CurrencyTexts[CurrencyNarrow[ci] & 0xFFFF],
                                uint8_t(CurrencyNarrow[ci] >> 16)};
                    }
                }
                uint32_t e = locale_symbol(CurrencySymbolRuns, locale_index, uint16_t(ci));
                if (e != UINT32_MAX && (e >> 16)) {
                    return {CurrencyTexts[e >> 16], uint8_t((e >> 9) & 3)};
                }
            }
            return {std::string_view(), 3};   // the code itself, letters on both sides
        }

        inline string currency_symbol(const currency& c, uint16_t locale_index, bool narrow) {
            CurrencyText t = currency_text(c, locale_index, narrow);
            if (!t.text.empty()) {
                return string(t.text);
            }
            return c.code();
        }
    }

    namespace detail::cldr {
        // The name of a form (0 the name, 1 + a category), else of the form
        // other, else the name, else the code
        inline string currency_name(const currency& c, const locale& in, uint32_t form) {
            auto chain = names::chain_of(in);
            uint16_t code = CurrencyAccess::packed(c);
            for (uint32_t f : {form, form ? 6u : 0u, 0u}) {
                std::string_view v = names::first_of(chain, [&](const names::Table& t) {
                    return names::currency_name(t, code, f);
                });
                if (!v.empty()) {
                    return string(v);
                }
            }
            return c.code();
        }
    }
}

