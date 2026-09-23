//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../containers/vector.h"
#include "properties.h"
#include "detail/normalize_tables.h"

// The same text can be written in more than one way: "é" is one code
// point or two, and a Korean syllable is one or three. Normalization
// (UAX #15) puts a text into one of four forms so that two texts a reader
// would call the same compare the same — which is what a search, a key of
// a map and a file name need.
//
// The form is a tag, not an enum with a branch: normalize(s, nfc) reads
// like the enum would at the call site, the walk has no runtime switch,
// and a program that never asks for the compatibility forms does not
// carry their table, which is the larger half of the data.
namespace sgcl::txt {
    // nfd, nfkd: decomposed — every character taken apart into its base
    // and its marks, the marks in canonical order. nfc, nfkc: composed —
    // decomposed first and then put back together, which is the form the
    // web and most file systems want. The k forms decompose by
    // compatibility as well, so that "ﬁ" becomes "fi", "①" becomes "1"
    // and a fullwidth letter becomes a plain one: the same meaning, not
    // the same appearance, and the mapping cannot be undone.
    struct nfc_t {};
    struct nfd_t {};
    struct nfkc_t {};
    struct nfkd_t {};

    inline constexpr nfc_t nfc {};
    inline constexpr nfd_t nfd {};
    inline constexpr nfkc_t nfkc {};
    inline constexpr nfkd_t nfkd {};

    namespace detail {
        // The Hangul syllables are not in any table: they decompose and
        // compose by arithmetic, eleven thousand of them from nineteen
        // leading jamo, twenty-one vowels and twenty-seven finals
        inline constexpr char32_t HangulSBase = 0xAC00;
        inline constexpr char32_t HangulLBase = 0x1100;
        inline constexpr char32_t HangulVBase = 0x1161;
        inline constexpr char32_t HangulTBase = 0x11A7;
        inline constexpr unsigned HangulLCount = 19;
        inline constexpr unsigned HangulVCount = 21;
        inline constexpr unsigned HangulTCount = 28;
        inline constexpr unsigned HangulNCount = HangulVCount * HangulTCount;
        inline constexpr unsigned HangulSCount = HangulLCount * HangulNCount;

        constexpr bool is_hangul_syllable(char32_t c) noexcept {
            return c >= HangulSBase && c < HangulSBase + HangulSCount;
        }

        constexpr uint8_t ccc_fn(char32_t c) noexcept {
            return c < 0x300 ? 0 : uint8_t(value_of(c, normalize_tables::CombiningClass));
        }

        // Two bits a form, in the order nfc, nfd, nfkc, nfkd. Nothing
        // below U+00A0 has a decomposition of any kind, so ASCII and the
        // C1 controls are in every form without a table; the bound is
        // asserted where the tables are generated, and it is not the
        // U+0300 of the combining class — e and acute compose below it
        constexpr unsigned quick_check(char32_t c, unsigned form) noexcept {
            return c < 0xA0 ? QuickCheckYes : (value_of(c, normalize_tables::QuickCheck) >> (2 * form)) & 3;
        }

        // The tag as the two numbers the tables are indexed by: which
        // quick check column, and whether the compatibility mappings
        // take part
        constexpr unsigned form_index(nfc_t) noexcept { return 0; }
        constexpr unsigned form_index(nfd_t) noexcept { return 1; }
        constexpr unsigned form_index(nfkc_t) noexcept { return 2; }
        constexpr unsigned form_index(nfkd_t) noexcept { return 3; }

        constexpr bool composes(nfc_t) noexcept { return true; }
        constexpr bool composes(nfd_t) noexcept { return false; }
        constexpr bool composes(nfkc_t) noexcept { return true; }
        constexpr bool composes(nfkd_t) noexcept { return false; }

        constexpr bool compatible(nfc_t) noexcept { return false; }
        constexpr bool compatible(nfd_t) noexcept { return false; }
        constexpr bool compatible(nfkc_t) noexcept { return true; }
        constexpr bool compatible(nfkd_t) noexcept { return true; }

