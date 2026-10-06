//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "detail/cldr.h"
#include "detail/cldr_autonyms.h"
#include "detail/cldr_likely.h"
#include "detail/cldr_locales.h"
#include "names/detail/lookup.h"

#include <cstdint>
#include <cstdlib>
#include <string_view>

// A language, and where and how it is written: the value every function of
// the module that depends on a language takes. It reads a BCP-47 tag or
// the name of a POSIX locale and keeps three of its parts — the language,
// the script and the region — and one key of the Unicode extension,
// -u-nu-latn, the one a formatter of numbers is asked for most. The rest
// of a tag (variants, other extensions, private use) is read past.
//
// The data that a locale selects lives in the headers of each group
// (numbers, dates, lists...); what this header carries is what the type
// itself answers: the likely subtags of every language CLDR knows, the
// parents CLDR names, the language matching, and each locale's own name.
namespace sgcl::txt {
    class locale;

    namespace detail::cldr {
        struct LocaleAccess;
    }

    // Why a text is not a code of ISO 4217 (a currency), of ISO 3166 or
    // UN M.49 (a region) or of ISO 15924 (a script): the byte the reading
    // stopped on, and what was expected there
    class code_error {
    public:
        enum class kind : uint8_t {
            currency,   // three ASCII letters
            region,     // two ASCII letters or three digits
            script,     // four ASCII letters
        };

        SGCL_INLINE_HOT constexpr code_error(kind what, size_t offset) noexcept
        : _what(what)
        , _offset(offset) {
        }

        SGCL_INLINE_HOT constexpr size_t offset() const noexcept {
            return _offset;
        }

        SGCL_INLINE_HOT constexpr kind what() const noexcept {
            return _what;
        }

        string message() const {
            switch (_what) {
                case kind::currency: return string("not a currency code: three ASCII letters expected");
                case kind::region: return string("not a region code: two ASCII letters or three digits expected");
                default: return string("not a script code: four ASCII letters expected");
            }
        }

        friend constexpr bool operator==(const code_error&, const code_error&) noexcept = default;

    private:
        kind _what;
        size_t _offset;
    };

    // The widths CLDR writes names and phrases in: "September", "Sep",
    // "S"; "a, b, and c", "a, b, & c", "a, b, c"; "in 3 days", "in 3
    // days", "in 3d". Shared by lists, relative time and the names of
    // months and days.
    enum class width : uint8_t {
        wide,
        abbreviated,
        narrow,
    };

    // The form of a name: the one it takes inside a phrase ("24 września",
    // the format form) or the one it takes standing alone, in a list or a
    // heading ("wrzesień"). Languages that inflect their names tell them
    // apart; for the others they are one.
    enum class name_context : uint8_t {
        format,
        standalone,
    };

    // A region: a code of ISO 3166 (two letters) or UN M.49 (three digits),
    // in two bytes, packed as a locale packs its region
    class region {
    public:
        constexpr region() noexcept = default;

        // A code the program itself writes; anything else throws
        // bad_expected_access<code_error> (parse's error)
        explicit region(const string& code)
        : region(parse(code).value()) {
        }

        // Two ASCII letters in any case ("pl" is PL) or three digits ("419")
        static expected<region, code_error> parse(const string& code) noexcept {
            return _parse(code.view());
        }

        constexpr bool operator==(const region&) const noexcept = default;

    private:
        friend struct detail::cldr::LocaleAccess;

