//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// NFKC_Casefold is the compatibility decompositions and the full case
// folding put back together, so this header stands on both of those
#include "case.h"
#include "normalize.h"
#include "properties.h"
#include "detail/identifier_tables.h"

// What may be a name, and when two names that are not the same look the
// same. Three specifications meet here and they answer three different
// questions, which is why they are in one header and not in three:
//
//   UAX #31 says which code points may spell an identifier — a variable
//   of a language, a key of a configuration file, the label of a domain.
//
//   NFKC_Casefold (UAX #15) is the form two names are compared in when
//   the comparison must be blind to the way they were written, to their
//   case and to the compatibility forms at once. It is what a registry
//   folds a name to before it asks whether the name is taken.
//
//   UTS #39 is the security half: whether a code point is one that
//   belongs in a name at all (Identifier_Status), what is wrong with it
//   when it does not (Identifier_Type), which names look alike
//   (skeleton, confusable) and how many scripts a name is written in.
//
// What this header does NOT do is decide anything for the caller. It
// reports; whoever registers the name decides. The limits of the reports
// are written down beside each one and on the page, because a security
// check whose limits are not stated is worse than none: it is trusted
// further than it reaches.
namespace sgcl::txt {
    using detail::identifier_type;

    // The profile of UAX #31 §2.3 that every parser of a programming
    // language wants: the underscore begins an identifier (XID_Start has
    // it in neither position, XID_Continue has it in the second) and the
    // dollar sign stands anywhere in one, which is what C, C++, Java,
    // JavaScript and the shells all do. A tag and not a flag, so that the
    // walk has no runtime branch and the call site reads as the enum
    // would: is_identifier(name, program_syntax).
    struct program_syntax_t {};
    inline constexpr program_syntax_t program_syntax {};

    // Identifier_Status of UTS #39: whether a code point is one the
    // specification would let into a name at all. Restricted is the
    // default — a code point nobody has argued for is not allowed
    enum class identifier_status : uint8_t {
        restricted,
        allowed,
    };

    // The ladder of UTS #39 §5.2, from the narrowest to the widest. A
    // caller picks the rung it will accept and refuses what is above it;
    // the specification recommends moderately_restrictive for a general
    // registry and highly_restrictive where more is at stake
    enum class restriction_level : uint8_t {
        ascii_only,
        single_script,
        highly_restrictive,
        moderately_restrictive,
        minimally_restrictive,
        unrestricted,
    };

    namespace detail {
        // XID_Start and XID_Continue, not ID_Start and ID_Continue. The
        // X is the whole point: the plain pair is not closed under
        // normalization, so a text that is an identifier can stop being
        // one when it is put into NFKC. U+037A GREEK YPOGEGRAMMENI is
        // ID_Start and normalizes to a space and an iota; U+309B
        // KATAKANA-HIRAGANA VOICED SOUND MARK does the same; U+0E33 THAI
        // CHARACTER SARA AM normalizes to a mark and a vowel, and a mark
        // may not begin a name. Twenty-three code points are ID_Start
        // and not XID_Start and nineteen are ID_Continue and not
        // XID_Continue, and they are exactly the ones that would break
        // that way. Taking them out is what lets a compiler compare two
        // names in a normal form and a linker carry one.
        constexpr bool xid_start_fn(char32_t c) noexcept {
            if (c < 0x80) {
                return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
            }
            return in_set(c, identifier_tables::XidStart);
        }

        constexpr bool xid_continue_fn(char32_t c) noexcept {
            if (c < 0x80) {
                return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z')
                       || (c >= U'0' && c <= U'9') || c == U'_';
            }
            return in_set(c, identifier_tables::XidContinue);
        }

        constexpr joining_type joining_type_of_fn(char32_t c) noexcept {
            return joining_type(value_of(c, identifier_tables::JoiningType));
        }

        constexpr identifier_status identifier_status_of_fn(char32_t c) noexcept {
            return in_set(c, identifier_tables::IdentifierAllowed) ? identifier_status::allowed
                                                                   : identifier_status::restricted;
        }

        // A code point the file names carries a set of the eleven
        // values; one it does not name is Not_Character, which is the
        // empty set and the value the table answers with zero
        constexpr identifier_type identifier_type_of_fn(char32_t c) noexcept {
            return identifier_type(value_of(c, identifier_tables::IdentifierType));
        }