        // What a and b compose to, or zero. The Hangul cases are the
        // arithmetic ones: a leading jamo and a vowel make a syllable,
        // and a syllable without a final takes one
        constexpr char32_t compose_pair(char32_t a, char32_t b) noexcept {
            if (a >= HangulLBase && a < HangulLBase + HangulLCount
                && b >= HangulVBase && b < HangulVBase + HangulVCount) {
                return HangulSBase + ((a - HangulLBase) * HangulVCount + (b - HangulVBase)) * HangulTCount;
            }
            if (is_hangul_syllable(a) && (a - HangulSBase) % HangulTCount == 0
                && b > HangulTBase && b < HangulTBase + HangulTCount) {
                return a + (b - HangulTBase);
            }
            return composed(a, b, normalize_tables::Composition);
        }

        // One code point, taken apart as far as it goes. A canonical
        // decomposition can chain (a letter with two accents is written
        // with one of them precomposed), and a compatibility one can end
        // in something with a canonical decomposition of its own, so both
        // are followed to the bottom.
        // The buffer is a template so that the collator can decompose
        // into a few words of stack rather than into a container: it
        // takes a text apart one combining sequence at a time and never
        // holds more than one
        template<bool Compatibility, class Buffer>
        void decompose_into(Buffer& out, char32_t c) {
            if (is_hangul_syllable(c)) {
                unsigned s = unsigned(c - HangulSBase);
                out.push_back(HangulLBase + s / HangulNCount);
                out.push_back(HangulVBase + (s % HangulNCount) / HangulTCount);
                if (unsigned t = s % HangulTCount) {
                    out.push_back(HangulTBase + t);
                }
                return;
            }
            // whether it comes apart at all is one read, where looking
            // for the decomposition is a search: almost nothing does, and
            // this is asked about every code point of every text
            bool canonical = in_set(c, normalize_tables::HasCanonical);
            if (!canonical && !(Compatibility && in_set(c, normalize_tables::HasCompat))) {
                out.push_back(c);
                return;
            }
            auto d = canonical ? decomposition_of(c, normalize_tables::CanonicalDecomposition)
                               : decomposition_of(c, normalize_tables::CompatDecomposition);
            if (!d) {
                out.push_back(c);
                return;
            }
            for (size_t i = 0; i < d.size; ++i) {
                char32_t x = d.units[i];
                if (x >= 0xD800 && x < 0xDC00 && i + 1 < d.size) {
                    x = 0x10000 + ((x - 0xD800) << 10) + (d.units[i + 1] - 0xDC00);
                    ++i;
                }
                decompose_into<Compatibility>(out, x);
            }
        }

        // The marks of every combining sequence put in the order the
        // standard fixes: a stable sort by the combining class, which
        // over runs this short is an insertion sort
        template<class Buffer>
        void canonical_order(Buffer& buffer, size_t from = 1) {
            for (size_t i = from < 1 ? 1 : from; i < buffer.size(); ++i) {
                uint8_t cc = ccc_fn(buffer[i]);
                if (cc == 0) {
                    continue;
                }
                char32_t c = buffer[i];
                size_t j = i;
                while (j > 0) {
                    uint8_t before = ccc_fn(buffer[j - 1]);
                    if (before == 0 || before <= cc) {
                        break;
                    }
                    buffer[j] = buffer[j - 1];
                    --j;
                }
                buffer[j] = c;
            }
        }

        // The composition of UAX #15: walk the decomposed text keeping
        // the last starter, and put back together what is not blocked
        // from it — a character is blocked when something between it and
        // the starter has a combining class of its own that is not lower
        inline void compose_buffer(vector<char32_t>& buffer) {
            if (buffer.empty()) {
                return;
            }
            size_t starter = 0;
            size_t written = 1;
            int last = -1;
            for (size_t i = 1; i < buffer.size(); ++i) {
                char32_t c = buffer[i];
                int cc = ccc_fn(c);
                char32_t composite = (last < cc || last == -1) ? compose_pair(buffer[starter], c) : 0;
                if (composite) {
                    buffer[starter] = composite;
                    continue;
                }
                if (cc == 0) {
                    starter = written;
                    last = -1;
                } else {
                    last = cc;
                }
                buffer[written++] = c;
            }
            buffer.resize(written);
        }

        // Whether the text is already in the form, by the quick check
        // properties alone: "no" settles it, "maybe" does not and the
        // caller has to do the work
        inline bool quick_check_text(std::string_view text, unsigned form, bool& maybe) {
            uint8_t last = 0;
            maybe = false;
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                uint8_t cc = ccc_fn(c);
                if (last > cc && cc != 0) {
                    return false;          // the marks are out of order
                }
                unsigned q = quick_check(c, form);
                if (q == QuickCheckNo) {
                    return false;
                }
                if (q == QuickCheckMaybe) {
                    maybe = true;
                }
                last = cc;
            }
            return true;
        }