        // the reading itself, of text where it lies
        static expected<region, code_error> _parse(std::string_view s) noexcept {
            auto letter = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
            auto digit = [](char c) { return c >= '0' && c <= '9'; };
            if (s.size() == 3 && digit(s[0])) {
                for (size_t i = 1; i < 3; ++i) {
                    if (!digit(s[i])) {
                        return unexpected(code_error(code_error::kind::region, i));
                    }
                }
                region r;
                r._code = uint16_t(729 + (s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0'));
                return r;
            }
            for (size_t i = 0; i < s.size() && i < 2; ++i) {
                if (!letter(s[i])) {
                    return unexpected(code_error(code_error::kind::region, i));
                }
            }
            if (s.size() != 2) {
                return unexpected(code_error(code_error::kind::region, s.size() < 2 ? s.size() : 2));
            }
            auto up = [](char c) { return c >= 'a' && c <= 'z' ? char(c - 32) : c; };
            region r;
            r._code = uint16_t((up(s[0]) - 'A' + 1) * 27 + (up(s[1]) - 'A' + 1));
            return r;
        }

    public:
        SGCL_INLINE_HOT constexpr explicit operator bool() const noexcept {
            return _code != 0;
        }

        // The code: "PL", "419"; empty for no region
        string code() const {
            char out[3];
            size_t n = 0;
            if (_code >= 729) {
                uint32_t v = _code - 729u;
                out[n++] = char('0' + v / 100);
                out[n++] = char('0' + v / 10 % 10);
                out[n++] = char('0' + v % 10);
            } else if (_code) {
                out[n++] = char('A' - 1 + _code / 27);
                out[n++] = char('A' - 1 + _code % 27);
            }
            return string(std::string_view(out, n));
        }

        // The region's name in the language of in: "Polen" in German, from
        // in's display names (sgcl/txt/names/<in>.h); the code where they
        // are not included
        string display_name(const locale& in) const;

    private:
        friend class locale;

        uint16_t _code = 0;
    };

    // A script: a code of ISO 15924, four letters, in four bytes, packed as
    // a locale packs its script. Not txt::script, which is the enumeration
    // of the script property of Unicode a code point has (properties.h)
    class script_code {
    public:
        constexpr script_code() noexcept = default;

        // A code the program itself writes; anything else throws
        // bad_expected_access<code_error> (parse's error)
        explicit script_code(const string& code)
        : script_code(parse(code).value()) {
        }

        // Four ASCII letters in any case: "latn" is Latn
        static expected<script_code, code_error> parse(const string& code) noexcept {
            return _parse(code.view());
        }

        constexpr bool operator==(const script_code&) const noexcept = default;

    private:
        friend struct detail::cldr::LocaleAccess;

        // the reading itself, of text where it lies
        static expected<script_code, code_error> _parse(std::string_view s) noexcept {
            for (size_t i = 0; i < s.size() && i < 4; ++i) {
                char c = s[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
                    return unexpected(code_error(code_error::kind::script, i));
                }
            }
            if (s.size() != 4) {
                return unexpected(code_error(code_error::kind::script, s.size() < 4 ? s.size() : 4));
            }
            script_code r;
            for (char c : s) {
                r._code = (r._code << 5) | uint32_t((c >= 'a' ? c - 32 : c) - 'A' + 1);
            }
            return r;
        }

    public:

        SGCL_INLINE_HOT constexpr explicit operator bool() const noexcept {
            return _code != 0;
        }

        // The code in title case: "Latn"; empty for no script
        string code() const {
            if (!_code) {
                return string();
            }
            char out[4];
            for (int i = 0; i < 4; ++i) {
                char c = char('a' - 1 + ((_code >> (5 * (3 - i))) & 31));
                out[i] = i == 0 ? char(c - 32) : c;
            }
            return string(std::string_view(out, 4));
        }

        // The script's name in the language of in: "łacińskie" in Polish;
        // the code where in's display names are not included
        string display_name(const locale& in) const;

    private:
        friend class locale;

        uint32_t _code = 0;
    };

    class locale {
    public:
        constexpr locale() noexcept = default;

        SGCL_INLINE_HOT explicit locale(const string& tag) noexcept {
            _parse(tag.view());
        }

        SGCL_INLINE_HOT static constexpr locale root() noexcept {
            return locale();
        }

        SGCL_INLINE_HOT static constexpr locale turkish() noexcept {
            return locale(_packed("tr"), 0);
        }

        SGCL_INLINE_HOT static constexpr locale azerbaijani() noexcept {
            return locale(_packed("az"), 0);
        }

        SGCL_INLINE_HOT static constexpr locale lithuanian() noexcept {
            return locale(_packed("lt"), 0);
        }

        // The user's locale as the environment names it: LC_ALL, else
        // LC_MESSAGES, else LANG; "C" and "POSIX" are the root locale
        static locale system() noexcept {
            for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
                const char* v = std::getenv(name);
                if (v && *v) {
                    locale l;
                    l._parse(std::string_view(v));
                    return l;
                }
            }
            return locale();
        }

        constexpr bool operator==(const locale&) const noexcept = default;

        // Whether the language writes an i the Turkish way, which is the
        // one question the case mappings ask
        SGCL_INLINE_HOT constexpr bool dotted_i() const noexcept {
            return _language == _packed("tr") || _language == _packed("az");
        }

        SGCL_INLINE_HOT constexpr bool keeps_dot() const noexcept {
            return _language == _packed("lt");
        }

        // The language subtag in four bytes, which is what a table keyed
        // by language is looked up with — the collator's tailorings are
        // one such table
        SGCL_INLINE_HOT constexpr uint32_t subtag() const noexcept {
            return _language;
        }

        // Whether the tag asked for Latin digits (-u-nu-latn)
        SGCL_INLINE_HOT constexpr bool latin_digits() const noexcept {
            return _rest & 1;
        }

        string language() const {
            return _language_text();
        }

    private:
        string _language_text() const {
            char out[3];
            size_t n = 0;
            for (int shift = 16; shift >= 0; shift -= 8) {
                if (char c = char((_language >> shift) & 0xFF)) {
                    out[n++] = c;
                }
            }
            return string(std::string_view(out, n));
        }

    public:
        string script() const {
            char out[4];
            size_t n = _script_text(out);
            return string(std::string_view(out, n));
        }

        string region() const {
            char out[3];
            size_t n = _region_text(out);
            return string(std::string_view(out, n));
        }

        // The tag in BCP-47's canonical case: "sr-Latn-RS", "und" for the
        // root locale
        string to_string() const {
            char out[32];
            size_t n = 0;
            if (!_language) {
                for (char c : {'u', 'n', 'd'}) {
                    out[n++] = c;
                }
            } else {
                for (int shift = 16; shift >= 0; shift -= 8) {
                    if (char c = char((_language >> shift) & 0xFF)) {
                        out[n++] = c;
                    }
                }
            }
            char part[4];
            if (size_t k = _script_text(part)) {
                out[n++] = '-';
                for (size_t i = 0; i < k; ++i) {
                    out[n++] = part[i];
                }
            }
            if (size_t k = _region_text(part)) {
                out[n++] = '-';
                for (size_t i = 0; i < k; ++i) {
                    out[n++] = part[i];
                }
            }
            if (latin_digits()) {
                for (char c : std::string_view("-u-nu-latn")) {
                    out[n++] = c;
                }
            }
            return string(std::string_view(out, n));
        }

        // The likely subtags added (TR35 §4.3): "sr" is sr-Cyrl-RS,
        // "zh-TW" zh-Hant-TW, "und-PL" pl-Latn-PL; a language CLDR does
        // not know stays as it is
        locale maximize() const noexcept;

        // The likely subtags taken away: zh-Hant-TW is zh-TW
        locale minimize() const noexcept;

        // The locale CLDR inherits from: es-MX is es-419, es-419 is es, es
        // is the root locale; -u-nu-latn is kept on the way, not on root
        locale parent() const noexcept;

        // The locale's name in its own language: "polski", "Deutsch",
        // "British English"; the language subtag for a language CLDR has
        // no data for, and an empty string for the root locale
        string autonym() const;

        // The locale's name in the language of in, from the display names
        // of in's header (sgcl/txt/names/<in>.h): "niemiecki (Szwajcaria)",
        // or CLDR's own name for the whole tag, "szwajcarski
        // wysokoniemiecki"; the tag where in's names are not included
        string display_name(const locale& in) const;

        // Whether the display names of this locale are included: its
        // header or the header of one of its parents
        bool has_names() const noexcept;

    private:
        friend struct detail::cldr::LocaleAccess;

        SGCL_INLINE_HOT explicit constexpr locale(uint32_t language, uint32_t rest) noexcept
        : _language(language)
        , _rest(rest) {
        }

        static constexpr uint32_t _packed(std::string_view letters) noexcept {
            uint32_t out = 0;
            for (char c : letters) {
                out = (out << 8) | uint32_t(uint8_t(c));
            }
            return out;
        }

        static constexpr char _lower(char c) noexcept {
            return c >= 'A' && c <= 'Z' ? char(c + 32) : c;
        }

        static constexpr bool _alpha(char c) noexcept {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        static constexpr bool _digit(char c) noexcept {
            return c >= '0' && c <= '9';
        }

        static constexpr bool _all(std::string_view s, bool (*f)(char)) noexcept {
            for (char c : s) {
                if (!f(c)) {
                    return false;
                }
            }
            return true;
        }

        static constexpr bool _alnum(char c) noexcept {
            return _alpha(c) || _digit(c);
        }

        // The tag read. The language is two or three letters, anything
        // else the whole of it is the root locale (C, POSIX, an empty
        // tag); after it, an extended language subtag is read past, four
        // letters are a script, two letters or three digits a region, a
        // variant is read past, and the extensions are walked for
        // u-nu-latn. A POSIX name ends at the '.' of its codeset; its
        // '@latin' or '@cyrillic' is a script. Whatever is not one of
        // these ends the reading with what was read so far.
        constexpr void _parse(std::string_view tag) noexcept {
            _language = 0;
            _rest = 0;
            std::string_view modifier;
            if (size_t at = tag.find('@'); at != std::string_view::npos) {
                modifier = tag.substr(at + 1);
                tag = tag.substr(0, at);
            }
            if (size_t dot = tag.find('.'); dot != std::string_view::npos) {
                tag = tag.substr(0, dot);
            }
            size_t pos = 0;
            auto next = [&]() {
                size_t start = pos;
                while (pos < tag.size() && tag[pos] != '-' && tag[pos] != '_') {
                    ++pos;
                }
                std::string_view s = tag.substr(start, pos - start);
                if (pos < tag.size()) {
                    ++pos;
                }
                return s;
            };
            std::string_view first = next();
            if (first.size() < 2 || first.size() > 3 || !_all(first, _alpha)) {
                return;
            }
            char lang[3] = {};
            for (size_t i = 0; i < first.size(); ++i) {
                lang[i] = _lower(first[i]);
            }
            uint32_t language = _packed(std::string_view(lang, first.size()));
            uint32_t script = 0, region = 0;
            bool nu_latn = false;
            int state = 0;   // 0: extlang may come, 1: script, 2: region, 3: variants, 4: extensions
            while (pos < tag.size()) {
                std::string_view s = next();
                if (s.empty()) {
                    break;
                }
                if (state == 0 && s.size() == 3 && _all(s, _alpha)) {
                    continue;   // an extended language subtag: zh-yue
                }
                if (state <= 1 && s.size() == 4 && _all(s, _alpha)) {
                    script = _pack_script(s);
                    state = 2;
                    continue;
                }
                if (state <= 2 && ((s.size() == 2 && _all(s, _alpha)) || (s.size() == 3 && _all(s, _digit)))) {
                    region = _pack_region(s);
                    state = 3;
                    continue;
                }
                if (state <= 3 && ((s.size() >= 5 && s.size() <= 8) || (s.size() == 4 && _digit(s[0])))
                    && _all(s, _alnum)) {
                    state = 3;
                    continue;   // a variant
                }
                if (s.size() == 1 && _alnum(s[0])) {
                    char single = _lower(s[0]);
                    if (single == 'x') {
                        break;   // private use: the rest is not ours
                    }
                    // an extension: its subtags up to the next singleton
                    std::string_view key;
                    while (pos < tag.size()) {
                        size_t save = pos;
                        std::string_view e = next();
                        if (e.size() == 1) {
                            pos = save;
                            break;
                        }
                        if (single == 'u') {
                            if (e.size() == 2) {
                                key = e;
                            } else if (key.size() == 2 && _lower(key[0]) == 'n' && _lower(key[1]) == 'u') {
                                nu_latn = e.size() == 4 && _lower(e[0]) == 'l' && _lower(e[1]) == 'a'
                                       && _lower(e[2]) == 't' && _lower(e[3]) == 'n';
                            }
                        }
                    }
                    state = 4;
                    continue;
                }
                break;
            }
            if (!script && modifier.size() >= 5) {
                if (_lower(modifier[0]) == 'l' && _lower(modifier[1]) == 'a' && _lower(modifier[2]) == 't'
                    && _lower(modifier[3]) == 'i' && _lower(modifier[4]) == 'n') {
                    script = _pack_script("Latn");
                } else if (modifier.size() >= 8 && _lower(modifier[0]) == 'c' && _lower(modifier[1]) == 'y'
                           && _lower(modifier[2]) == 'r') {
                    script = _pack_script("Cyrl");
                }
            }
            // the deprecated codes CLDR replaces (supplementalMetadata):
            // in, iw, ji, jw, mo, and the legacy sh and tl
            switch (language) {
                case _packed("in"): language = _packed("id"); break;
                case _packed("iw"): language = _packed("he"); break;
                case _packed("ji"): language = _packed("yi"); break;
                case _packed("jw"): language = _packed("jv"); break;
                case _packed("mo"): language = _packed("ro"); break;
                case _packed("tl"): language = _packed("fil"); break;
                case _packed("sh"):
                    language = _packed("sr");
                    if (!script) {
                        script = _pack_script("Latn");
                    }
                    break;
                case _packed("und"): language = 0; break;
                default: break;
            }
            _language = language;
            _rest = (script << 12) | (region << 1) | (nu_latn ? 1 : 0);
        }

        static constexpr uint32_t _pack_script(std::string_view s) noexcept {
            uint32_t v = 0;
            for (char c : s) {
                v = (v << 5) | uint32_t(_lower(c) - 'a' + 1);
            }
            return v;
        }

        static constexpr uint32_t _pack_region(std::string_view s) noexcept {
            if (_digit(s[0])) {
                return 729 + uint32_t((s[0] - '0') * 100 + (s[1] - '0') * 10 + (s[2] - '0'));
            }
            auto up = [](char c) { return c >= 'a' && c <= 'z' ? char(c - 32) : c; };
            return uint32_t(up(s[0]) - 'A' + 1) * 27 + uint32_t(up(s[1]) - 'A' + 1);
        }

        size_t _script_text(char* out) const noexcept {
            uint32_t s = _rest >> 12;
            if (!s) {
                return 0;
            }
            for (int i = 0; i < 4; ++i) {
                char c = char('a' - 1 + ((s >> (5 * (3 - i))) & 31));
                out[i] = i == 0 ? char(c - 32) : c;
            }
            return 4;
        }

        size_t _region_text(char* out) const noexcept {
            uint32_t r = (_rest >> 1) & 0x7FF;
            if (!r) {
                return 0;
            }
            if (r >= 729) {
                r -= 729;
                out[0] = char('0' + r / 100);
                out[1] = char('0' + r / 10 % 10);
                out[2] = char('0' + r % 10);
                return 3;
            }
            out[0] = char('A' - 1 + r / 27);
            out[1] = char('A' - 1 + r % 27);
            return 2;
        }

        uint32_t _language = 0;   // the letters, as subtag() gives them
        uint32_t _rest = 0;       // script << 12 | region << 1 | nu-latn
    };

    namespace detail::cldr {
        // What the tables of the module are keyed by: the packing of a
        // locale, and the index of the locale whose data it reads
        struct LocaleAccess {
            SGCL_INLINE_HOT static constexpr uint64_t key(const locale& l) noexcept {
                return (uint64_t(l._language) << 32) | (l._rest & ~uint32_t(1));
            }

            // the readings of text where it lies (a fuzzer's buffer): what
            // locale's, region's and script_code's constructors and parse read
            static locale parse(std::string_view tag) noexcept {
                locale l;
                l._parse(tag);
                return l;
            }

            static expected<region, code_error> parse_region(std::string_view s) noexcept {
                return region::_parse(s);
            }

            static expected<script_code, code_error> parse_script(std::string_view s) noexcept {
                return script_code::_parse(s);
            }

            SGCL_INLINE_HOT static constexpr locale make(uint64_t key, bool latin = false) noexcept {
                return locale(uint32_t(key >> 32), (uint32_t(key) & ~uint32_t(1)) | (latin ? 1 : 0));
            }

            SGCL_INLINE_HOT static constexpr uint32_t language(uint64_t key) noexcept {
                return uint32_t(key >> 32);
            }

            SGCL_INLINE_HOT static constexpr uint32_t script(uint64_t key) noexcept {
                return uint32_t(key) >> 12;
            }

            SGCL_INLINE_HOT static constexpr uint32_t region(uint64_t key) noexcept {
                return (uint32_t(key) >> 1) & 0x7FF;
            }

            SGCL_INLINE_HOT static constexpr uint64_t compose(uint32_t language, uint32_t script,
                                                              uint32_t region) noexcept {
                return (uint64_t(language) << 32) | (script << 12) | (region << 1);
            }
        };

        // The index (cldr_locales.h) of the data a locale reads: the
        // script filled from the region where the language's script
        // depends on it, the default script dropped, then the locale with
        // its region, without it, root
        // The key of a locale as the data's locales are keyed: the script
        // filled from the region where the language's script depends on
        // it, the default script dropped; 0 for a language without data
        inline uint64_t normalized_key(const locale& l) noexcept {
            uint64_t key = LocaleAccess::key(l);
            uint32_t language = LocaleAccess::language(key);
            size_t li = find(DataLanguages, language);
            if (li == std::size(DataLanguages)) {
                return 0;
            }
            uint32_t script = LocaleAccess::script(key);
            uint32_t region = LocaleAccess::region(key);
            if (!script && region) {
                size_t r = find(RegionScriptKeys, (uint64_t(language) << 32) | region);
                if (r != std::size(RegionScriptKeys)) {
                    script = RegionScripts[r];
                }
            }
            if (script == DefaultScripts[li]) {
                script = 0;
            }
            return LocaleAccess::compose(language, script, region);
        }

        inline uint16_t locale_index(const locale& l) noexcept {
            uint64_t key = normalized_key(l);
            if (!key) {
                return 0;
            }
            uint32_t language = LocaleAccess::language(key), script = LocaleAccess::script(key);
            uint32_t region = LocaleAccess::region(key);
            for (uint64_t k : {LocaleAccess::compose(language, script, region), LocaleAccess::compose(language, script, 0)}) {
                size_t i = find(LocaleKeys, k);
                if (i != std::size(LocaleKeys)) {
                    return LocaleKeyIndex[i];
                }
            }
            return 0;
        }

        inline uint32_t pack15(uint32_t language) noexcept {
            uint32_t v = 0;
            for (int shift = 16; shift >= 0; shift -= 8) {
                if (uint32_t c = (language >> shift) & 0xFF) {
                    v = (v << 5) | (c - 96);
                }
            }
            return v;
        }

        // The likely script and region of a language alone, 0 when CLDR
        // does not know it
        inline uint64_t likely_language(uint32_t language) noexcept {
            uint32_t key = pack15(language);
            size_t lo = 0, n = LikelyLanguageCount;
            while (n > 0) {
                size_t half = n / 2;
                if ((LikelyLanguages[lo + half] >> 17) < key) {
                    lo += half + 1;
                    n -= half + 1;
                } else {
                    n = half;
                }
            }
            if (lo >= LikelyLanguageCount) {
                return 0;
            }
            uint32_t w = LikelyLanguages[lo];
            if ((w >> 17) != key) {
                return 0;
            }
            return LocaleAccess::compose(language, LikelyScripts[(w >> 8) & 0xFF], LikelyRegions[w & 0xFF]);
        }

        inline uint64_t likely_pair(uint64_t key) noexcept {
            size_t i = find(LikelyKeys, key);
            return i == std::size(LikelyKeys) ? 0 : LikelyValues[i];
        }

        // TR35 §4.3, Add Likely Subtags: the first of language_script_region,
        // language_region, language_script, language, und_script that CLDR
        // has, its fields filling the ones the locale lacks
        inline uint64_t maximize(uint64_t key) noexcept {
            using A = LocaleAccess;
            uint32_t language = A::language(key), script = A::script(key), region = A::region(key);
            // Zzzz and ZZ, the unknown script and region, are none
            constexpr uint32_t Zzzz = (26u << 15) | (26u << 10) | (26u << 5) | 26u, ZZ = 26u * 27 + 26;
            if (script == Zzzz) {
                script = 0;
            }
            if (region == ZZ) {
                region = 0;
            }
            uint64_t found = 0;
            if (script && region) {
                found = likely_pair(A::compose(language, script, region));
            }
            if (!found && region) {
                found = likely_pair(A::compose(language, 0, region));
            }
            if (!found && script) {
                found = likely_pair(A::compose(language, script, 0));
            }
            if (!found) {
                found = language ? likely_language(language) : likely_pair(0);
            }
            if (!found && language && script) {
                found = likely_pair(A::compose(0, script, 0));
            }
            if (!found) {
                return key;
            }
            return A::compose(language ? language : A::language(found), script ? script : A::script(found),
                              region ? region : A::region(found));
        }

        inline uint64_t parent(uint64_t key) noexcept {
            using A = LocaleAccess;
            size_t i = find(ParentKeys, key);
            if (i != std::size(ParentKeys)) {
                return ParentValues[i];
            }
            uint32_t language = A::language(key), script = A::script(key), region = A::region(key);
            if (region) {
                return A::compose(language, script, 0);
            }
            if (script) {
                // a script the language is not usually written in has root
                // as its parent (parentLocales' nonlikelyScript)
                uint64_t likely = likely_language(language);
                if (A::script(likely) != script) {
                    return 0;
                }
                return A::compose(language, 0, 0);
            }
            return 0;
        }
    }

    inline locale locale::maximize() const noexcept {
        using A = detail::cldr::LocaleAccess;
        return A::make(detail::cldr::maximize(A::key(*this)), latin_digits());
    }

    // TR35 §4.3, Remove Likely Subtags: the shortest of language,
    // language_region, language_script that maximizes to the same
    inline locale locale::minimize() const noexcept {
        using A = detail::cldr::LocaleAccess;
        uint64_t max = detail::cldr::maximize(A::key(*this));
        uint32_t language = A::language(max);
        for (uint64_t trial : {A::compose(language, 0, 0), A::compose(language, 0, A::region(max)),
                               A::compose(language, A::script(max), 0)}) {
            if (detail::cldr::maximize(trial) == max) {
                return A::make(trial, latin_digits());
            }
        }
        return A::make(max, latin_digits());
    }

    // The chain of parents ends at the root locale itself, the key of the
    // digits dropped there, so that a loop to root ends
    inline locale locale::parent() const noexcept {
        using A = detail::cldr::LocaleAccess;
        uint64_t p = detail::cldr::parent(A::key(*this));
        return p ? A::make(p, latin_digits()) : locale();
    }

    inline string locale::autonym() const {
        uint16_t i = detail::cldr::locale_index(*this);
        if (i) {
            return string(detail::cldr::AutonymTexts[detail::cldr::Autonyms[i]]);
        }
        return language();
    }

    namespace detail::cldr {
        // The fields' distance of TR35 §4.4 (languageInfo.xml): the first
        // rule of the level whose fields match, both ways unless the rule
        // is one way
        inline bool in_variable(uint32_t region, uint32_t variable) noexcept {
            size_t lo = MatchVariableStart[variable], hi = MatchVariableStart[variable + 1];
            size_t i = lo + lower_bound(MatchVariableRegions + lo, hi - lo, uint16_t(region));
            return i < hi && MatchVariableRegions[i] == region;
        }

        inline bool field_matches(int level, uint32_t pattern, uint32_t value) noexcept {
            if (!pattern) {
                return true;
            }
            if (level == 2 && (pattern & 0x8000)) {
                bool in = in_variable(value, pattern & 0x3FFF);
                return (pattern & 0x4000) ? !in : in;
            }
            return pattern == value;
        }

        inline int rule_distance(int level, const uint32_t (&d)[3], const uint32_t (&s)[3]) noexcept {
            for (const auto& r : MatchRules) {
                if (int(r[0]) != level + 1) {
                    continue;
                }
                auto matches = [&](const uint32_t (&x)[3], const uint32_t (&y)[3], size_t xo, size_t yo) {
                    for (int k = 0; k <= level; ++k) {
                        if (!field_matches(k, r[xo + k], x[k]) || !field_matches(k, r[yo + k], y[k])) {
                            return false;
                        }
                    }
                    // a variable on both sides speaks of both regions at
                    // once: $!enUS against $!enUS is two regions outside
                    return true;
                };
                if (matches(d, s, 1, 4) || (!(r[7] & 256) && matches(s, d, 1, 4))) {
                    return int(r[7] & 255);
                }
            }
            return 80;
        }

        inline int distance(uint64_t desired, uint64_t supported) noexcept {
            using A = LocaleAccess;
            uint32_t d[3] = {A::language(desired), A::script(desired), A::region(desired)};
            uint32_t s[3] = {A::language(supported), A::script(supported), A::region(supported)};
            int total = 0;
            if (d[0] != s[0]) {
                total += rule_distance(0, d, s);
            }
            if (d[1] != s[1]) {
                total += rule_distance(1, d, s);
            }
            if (d[2] != s[2]) {
                int r = rule_distance(2, d, s);
                // a paradigm locale is a little closer than the others of
                // its group
                bool dp = find(ParadigmLocales, desired) != std::size(ParadigmLocales);
                bool sp = find(ParadigmLocales, supported) != std::size(ParadigmLocales);
                if (sp && !dp && r > 0) {
                    --r;
                }
                total += r;
            }
            return total;
        }

        inline size_t best_match(const locale* desired, size_t nd, const locale* supported, size_t ns) noexcept {
            using A = LocaleAccess;
            int best = 1 << 30;
            size_t chosen = 0;
            bool exact = false;
            for (size_t k = 0; k < nd; ++k) {
                uint64_t d = maximize(A::key(desired[k]));
                for (size_t j = 0; j < ns; ++j) {
                    int dist = distance(d, maximize(A::key(supported[j]))) + int(k) * 5;
                    // of equals, the first; but the supported locale that is
                    // the desired one exactly (en-US over en, which maximizes
                    // the same), as ICU has it
                    bool same = desired[k] == supported[j];
                    if (dist < best || (dist == best && same && !exact)) {
                        best = dist;
                        chosen = j;
                        exact = same;
                    }
                }
            }
            // a script or a language apart is no match: the default
            return best < 50 ? chosen : 0;
        }
    }

    // The supported locale closest to what the user wants (TR35 §4.4:
    // the distances of languageInfo.xml, each desired locale after the
    // first a little further), or the first supported one when none is
    // close enough — a different language or script; the root locale
    // when nothing is supported
    inline locale best_match(const slice<const locale>& desired, const slice<const locale>& supported) noexcept {
        if (supported.empty()) {
            return locale();
        }
        return supported[detail::cldr::best_match(desired.data(), desired.size(), supported.data(), supported.size())];
    }

    // The same for the value of an Accept-Language header (RFC 9110
    // §12.5.4): the languages in the order of their weights, those of
    // weight 0 and "*" left out; at most 32 are read
    namespace detail::cldr {
        inline locale best_match_text(std::string_view text, const slice<const locale>& supported) noexcept;
    }

    inline locale best_match(const string& accept_language, const slice<const locale>& supported) noexcept {
        return detail::cldr::best_match_text(accept_language.view(), supported);
    }

    // an Accept-Language header where it lies
    inline locale detail::cldr::best_match_text(std::string_view text, const slice<const locale>& supported) noexcept {
        if (supported.empty()) {
            return locale();
        }
        struct Weighted {
            locale where;
            int q;
        };
        Weighted list[32];
        size_t n = 0;
        size_t pos = 0;
        while (pos < text.size() && n < 32) {
            size_t end = text.find(',', pos);
            if (end == std::string_view::npos) {
                end = text.size();
            }
            std::string_view item = text.substr(pos, end - pos);
            pos = end + 1;
            int q = 1000;
            if (size_t semi = item.find(';'); semi != std::string_view::npos) {
                std::string_view param = item.substr(semi + 1);
                item = item.substr(0, semi);
                size_t eq = param.find('=');
                if (eq != std::string_view::npos) {
                    std::string_view v = param.substr(eq + 1);
                    while (!v.empty() && v.front() == ' ') {
                        v.remove_prefix(1);
                    }
                    q = 0;
                    int scale = 1000;
                    bool point = false;
                    for (char c : v) {
                        if (c == '.') {
                            point = true;
                        } else if (c >= '0' && c <= '9') {
                            if (!point) {
                                q = (c - '0') * 1000;
                            } else if (scale > 1) {
                                scale /= 10;
                                q += (c - '0') * scale;
                            }
                        } else {
                            break;
                        }
                    }
                }
            }
            while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) {
                item.remove_prefix(1);
            }
            while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) {
                item.remove_suffix(1);
            }
            if (q <= 0 || item.empty() || item == "*") {
                continue;
            }
            locale l;
            l = locale(string(item));
            if (l == locale()) {
                continue;
            }
            size_t at = n;
            while (at > 0 && list[at - 1].q < q) {
                list[at] = list[at - 1];
                --at;
            }
            list[at] = {l, q};
            ++n;
        }
        if (n == 0) {
            return supported[0];
        }
        locale desired[32];
        for (size_t i = 0; i < n; ++i) {
            desired[i] = list[i].where;
        }
        return supported[detail::cldr::best_match(desired, n, supported.data(), supported.size())];
    }