        // The question with the profile as a second argument rather than
        // as a second name: one object a question, still callable with a
        // code point alone and so still passable where a predicate is
        // asked for, and still refusing a char and an int
        template<bool (*F)(char32_t)>
        struct identifier_fn {
            constexpr bool operator()(char32_t c) const noexcept {
                return F(c);
            }

            constexpr bool operator()(char32_t c, program_syntax_t) const noexcept {
                return F(c) || c == U'$' || c == U'_';
            }

            template<class T, class... Rest> requires (!std::same_as<T, char32_t>)
            constexpr bool operator()(T, Rest...) const noexcept = delete;
        };

        // Rule UAX31-R1a. A zero width joiner and a zero width
        // non-joiner are formatting code points and are in neither XID
        // set, and the Indic languages cannot be written without them:
        // "क्ष" is a conjunct and "क्‍ष" with a joiner in it is not, and
        // the two are different words. The rule lets them in where they
        // do the work they exist for and nowhere else, which is the same
        // line RFC 5892 draws for domain names.
        //
        // A joiner is allowed after a virama — a mark of combining class
        // 9, the sign that kills the vowel of the consonant before it.
        constexpr bool after_virama(std::string_view text, size_t at) noexcept {
            if (at == 0) {
                return false;
            }
            size_t i = at;
            auto [c, n] = utf8::decode_last(text, i);
            (void)n;
            return ccc_fn(c) == 9;
        }

        // A non-joiner is allowed after a virama as well, and also where
        // it breaks a cursive join that would otherwise happen: between
        // a letter that joins to the left and one that joins to the
        // right, with only transparent code points (the marks) between
        constexpr bool breaks_a_join(std::string_view text, size_t at, size_t after) noexcept {
            bool left = false;
            for (size_t i = at; i > 0;) {
                auto [c, n] = utf8::decode_last(text, i);
                i -= n;
                auto j = joining_type_of_fn(c);
                if (j == joining_type::t) {
                    continue;
                }
                left = j == joining_type::l || j == joining_type::d;
                break;
            }
            if (!left) {
                return false;
            }
            for (size_t i = after; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                auto j = joining_type_of_fn(c);
                if (j == joining_type::t) {
                    continue;
                }
                return j == joining_type::r || j == joining_type::d;
            }
            return false;
        }

        constexpr bool joiner_in_context(std::string_view text, size_t at, size_t after, char32_t c) noexcept {
            if (after_virama(text, at)) {
                return true;
            }
            return c == 0x200C && breaks_a_join(text, at, after);
        }

        template<bool Program>
        constexpr bool identifier_text(std::string_view text) noexcept {
            if (text.empty()) {
                return false;
            }
            bool first = true;
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                size_t at = i;
                i += n;
                if (c == 0x200C || c == 0x200D) {
                    // a joiner is neither a start nor a continue: it
                    // stands between two letters and never begins a name
                    if (first || !joiner_in_context(text, at, i, c)) {
                        return false;
                    }
                    continue;
                }
                bool ok = first ? xid_start_fn(c) : xid_continue_fn(c);
                if constexpr (Program) {
                    ok = ok || c == U'$' || (first && c == U'_');
                }
                if (!ok) {
                    return false;
                }
                first = false;
            }
            return true;
        }