        template<class Form>
        vector<char32_t> normalized_points(std::string_view text, Form form) {
            vector<char32_t> buffer;
            buffer.reserve(text.size());
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                decompose_into<compatible(form)>(buffer, c);
            }
            canonical_order(buffer);
            if constexpr (composes(form)) {
                compose_buffer(buffer);
            }
            return buffer;
        }

        // A string is immutable, so the bytes are laid out first and the
        // string made once from them
        // The room is taken from the widths and the bytes are written
        // through a pointer, not appended one code point at a time: this
        // is the common exit of every normalization, of the full case
        // mappings and of decompose(), and appending cost 1021 ns over
        // two hundred code points where writing costs 355.
        inline string encoded(const vector<char32_t>& points) {
            size_t bytes = 0;
            for (auto c : points) {
                bytes += utf8::width(c);
            }
            std::string out(bytes, '\0');
            char* at = out.data();
            for (auto c : points) {
                at += utf8::encode(c, at);
            }
            return string(out.data(), size_t(at - out.data()));
        }
    }

    // Whether the text is in the form already. The quick check properties
    // answer most texts without looking at a single decomposition; where
    // they say "maybe", the text is normalized and compared.
    template<class Form>
    bool is_normalized(const string& text, Form form) {
        bool maybe = false;
        if (!detail::quick_check_text(text.view(), detail::form_index(form), maybe)) {
            return false;
        }
        if (!maybe) {
            return true;
        }
        return detail::encoded(detail::normalized_points(text.view(), form)) == text;
    }

    // The text in the form. A text that is in it already comes back as
    // the same object — the strings of the library are shared by copying,
    // so nothing is allocated and nothing is copied for the common case.
    // The quick check is asked here rather than through is_normalized,
    // because that one normalizes to answer "maybe" and the answer was
    // then thrown away and the work done again: a text that does need
    // normalizing cost 1510 ns where the check alone costs 793.
    template<class Form>
    string normalize(const string& text, Form form) {
        bool maybe = false;
        if (detail::quick_check_text(text.view(), detail::form_index(form), maybe) && !maybe) {
            return text;
        }
        auto made = detail::encoded(detail::normalized_points(text.view(), form));
        // The same text keeps the object it came in, which a shared and
        // immutable string is worth holding on to
        return made == text ? text : made;
    }

    // Whether two texts are the same text written differently: canonical
    // equivalence, which is what a search and a key of a map want
    inline bool equal_normalized(const string& a, const string& b) {
        if (a == b) {
            return true;
        }
        return detail::normalized_points(a.view(), nfd) == detail::normalized_points(b.view(), nfd);
    }

    // The order of two texts, blind to the way they are written: negative
    // when a comes first, zero when they are the same text. Not a
    // language's order — that is what a collator is for
    inline int compare_normalized(const string& a, const string& b) {
        auto x = detail::normalized_points(a.view(), nfd);
        auto y = detail::normalized_points(b.view(), nfd);
        size_t n = std::min(x.size(), y.size());
        for (size_t i = 0; i < n; ++i) {
            if (x[i] != y[i]) {
                return x[i] < y[i] ? -1 : 1;
            }
        }
        return x.size() == y.size() ? 0 : (x.size() < y.size() ? -1 : 1);
    }

    // A hash two texts share when equal_normalized says they are one: the
    // key of a map that must not care which form a name arrived in
    inline size_t hash_normalized(const string& text) {
        size_t h = 14695981039346656037ull;
        for (auto c : detail::normalized_points(text.view(), nfd)) {
            h = (h ^ size_t(c)) * 1099511628211ull;
        }
        return h;
    }

    // The canonical combining class: zero for a starter, and for a mark
    // the number that decides the order the marks are put in
    inline constexpr sgcl::detail::code_point_fn<detail::ccc_fn> combining_class {};

    // What two code points compose to, or zero when they do not — the
    // Hangul syllables included, which compose by arithmetic
    inline constexpr sgcl::detail::code_point_fn<detail::compose_pair> compose {};

    // One code point taken apart as far as it goes, canonically
    inline string decompose(char32_t c) {
        vector<char32_t> out;
        detail::decompose_into<false>(out, c);
        detail::canonical_order(out);
        return detail::encoded(out);
    }
}