    namespace detail::names {
        // The registered tables of a locale's chain: its own, by its key
        // normalized as the data's locales are, else its CLDR parents'; a
        // Debug build says once when none is registered
        inline Chain chain_of(const locale& in) noexcept {
            Chain c;
            uint64_t key = cldr::normalized_key(in);
            for (uint64_t k = key; k; k = cldr::parent(k)) {
                if (const Table* t = table_of(k)) {
                    follow(c, t);
                    return c;
                }
            }
            if (in != locale()) {
                string tag = in.to_string();
                missing(cldr::LocaleAccess::key(in), tag.view());
            }
            return c;
        }

        // a pattern of {0} and {1}: "{0} ({1})", "{0}, {1}"
        inline string joined(std::string_view pattern, std::string_view a, std::string_view b) {
            std::string out;
            for (size_t i = 0; i < pattern.size();) {
                if (pattern.substr(i, 3) == "{0}") {
                    out += a;
                    i += 3;
                } else if (pattern.substr(i, 3) == "{1}") {
                    out += b;
                    i += 3;
                } else {
                    out += pattern[i++];
                }
            }
            return string(std::string_view(out));
        }

        // the first format of the chain that a table gives
        inline std::string_view chain_format(const Chain& c, uint32_t which, std::string_view otherwise) noexcept {
            for (uint32_t i = 0; i < c.n; ++i) {
                std::string_view v = format(*c.tables[i], which);
                if (!v.empty()) {
                    return v;
                }
            }
            return otherwise;
        }
    }