        // NFKC_Casefold, and no table of its own. The mapping of
        // DerivedNormalizationProps.txt is 91.4 KB of prototypes, and
        // every one of them is NFC(fold(NFKD(c))) with the default
        // ignorable code points dropped — all 1114112 of them, which the
        // generator asserts before it writes the tables. So the mapping
        // is worked out from the compatibility decompositions and the
        // full case folding the module carries anyway, and what is kept
        // is the set of code points it drops (0.6 KB) and the set it
        // changes (2.3 KB, for the quick check below).
        //
        // The order matters and is not the order one would guess. The
        // text is NOT decomposed as a whole first: U+0345 COMBINING
        // GREEK YPOGEGRAMMENI has combining class 240 and folds to
        // U+03B9 GREEK SMALL LETTER IOTA, which has none, so decomposing
        // the whole text would sort it past the marks that follow it and
        // the fold would then leave it there. Each code point is taken
        // apart on its own, folded, and only then is the whole buffer put
        // in canonical order and composed — which is where the standard
        // draws the same line, its mapping being per code point with one
        // NFC at the end.
        inline void nfkc_casefold_points(vector<char32_t>& out, std::string_view text) {
            vector<char32_t> taken;
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                if (in_set(c, identifier_tables::DefaultIgnorable)) {
                    continue;
                }
                taken.clear();
                decompose_into<true>(taken, c);
                for (auto x : taken) {
                    // the folding of a decomposed code point is itself
                    // decomposed, and neither a decomposition nor a
                    // folding of a code point that is not ignorable ever
                    // brings an ignorable one in — the generator asserts
                    // both, so nothing here is taken apart or dropped a
                    // second time
                    if (auto d = full_of(x, case_tables::FullFold)) {
                        append(out, d);
                    } else {
                        out.push_back(unicode::to_lower(x));
                    }
                }
            }
            canonical_order(out);
            compose_buffer(out);
        }

        // Whether the text is in NFKC_Casefold already, by two
        // properties and no folding: the mapping changes the text if it
        // changes any one of its code points, and what is left over is
        // NFC, which the quick check properties of the normalization
        // answer. False settles it; true with `maybe` set does not, and
        // the caller has to fold and compare.
        inline bool nfkc_casefold_quick(std::string_view text, bool& maybe) noexcept {
            uint8_t last = 0;
            maybe = false;
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                if (in_set(c, identifier_tables::ChangesWhenNfkcCasefolded)) {
                    return false;
                }
                uint8_t cc = ccc_fn(c);
                if (last > cc && cc != 0) {
                    return false;                  // the marks are out of order
                }
                unsigned q = quick_check(c, form_index(nfc));
                if (q == QuickCheckNo) {
                    return false;
                }
                maybe = maybe || q == QuickCheckMaybe;
                last = cc;
            }
            return true;
        }

        // How many distinct scripts are held on the stack while a text
        // is walked. A name is written in one script or two; the widest
        // set UTS #39 names is four — Latin, Han, Hiragana and Katakana,
        // which is Japanese — and eight is twice that, so nothing
        // anybody would call a name comes near it. The number is on the
        // stack and not in a container because the container was the
        // whole cost of the question: is_single_script took longer over
        // an ASCII name than over a Polish one, which is not the shape
        // of a walk that decodes a code point and reads a table.
        inline constexpr size_t scripts_held = 8;

        // The scripts a text is written in. Common (the digits, the
        // punctuation) and Inherited (the marks) belong to every script
        // and so constrain nothing: a text of nothing but those is
        // single script by the specification's reckoning, and a mark
        // after a Cyrillic letter does not make a second script.
        //
        // What does not fit is not dropped. A ninth distinct script sets
        // `more` and stops the walk, and the two callers read that as
        // what it is rather than as a count they can use: nine scripts
        // is not one script (so not single_script), it is not any of the
        // three sets of Table 2, which have four members at their widest
        // (so not highly_restrictive), and it is not Latin with one
        // other (so not moderately_restrictive). Every rung of the
        // ladder is settled by the fact of the overflow, so there is
        // nothing to walk a second time for — a truncated set would be
        // wrong, this is not a truncated set.
        inline size_t scripts_of_text(std::string_view text, script* out, size_t room, bool& more) noexcept {
            size_t count = 0;
            more = false;
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                i += n;
                auto s = script_of_fn(c);
                if (s == script::common || s == script::inherited || s == script::unknown) {
                    continue;
                }
                bool seen = false;
                for (size_t k = 0; k < count; ++k) {
                    seen = seen || out[k] == s;
                }
                if (seen) {
                    continue;
                }
                if (count == room) {
                    more = true;
                    return count;
                }
                out[count++] = s;
            }
            return count;
        }

        // The three sets of UTS #39 Table 2 that are one writing system
        // although they are more than one script: Japanese, Chinese and
        // Korean. Latin is allowed beside each of them, since every one
        // of the three writes Latin words as they are
        inline bool covered_by_a_writing_system(const script* scripts, size_t count) noexcept {
            bool japanese = true, chinese = true, korean = true;
            for (size_t i = 0; i < count; ++i) {
                script s = scripts[i];
                bool latin_or_han = s == script::latin || s == script::han;
                japanese = japanese && (latin_or_han || s == script::hiragana || s == script::katakana);
                chinese = chinese && (latin_or_han || s == script::bopomofo);
                korean = korean && (latin_or_han || s == script::hangul);
            }
            return japanese || chinese || korean;
        }
    }

    // Whether a code point may begin an identifier and whether it may
    // stand further along in one: XID_Start and XID_Continue of UAX #31.
    // Each also takes the programmer's profile as a second argument —
    // is_identifier_start(c, program_syntax) — and each is still an
    // object rather than a function, so that it is passable where a
    // predicate is asked for: s.runes().all_of(txt::is_identifier_continue)
    inline constexpr detail::identifier_fn<detail::xid_start_fn> is_identifier_start {};
    inline constexpr detail::identifier_fn<detail::xid_continue_fn> is_identifier_continue {};

    // Whether the whole text is an identifier: rule UAX31-R1, a start
    // followed by continues, with rule R1a for the two joiners. An empty
    // text is not one. Nothing here says the name is a good idea — for
    // that, ask restriction_level_of below.
    inline bool is_identifier(const string& text) {
        return detail::identifier_text<false>(text.view());
    }

    inline bool is_identifier(const string& text, program_syntax_t) {
        return detail::identifier_text<true>(text.view());
    }

    // The form a name is compared in when the comparison must not care
    // how the name was written, what case it was written in, or whether
    // a compatibility form was used: "ＦＵＬＬ" and "full" fold to one
    // text, "ﬁle" and "file" fold to one, and so do "é" written as one
    // code point and as two. It is what UTS #46 folds a domain label to
    // and what UAX #31 recommends a compiler compare identifiers by.
    //
    // It is not for showing to anybody: the mapping cannot be undone, it
    // loses the case and it loses the difference between a ligature and
    // the letters in it. Fold to compare and keep the original to print.
    inline string nfkc_casefold(const string& text) {
        auto v = text.view();
        // over ASCII the whole mapping is the one bit of the case: the
        // only ASCII code points it touches are A to Z, and what comes
        // out is ASCII and so already composed
        if (utf8::all_ascii(v)) {
            return detail::ascii_cased<'A', 'Z', 32>(text);
        }
        // and a text that is in the form already comes back as the
        // object it came in as, which is what most names that arrive
        // are: the quick check costs 14.4 ns over a name where the fold
        // costs 100, and a string of the library is shared by copying,
        // so nothing is allocated for it
        bool maybe = false;
        if (detail::nfkc_casefold_quick(v, maybe) && !maybe) {
            return text;
        }
        vector<char32_t> out;
        out.reserve(v.size());
        detail::nfkc_casefold_points(out, v);
        auto made = detail::encoded(out);
        return made == text ? text : made;
    }

    // Whether the text is in that form already, which is the question
    // asked of every name that arrives. Two properties answer it without
    // folding anything: the text changes under the mapping if any of its
    // code points does, and what is left is NFC, which the quick check
    // properties of the normalization answer. Only a "maybe" from those
    // costs the fold.
    inline bool is_nfkc_casefolded(const string& text) {
        bool maybe = false;
        if (!detail::nfkc_casefold_quick(text.view(), maybe)) {
            return false;
        }
        return !maybe || nfkc_casefold(text) == text;
    }

    // Identifier_Status and Identifier_Type of UTS #39: whether a code
    // point belongs in a name, and what is wrong with it when it does
    // not — deprecated, technical, obsolete, an exclusion, a
    // compatibility form. The type is a set of those and not one of
    // them, so it is read with the & operator: (type_of(c) &
    // identifier_type::technical) != identifier_type::not_character
    inline constexpr sgcl::detail::code_point_fn<detail::identifier_status_of_fn> identifier_status_of {};
    inline constexpr sgcl::detail::code_point_fn<detail::identifier_type_of_fn> identifier_type_of {};

    // Whether every code point of the text is one UTS #39 allows in an
    // identifier. It says nothing about the scripts they are written in
    inline bool is_allowed_identifier(const string& text) {
        auto v = text.view();
        for (size_t i = 0; i < v.size();) {
            auto [c, n] = utf8::decode(v, i);
            i += n;
            if (detail::identifier_status_of_fn(c) != identifier_status::allowed) {
                return false;
            }
        }
        return !v.empty();
    }

    // The skeleton of UTS #39 §4: the text with every code point
    // replaced by the one it is confusable with, decomposed on the way
    // in and on the way out. Two texts a reader could mistake for one
    // another have the same skeleton — "раypal" written with a Cyrillic
    // а and р has the skeleton of "paypal", and so does "pаypaI" with a
    // capital i for the l.
    //
    // The skeleton is not text. It is a key to compare by and to look up
    // in a table of the names already taken; it is not to be shown, and
    // no reader should ever see one.
    inline string skeleton(const string& text) {
        auto points = detail::normalized_points(text.view(), nfd);
        // The second decomposition of the three steps is folded into
        // the mapping: a code point the table does not name came out of
        // the first one decomposed already, and a prototype is taken
        // apart where it is written rather than on a pass of its own —
        // two buffers instead of three, and 210 ns over a short ASCII
        // name where the three passes cost 338
        vector<char32_t> out;
        out.reserve(points.size());
        vector<char32_t> prototype;
        for (auto c : points) {
            auto d = detail::decomposition_of(c, detail::identifier_tables::Confusables);
            if (!d) {
                out.push_back(c);
                continue;
            }
            prototype.clear();
            detail::append(prototype, d);
            for (auto x : prototype) {
                detail::decompose_into<false>(out, x);
            }
        }
        detail::canonical_order(out);
        auto made = detail::encoded(out);
        return made == text ? text : made;
    }

    // Whether two texts look alike, by their skeletons. What this
    // catches is the whole of what the table of UTS #39 knows and
    // nothing more: it is one font's judgement of what looks like what,
    // and it says nothing about a name that is merely similar ("rn" for
    // "m" is in the table, "1" for "l" is, "paypa1" against "paypal" is
    // caught; "paypaI-inc" against "paypal" is not, being a different
    // name rather than the same one written differently)
    inline bool is_confusable(const string& a, const string& b) {
        return a == b || skeleton(a) == skeleton(b);
    }

    // Whether the text is written in one script, Common and Inherited
    // aside — the digits, the punctuation and the marks belong to every
    // script and so do not make a second one. This is the question §5.1
    // asks, and it is the one that catches a name half in Latin and half
    // in Cyrillic, which is how nearly every attack on a name is built.
    //
    // It is the Script property and not Script_Extensions: a code point
    // that is Common although it is used by only two scripts — the
    // Japanese prolonged sound mark is the one everybody meets — counts
    // here as belonging to all of them. That makes the answer more
    // generous than the specification's, never less, so a text this
    // calls single script may be two by Script_Extensions.
    inline bool is_single_script(const string& text) {
        script scripts[detail::scripts_held];
        bool more = false;
        size_t count = detail::scripts_of_text(text.view(), scripts, detail::scripts_held, more);
        // more than the stack holds is more than one, whichever they are
        return !more && count <= 1;
    }

    // Where the text stands on the ladder of UTS #39 §5.2. A caller
    // names the rung it accepts; the specification suggests
    // moderately_restrictive for a registry open to the world and
    // highly_restrictive where a mistaken name costs something.
    inline restriction_level restriction_level_of(const string& text) {
        auto v = text.view();
        bool ascii = true;
        for (size_t i = 0; i < v.size();) {
            auto [c, n] = utf8::decode(v, i);
            i += n;
            if (detail::identifier_status_of_fn(c) != identifier_status::allowed) {
                return restriction_level::unrestricted;
            }
            ascii = ascii && c < 0x80;
        }
        if (v.empty()) {
            return restriction_level::unrestricted;
        }
        if (ascii) {
            return restriction_level::ascii_only;
        }
        script scripts[detail::scripts_held];
        bool more = false;
        size_t count = detail::scripts_of_text(v, scripts, detail::scripts_held, more);
        // A ninth script settles every rung below the last one: it is
        // not one script, it is not one of the three sets of Table 2,
        // which are four at their widest, and it is not Latin with one
        // other. Every code point is allowed — the loop above said so —
        // so what is left is the rung where the scripts mix freely
        if (more) {
            return restriction_level::minimally_restrictive;
        }
        if (count <= 1) {
            return restriction_level::single_script;
        }
        if (detail::covered_by_a_writing_system(scripts, count)) {
            return restriction_level::highly_restrictive;
        }
        // Latin and one other, and that other not one of the three whose
        // letters are the ones Latin is mistaken for. Every script that
        // gets this far is a recommended one: the characters of the
        // limited use scripts are Restricted, and the loop above has
        // already turned those away
        if (count == 2) {
            bool latin = scripts[0] == script::latin || scripts[1] == script::latin;
            auto other = scripts[0] == script::latin ? scripts[1] : scripts[0];
            if (latin && other != script::latin && other != script::cyrillic
                && other != script::greek && other != script::cherokee) {
                return restriction_level::moderately_restrictive;
            }
        }
        return restriction_level::minimally_restrictive;
    }

    // The two rungs a caller asks for by name
    inline bool is_highly_restrictive(const string& text) {
        return restriction_level_of(text) <= restriction_level::highly_restrictive;
    }

    inline bool is_moderately_restrictive(const string& text) {
        return restriction_level_of(text) <= restriction_level::moderately_restrictive;
    }
}