    inline bool locale::has_names() const noexcept {
        for (uint64_t k = detail::cldr::normalized_key(*this); k; k = detail::cldr::parent(k)) {
            if (detail::names::table_of(k)) {
                return true;
            }
        }
        return false;
    }

    inline string region::display_name(const locale& in) const {
        auto c = detail::names::chain_of(in);
        std::string_view v = detail::names::first_of(c, [&](const detail::names::Table& t) {
            return detail::names::region_name(t, _code);
        });
        return v.empty() ? code() : string(v);
    }

    inline string script_code::display_name(const locale& in) const {
        auto c = detail::names::chain_of(in);
        // its name standing alone where it has one ("Simplified Han"), as
        // ICU names a script alone
        std::string_view v = detail::names::first_of(c, [&](const detail::names::Table& t) {
            return detail::names::script_name(t, _code, true);
        });
        return v.empty() ? code() : string(v);
    }

    // The locale display name of TR35 §3.3 in its standard form, as ICU's
    // uloc_getDisplayName writes it: the language's name with the script's
    // and the region's in the locale pattern, "niemiecki (Szwajcaria)",
    // "serbski (łacińskie, Czarnogóra)"; codes for the parts without a name
    inline string locale::display_name(const locale& in) const {
        namespace N = detail::names;
        auto c = N::chain_of(in);
        uint32_t language = _language ? _language : _packed("und");
        uint32_t s = _rest >> 12, r = (_rest >> 1) & 0x7FF;
        bool with_script = s != 0, with_region = r != 0;
        std::string_view name = N::first_of(c, [&](const N::Table& t) {
            return N::language_name(t, 0, detail::cldr::pack15(language));
        });
        string base = name.empty() ? (_language ? _language_text() : string("und")) : string(name);
        if (!with_script && !with_region) {
            return base;
        }
        std::string_view pattern = N::chain_format(c, N::FmtPattern, "{0} ({1})");
        std::string_view separator = N::chain_format(c, N::FmtSeparator, "{0}, {1}");
        string inner;
        if (with_script) {
            // inside a locale's name the plain name: "chiński (tradycyjne, Tajwan)"
            std::string_view sn = N::first_of(c, [&](const N::Table& t) { return N::script_name(t, s); });
            if (sn.empty()) {
                txt::script_code sc;
                sc._code = s;
                inner = sc.code();
            } else {
                inner = string(sn);
            }
        }
        if (with_region) {
            txt::region rg;
            rg._code = uint16_t(r);
            string rn = rg.display_name(in);
            inner = inner.empty() ? rn : N::joined(separator, inner.view(), rn.view());
        }
        return N::joined(pattern, base.view(), inner.view());
    }
}

