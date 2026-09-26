//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// The bidirectional classes of RFC 5893 are the ones UAX #9 gives, so the
// rule reads them out of that header rather than carrying a second copy;
// bidi.h brings normalize.h and properties.h with it
#include "bidi.h"
// the full case folding, which is half of the mapping: UTS #46 maps
// what NFKC_Casefold maps, and identifier.h works that out rather than
// tabling it
#include "case.h"
#include "detail/idna_tables.h"

#include "../core/expected.h"

#include <algorithm>
#include <bit>
#include <string>
#include <vector>

// The name of a host, written in somebody's own script and written the
// way the DNS carries it.
//
// This is text work and not network work, which is why it lives here: what
// "bücher.de" is called in the DNS follows from case, from normalization
// and from the bidirectional classes, and all three are in this module
// already. A socket never sees any of it — it is given the ASCII the name
// turns into, and hands back the ASCII a lookup answered.
//
// Two things, the second built on the first:
//
//   punycode   one label between Unicode and ASCII (RFC 3492)
//   idna       a whole domain name, both ways (UTS #46, RFC 5890-5894)
//
// The other half of the text of a URL — the bytes of a path, escaped as
// %XX — is percent.h, which shares nothing with this: a different
// specification, no tables, and nobody looking for it would open a file
// called idna.
//
// Here, unlike anywhere else in the module, an operation can really fail.
// A name may hold a character no domain name may hold, a label may be
// longer than the 63 bytes the DNS allows, a punycode label may be
// nonsense. None of that is a text to be repaired the way an invalid byte
// becomes U+FFFD: a name that is wrong must not be looked up, so the
// answer is an expected and the caller cannot walk past it.
namespace sgcl::txt {
    namespace detail {
        //----------------------------------------------------------------
        // punycode (RFC 3492)
        //----------------------------------------------------------------
        // The parameters the RFC fixes for the "punycode" profile. They
        // are not a choice: a decoder that used others would read a
        // different string out of the same label.
        inline constexpr uint32_t PunyBase = 36;
        inline constexpr uint32_t PunyTMin = 1;
        inline constexpr uint32_t PunyTMax = 26;
        inline constexpr uint32_t PunySkew = 38;
        inline constexpr uint32_t PunyDamp = 700;
        inline constexpr uint32_t PunyInitialBias = 72;
        inline constexpr char32_t PunyInitialN = 128;

        // Every counter of the algorithm is a 32 bit unsigned in the RFC,
        // and the RFC says in as many words that an implementation must
        // detect the overflow rather than let it wrap (section 6.4). A
        // wrap is not a wrong answer that a test would notice, it is a
        // decoder that writes a code point somebody else chose: this is
        // where the security advisories against punycode implementations
        // have been. So the arithmetic is done 64 bits wide, which cannot
        // itself overflow at these magnitudes, and every step is compared
        // against the 32 bit bound before it is taken.
        inline constexpr uint64_t PunyMaxInt = 0xFFFFFFFF;

        constexpr uint32_t puny_adapt(uint64_t delta, uint32_t points, bool first) noexcept {
            delta = first ? delta / PunyDamp : delta / 2;
            delta += delta / points;
            uint32_t k = 0;
            while (delta > ((PunyBase - PunyTMin) * PunyTMax) / 2) {
                delta /= PunyBase - PunyTMin;
                k += PunyBase;
            }
            return k + uint32_t(((PunyBase - PunyTMin + 1) * delta) / (delta + PunySkew));
        }

        constexpr uint32_t puny_threshold(uint32_t k, uint32_t bias) noexcept {
            return k <= bias ? PunyTMin : (k >= bias + PunyTMax ? PunyTMax : k - bias);
        }

        // The digits are the letters and then the numbers, and a decoder
        // takes either case: a label may have passed through something
        // that upper cased it
        constexpr int puny_digit(char c) noexcept {
            if (c >= 'a' && c <= 'z') {
                return c - 'a';
            }
            if (c >= 'A' && c <= 'Z') {
                return c - 'A';
            }
            if (c >= '0' && c <= '9') {
                return c - '0' + 26;
            }
            return -1;
        }

        constexpr char puny_char(uint32_t d) noexcept {
            return char(d < 26 ? 'a' + d : '0' + (d - 26));
        }

        // Section 6.3. The basic code points come out as they are, then
        // the rest as deltas over an integer that only ever grows.
        inline bool puny_encode(const vector<char32_t>& in, std::string& out) {
            size_t basic = 0;
            for (auto c : in) {
                if (c < 0x80) {
                    out.push_back(char(c));
                    ++basic;
                }
            }
            if (basic) {
                out.push_back('-');
            }
            uint64_t delta = 0;
            uint32_t bias = PunyInitialBias;
            char32_t n = PunyInitialN;
            size_t h = basic;
            while (h < in.size()) {
                char32_t m = 0x110000;
                for (auto c : in) {
                    if (c >= n && c < m) {
                        m = c;
                    }
                }
                if (m == 0x110000) {
                    return false;               // nothing left to take: the input is not code points
                }
                // the one multiplication of the algorithm, and the one
                // place a long label with a high code point in it really
                // can run past 2^32
                if (uint64_t(m - n) > (PunyMaxInt - delta) / (h + 1)) {
                    return false;
                }
                delta += uint64_t(m - n) * (h + 1);
                n = m;
                for (auto c : in) {
                    if (c < n) {
                        if (++delta > PunyMaxInt) {
                            return false;
                        }
                    } else if (c == n) {
                        uint64_t q = delta;
                        for (uint32_t k = PunyBase; ; k += PunyBase) {
                            uint32_t t = puny_threshold(k, bias);
                            if (q < t) {
                                break;
                            }
                            out.push_back(puny_char(uint32_t(t + (q - t) % (PunyBase - t))));
                            q = (q - t) / (PunyBase - t);
                        }
                        out.push_back(puny_char(uint32_t(q)));
                        bias = puny_adapt(delta, uint32_t(h + 1), h == basic);
                        delta = 0;
                        ++h;
                    }
                }
                if (++delta > PunyMaxInt) {
                    return false;
                }
                ++n;
            }
            return true;
        }

        // Section 6.2, and the one that is given somebody else's bytes.
        // Every addition and every multiplication is bounded before it is
        // made, and the code point that comes out is checked to be one:
        // the RFC's own decoder lets n run to 2^32 and leaves it to the
        // caller to notice.
        inline bool puny_decode(std::string_view text, vector<char32_t>& out) {
            size_t at = 0;
            // "consume all code points before the last delimiter, and if
            // more than zero were consumed then consume one more". A
            // hyphen at the very front is not a delimiter, then: nothing
            // stood before it, so it is read as a digit and is not one.
            // That is the difference between "xn--", whose rest is empty
            // and decodes to the empty label, and "xn---", whose rest is
            // a lone hyphen and does not decode at all
            if (size_t last = text.rfind('-'); last != std::string_view::npos && last > 0) {
                for (size_t k = 0; k < last; ++k) {
                    if (uint8_t(text[k]) >= 0x80) {
                        return false;           // a basic code point is ASCII by definition
                    }
                    out.push_back(char32_t(uint8_t(text[k])));
                }
                at = last + 1;
            }
            char32_t n = PunyInitialN;
            uint64_t i = 0;
            uint32_t bias = PunyInitialBias;
            // Where each code point is to go, gathered first and placed
            // afterwards; see the note where they are placed. A label of
            // the length the DNS allows never leaves the stack.
            struct Place { uint32_t at; char32_t point; };
            constexpr size_t SmallLabel = 64;
            Place small[SmallLabel];
            std::vector<Place> spill;
            size_t made = 0;
            while (at < text.size()) {
                uint64_t old = i;
                uint64_t w = 1;
                for (uint32_t k = PunyBase; ; k += PunyBase) {
                    if (at >= text.size()) {
                        return false;           // the digits stop in the middle of a number
                    }
                    int d = puny_digit(text[at++]);
                    if (d < 0) {
                        return false;
                    }
                    if (uint64_t(d) > (PunyMaxInt - i) / w) {
                        return false;
                    }
                    i += uint64_t(d) * w;
                    uint32_t t = puny_threshold(k, bias);
                    if (uint32_t(d) < t) {
                        break;
                    }
                    if (w > PunyMaxInt / (PunyBase - t)) {
                        return false;
                    }
                    w *= PunyBase - t;
                }
                // The count is what the sequence would hold if the
                // placements had been made as they were decoded, which
                // is what the RFC counts; it is not out.size() any more,
                // since nothing is placed until the loop is over
                uint32_t points = uint32_t(out.size() + made + 1);
                bias = puny_adapt(i - old, points, old == 0);
                if (i / points > PunyMaxInt - n) {
                    return false;
                }
                n += char32_t(i / points);
                i %= points;
                if (n > 0x10FFFF || (n >= 0xD800 && n < 0xE000)) {
                    return false;               // not a code point a text can hold
                }
                if (made < SmallLabel) {
                    small[made] = {uint32_t(i), n};
                } else {
                    if (made == SmallLabel) {
                        spill.assign(small, small + SmallLabel);
                    }
                    spill.push_back({uint32_t(i), n});
                }
                ++made;
                ++i;
            }
            // The code points are placed now rather than as they are
            // decoded, and that is not a detail of style. RFC 3492 says
            // to insert each one at an index of the sequence as it
            // stands, which over a sequence held in one run of memory is
            // a move of everything after it: quadratic in the length,
            // and reachable from outside, since a name arrives from
            // whoever is asking. A label of forty thousand points cost
            // 70 ms, and four times that at every doubling.
            //
            // For a label of any real length the road is the one the RFC
            // draws, straight into the caller's vector, since nothing is
            // cheaper than sixty-three moves of a handful of words. Past
            // that the placements are done backwards instead: the last
            // one is where it will stay, and every earlier one goes to
            // the k-th place still empty, counted over a tree of counts
            // — the same order, in n log n rather than n squared. The
            // two roads are held to each other by a test.
            if (spill.empty()) {
                for (size_t k = 0; k < made; ++k) {
                    out.insert(out.begin() + small[k].at, small[k].point);
                }
                return true;
            }
            size_t base = out.size();
            size_t whole = base + spill.size();
            std::vector<uint32_t> counts(whole + 1, 0);       // a Fenwick tree of empty places
            for (size_t k = 1; k <= whole; ++k) {
                counts[k] += 1;
                if (size_t up = k + (k & (~k + 1)); up <= whole) {
                    counts[up] += counts[k];
                }
            }
            auto take = [&](size_t k) {                        // the (k+1)-th place still empty
                size_t at = 0;
                size_t step = std::bit_floor(whole ? whole : size_t(1));
                uint32_t left = uint32_t(k) + 1;
                for (; step; step >>= 1) {
                    if (at + step <= whole && counts[at + step] < left) {
                        at += step;
                        left -= counts[at];
                    }
                }
                for (size_t up = at + 1; up <= whole; up += up & (~up + 1)) {
                    counts[up] -= 1;
                }
                return at;                                    // zero based
            };
            vector<char32_t> placed(whole, char32_t(0));
            for (size_t k = spill.size(); k-- > 0;) {
                placed[take(spill[k].at)] = spill[k].point;
            }
            // What stood before the delimiter goes into the places
            // still empty, in the order it was written: each one asks
            // for the first of them, since taking it leaves the next
            for (size_t k = 0; k < base; ++k) {
                placed[take(0)] = out[k];
            }
            out = placed;
            return true;
        }
    }

    // One label, between the two ways of writing it. Neither takes or
    // gives the "xn--" prefix: that belongs to IDNA, which is what knows
    // whether a label has one, and punycode itself is only the encoding.
    //
    // Nothing comes back when the label cannot be written that way at
    // all: for encode, a delta past the 2^32 the RFC gives it, which a
    // label of some thousands of characters with a supplementary code
    // point in it reaches; for decode, a digit that is not one, a number
    // that stops in the middle, a counter run past its bound, or a code
    // point that no text can hold.
    namespace punycode {
        inline optional<string> encode(const string& label) {
            vector<char32_t> points;
            auto v = label.view();
            points.reserve(v.size());
            for (size_t i = 0; i < v.size();) {
                auto [c, n] = utf8::decode(v, i);
                points.push_back(c);
                i += n;
            }
            std::string out;
            if (!detail::puny_encode(points, out)) {
                return nullopt;
            }
            return string(out.data(), out.size());
        }

        inline optional<string> decode(const string& label) {
            vector<char32_t> points;
            if (!detail::puny_decode(label.view(), points)) {
                return nullopt;
            }
            return detail::encoded(points);
        }
    }

    //--------------------------------------------------------------------
    // IDNA (UTS #46, RFC 5890-5894)
    //--------------------------------------------------------------------
    namespace idna {
        // Which rule the name broke. One name can break several at once
        // and this is the first of them; the numbers in the comments are
        // the steps of UTS #46 and the codes IdnaTestV2.txt writes, so
        // that a failure can be read against the standard.
        enum class error : uint8_t {
            none,
            disallowed,           // V7: a code point no domain name may hold
            not_normalized,       // V1: a decoded label that is not NFC
            hyphen,               // V2, V3: "--" in the third and fourth place, or a hyphen at an end
            label_prefix,         // V4: "xn--" where the hyphens are not being checked
            label_separator,      // V5: a full stop inside a label
            leading_combining,    // V6: a label beginning with a mark
            joiner,               // C1, C2: a zero width joiner where the script does not call for one
            bidi,                 // B1-B6: RFC 5893, in a name that runs right to left
            std3,                 // U1: an ASCII character that is not a letter, a digit or a hyphen
            punycode,             // P4, A3: an "xn--" label that is not punycode
            empty_label,          // X4_2, A4_2: a label with nothing in it
            label_too_long,       // A4_2: more than 63 bytes
            name_too_long,        // A4_1: more than 253 bytes
        };

        // A line for a log or a message. Not a sentence: the caller
        // writes the sentence, this says which rule it was
        constexpr const char* message_of(error e) noexcept {
            switch (e) {
                case error::none: return "no error";
                case error::disallowed: return "a code point that is not allowed in a domain name";
                case error::not_normalized: return "a label that is not in normalization form C";
                case error::hyphen: return "a hyphen in the third and fourth place, or at an end";
                case error::label_prefix: return "a label beginning with xn--";
                case error::label_separator: return "a full stop inside a label";
                case error::leading_combining: return "a label beginning with a combining mark";
                case error::joiner: return "a zero width joiner the context does not allow";
                case error::bidi: return "a label that breaks the bidirectional rule of RFC 5893";
                case error::std3: return "an ASCII character outside letters, digits and the hyphen";
                case error::punycode: return "an xn-- label that is not punycode";
                case error::empty_label: return "a label with nothing in it";
                case error::label_too_long: return "a label longer than 63 bytes";
                case error::name_too_long: return "a name longer than 253 bytes";
            }
            return "";
        }

        // The flags of UTS #46. They are a value and not a set of tags,
        // because a program does not choose them where it is compiled: a
        // URL parser is told what profile to follow, and the same call
        // site serves a strict lookup and a lenient one.
        //
        // The defaults are the strict reading — everything checked, the
        // name held to what the DNS will carry. A browser's URL parser
        // wants whatwg() instead, which is a looser profile on purpose:
        // the WHATWG URL Standard leaves the hyphens and the lengths
        // alone so that names already in the wild keep working.
        struct options {
            // Transitional processing, which UTS #46 deprecates. It maps
            // the four deviation characters instead of leaving them:
            // "faß.de" becomes "fass.de" rather than "xn--fa-hia.de",
            // and a Greek final sigma becomes an ordinary one. That was
            // meant to carry IDNA2003 over, it silently sends two
            // different names to the same host, and nothing new should
            // ask for it.
            bool transitional = false;
            // Only the letters, the digits and the hyphen are let
            // through in ASCII (RFC 1123). Off by default, as the
            // browsers have it: an underscore in a host name is common
            // enough that refusing it surprises people
            bool use_std3_ascii_rules = false;
            bool check_hyphens = true;
            bool check_bidi = true;
            bool check_joiners = true;
            bool verify_dns_length = true;          // to_ascii only
            // A label that says "xn--" and is not punycode is left as it
            // stands rather than refused. Off by default and only there
            // for a parser that must not lose a name it cannot read
            bool ignore_invalid_punycode = false;

            static constexpr options standard() noexcept {
                return options{};
            }

            // What the WHATWG URL Standard asks for: the deviations kept,
            // the hyphens and the lengths not checked, the bidirectional
            // and joiner rules still checked
            static constexpr options whatwg() noexcept {
                options o;
                o.check_hyphens = false;
                o.verify_dns_length = false;
                return o;
            }
        };

        // What was wrong, and where. A name is refused for something one
        // of its labels did, and saying only that the name is wrong is
        // not enough for the caller that has to show it: a browser
        // underlines the label, it does not grey out the address bar. So
        // the label is named — which one it is, counting from zero, and
        // the bytes it took up in the text that was handed in, which are
        // not the bytes of the converted text and are what a caller can
        // point at.
        //
        // The range is of the label as it arrived, so it covers what was
        // mapped away and what was ignored: "a­b.com" is one label
        // of five bytes even though the soft hyphen leaves no trace in
        // the answer.
        struct failure {
            // The name as a whole, not any one label: only the length of
            // the whole name is wrong in this way
            static constexpr size_t whole_name = size_t(-1);

            error rule = error::none;
            size_t label = 0;           // which label, counting from zero
            size_t at = 0;              // where that label begins in the text given
            size_t size = 0;            // and how many bytes of it there were

            // Which rule it broke, in words (message_of(rule))
            string message() const {
                return string(message_of(rule));
            }
        };

        // What came out and what was wrong with it. UTS #46 converts as
        // far as it can even when it fails, and that text is worth
        // having: a browser shows the user the name it would not look up,
        // with the label that failure names marked. A caller that only
        // wants the name uses to_ascii and to_unicode below, which hand
        // back nothing at all when something was wrong.
        struct outcome {
            string text;
            failure reason;

            explicit operator bool() const noexcept {
                return reason.rule == error::none;
            }
        };
    }

    namespace detail {
        constexpr idna_status idna_status_of(char32_t c) noexcept {
            return idna_status(value_of(c, idna_tables::IdnaStatus));
        }

        constexpr joining idna_joining_of(char32_t c) noexcept {
            return joining(value_of(c, idna_tables::JoiningType));
        }

        // What a mapped code point becomes, and no table for it.
        //
        // The mapping of UTS #46 is NFKC_Casefold with three exceptions
        // and nothing else: of the 6350 code points the standard's table
        // maps, 6347 are exactly what NFKC_Casefold gives, which the
        // generator asserts against IdnaMappingTable.txt before it writes
        // anything. identifier.h had already found that NFKC_Casefold
        // needs no table either — it is the compatibility decomposition
        // folded and put back together, out of the tables normalization
        // and the case mappings carry anyway — so the 58.6 KB this
        // header used to spend saying the same thing a second time is
        // gone, and the three exceptions are a switch.
        //
        // The three are not arbitrary. The capital sharp s folds to "ss"
        // for everybody else, and UTS #46 sends it to the small sharp s
        // instead, so that it lands on a deviation character and
        // nontransitional processing can leave it there. And the two
        // ideographic full stops are label separators here and ordinary
        // characters everywhere else, so UTS #46 folds them to the ASCII
        // stop where NFKC_Casefold does not.
        //
        // Two more come in below them, and they are not a difference
        // from NFKC_Casefold but from the way this header computes it:
        // the zero width joiners map to nothing, which NFKC_Casefold also
        // says, by dropping the default ignorable code points. Nothing
        // here drops those — no other mapped code point is one, which the
        // generator asserts — so the two are written out. They are only
        // ever reached under transitional processing, the joiners being
        // deviation characters.
        //
        // What comes out is composed, one code point's worth at a time,
        // because that is what the property value is: NFC of the folded
        // decomposition of that one code point. Leaving it decomposed
        // would give the same name in the end — the step after this one
        // normalizes the whole thing — but it would give it the long
        // way: "BÜCHER.DE" would arrive as a U and a diaeresis and the
        // quick check would have to say no, where composed here it stays
        // a name that is already in form C. Measured: 1028 ns against
        // 673 for a name of capitals with accents in it.
        //
        // What is not done is taking the run apart as a whole. Each code
        // point is decomposed and folded on its own, because U+0345 has
        // a combining class of 240 and folds to an iota that has none,
        // and decomposing a run first would sort it past what follows it
        // — which is what cost identifier.h 311 differences before it
        // was found.
        inline void idna_mapped(vector<char32_t>& out, vector<char32_t>& taken,
                                vector<char32_t>& folded, char32_t c) {
            switch (c) {
                case 0x1E9E: out.push_back(0x00DF); return;     // capital sharp s
                case 0x3002: case 0xFF61: out.push_back(U'.'); return;
                case 0x200C: case 0x200D: return;               // the joiners, to nothing
                default: break;
            }
            // The one that is worth a line of its own: the only ASCII
            // code points the standard maps are A to Z, and every one of
            // them to the letter 32 above it. The generator asserts both.
            // It is the commonest mapped character in any name there is —
            // somebody typing a host name in capitals — and this saves it
            // the decomposition and the search of the fold table.
            if (c < 0x80) {
                out.push_back(c | 32);
                return;
            }
            taken.clear();
            decompose_into<true>(taken, c);
            // then the common one: a letter that folds to one letter and
            // has nothing to put in order or to compose
            if (taken.size() == 1) {
                if (auto d = full_of(taken[0], case_tables::FullFold)) {
                    append(out, d);
                } else {
                    out.push_back(unicode::to_lower(taken[0]));
                }
                return;
            }
            folded.clear();
            for (auto x : taken) {
                if (auto d = full_of(x, case_tables::FullFold)) {
                    append(folded, d);
                } else {
                    folded.push_back(unicode::to_lower(x));
                }
            }
            canonical_order(folded);
            compose_buffer(folded);
            for (auto x : folded) {
                out.push_back(x);
            }
        }

        // Step 1 of section 4, and it answers whether everything that came
        // out is ASCII: an ASCII text is in every normalization form, so
        // the step after this one can be skipped for the names that most
        // of the world's traffic is addressed to.
        //
        // It can also write down where every separator stood in the text
        // it was given — the byte it began at and how wide it was — which
        // is what lets a failure name the label at fault in the caller's
        // own bytes. Nobody asks for that on the way through: it is asked
        // for afterwards, by mapping the name a second time, and only
        // when the name has already failed. Writing the positions down on
        // every pass cost 150 ns on every name that was fine — two heap
        // arrays for an answer almost nobody wants — where a second pass
        // costs nothing at all on the road that matters and one more walk
        // on the road where nothing is waiting.
        //
        // Only the separators, not an origin per code point: the count
        // and the order of them survive everything that comes after, so
        // the k-th stop here is the k-th stop in the converted name.
        // Normalization cannot make one, lose one or move one — a full
        // stop is a starter, a mark never crosses one, and nothing
        // composes with it.
        inline bool idna_map(std::string_view text, bool transitional, vector<char32_t>& out,
                             std::vector<pair<size_t, size_t>>* stops = nullptr) {
            bool ascii = true;
            out.reserve(text.size());
            // two scratch buffers for the whole name: idna_mapped takes
            // a code point apart into them, and they are cleared and not
            // freed
            vector<char32_t> taken;
            vector<char32_t> folded;
            for (size_t i = 0; i < text.size();) {
                auto [c, n] = utf8::decode(text, i);
                size_t from = i;
                size_t was = out.size();
                i += n;
                switch (idna_status_of(c)) {
                    case idna_status::valid:
                    case idna_status::disallowed:
                        // a disallowed code point stays where it is: the
                        // validity criteria are what refuses it, and the
                        // text that comes out is still the text that went
                        // in as far as it could be
                        out.push_back(c);
                        ascii = ascii && c < 0x80;
                        break;
                    case idna_status::ignored:
                        break;
                    case idna_status::mapped:
                        // the one code point the table cannot answer for:
                        // a capital sharp s maps to a small one, which
                        // transitional processing must then take to "ss"
                        // rather than leave as a deviation
                        if (transitional && c == 0x1E9E) {
                            out.push_back(U's');
                            out.push_back(U's');
                        } else {
                            idna_mapped(out, taken, folded, c);
                            for (size_t k = was; k < out.size(); ++k) {
                                ascii = ascii && out[k] < 0x80;
                            }
                        }
                        break;
                    case idna_status::deviation:
                        if (transitional) {
                            idna_mapped(out, taken, folded, c);   // the joiners map to nothing
                        } else {
                            out.push_back(c);
                            ascii = false;              // all four deviations are above ASCII
                        }
                        break;
                }
                // a separator may be one the mapping made (the three
                // other full stops of UTS #46 all map to this one), and
                // in principle one code point could make several
                if (stops) {
                    for (size_t k = was; k < out.size(); ++k) {
                        if (out[k] == U'.') {
                            stops->push_back({from, n});
                        }
                    }
                }
            }
            return ascii;
        }

        // Normalization form C over code points rather than over a text:
        // the name is taken apart into them once and put together once,
        // and normalize.h's own steps do the work
        // Whether a run of code points is in form C already, by the
        // quick check properties alone — the same question normalize.h
        // asks of a text, over code points that have been mapped. It is
        // "maybe" that costs: a name of letters with accents already on
        // them answers yes and the work below is not done at all
        inline bool idna_quick_nfc(const vector<char32_t>& in) noexcept {
            uint8_t last = 0;
            for (auto c : in) {
                uint8_t cc = ccc_fn(c);
                if (last > cc && cc != 0) {
                    return false;
                }
                if (quick_check(c, 0) != QuickCheckYes) {
                    return false;
                }
                last = cc;
            }
            return true;
        }

        inline vector<char32_t> idna_nfc(const vector<char32_t>& in) {
            vector<char32_t> out;
            out.reserve(in.size());
            for (auto c : in) {
                decompose_into<false>(out, c);
            }
            canonical_order(out);
            compose_buffer(out);
            return out;
        }

        inline bool idna_is_nfc(const char32_t* p, size_t n) {
            // the quick check first, and it settles almost every label:
            // only where it says "maybe" does the label have to be
            // normalized and compared
            uint8_t last = 0;
            bool sure = true;
            for (size_t i = 0; i < n; ++i) {
                uint8_t cc = ccc_fn(p[i]);
                if (last > cc && cc != 0) {
                    return false;
                }
                unsigned q = quick_check(p[i], 0);
                if (q == QuickCheckNo) {
                    return false;
                }
                sure = sure && q != QuickCheckMaybe;
                last = cc;
            }
            if (sure) {
                return true;
            }
            vector<char32_t> in;
            in.reserve(n);
            for (size_t i = 0; i < n; ++i) {
                in.push_back(p[i]);
            }
            auto made = idna_nfc(in);
            return made.size() == n && std::equal(made.begin(), made.end(), p);
        }

        // Appendix A.1 and A.2 of RFC 5892, the rules that say when a
        // zero width joiner is there to shape a script and when it is
        // there to make two names look alike. A.2 is the short one: a
        // joiner is allowed only after a virama. A.1 also allows a non
        // joiner between two letters that would otherwise join, which is
        // what Persian needs to write a word with a visible break in it.
        //
        // A.1 and A.2 are the whole of CONTEXTJ, and CONTEXTJ is the
        // whole of what is here. CONTEXTO — appendices A.3 to A.9, the
        // rules for the middle dot between two l's in Catalan, the Greek
        // keraia, the Hebrew geresh and gershayim, the katakana middle
        // dot and the two families of Arabic-Indic digits — is not
        // implemented, and that is a decision and not an oversight.
        //
        // Where it stops being the right one: those rules exist to stop
        // a name being *registered*, and this library is the other end.
        // A client looks a name up, and the registry it is looking it up
        // in is what applied them — IdnaTestV2.txt says as much, calling
        // the CONTEXTO tests optional for client software and carrying
        // none of them, and a client that enforced them would refuse
        // names that are registered and resolve today. The day something
        // here decides whether a name may be taken rather than whether
        // one may be resolved, they are needed, and they want the Script
        // property (which properties.h has) and a table for the digits;
        // it is an afternoon's work, not a design question.
        inline bool idna_joiners_ok(const char32_t* p, size_t n) {
            for (size_t i = 0; i < n; ++i) {
                if (p[i] != 0x200C && p[i] != 0x200D) {
                    continue;
                }
                if (i == 0) {
                    return false;                       // nothing before it to be joined
                }
                if (ccc_fn(p[i - 1]) == 9) {
                    continue;                           // a virama: both rules allow it
                }
                if (p[i] == 0x200D) {
                    return false;                       // and that is the whole of A.2
                }
                size_t before = i;
                while (before > 0 && idna_joining_of(p[before - 1]) == joining::t) {
                    --before;
                }
                if (before == 0) {
                    return false;
                }
                auto left = idna_joining_of(p[before - 1]);
                if (left != joining::l && left != joining::d) {
                    return false;
                }
                size_t after = i + 1;
                while (after < n && idna_joining_of(p[after]) == joining::t) {
                    ++after;
                }
                if (after == n) {
                    return false;
                }
                auto right = idna_joining_of(p[after]);
                if (right != joining::r && right != joining::d) {
                    return false;
                }
            }
            return true;
        }

        // The six conditions of RFC 5893 section 2, over a label of a
        // name that has something right to left in it anywhere. They are
        // there so that a name reads the same way whichever end of it a
        // reader starts from — a label that mixes the two directions can
        // be drawn in an order that is not the order it is stored in, and
        // two different names would then look like one.
        inline bool idna_bidi_ok(const char32_t* p, size_t n) {
            auto first = bidi_of(p[0]);
            bool rtl = first == bidi::r || first == bidi::al;
            if (!rtl && first != bidi::l) {
                return false;                           // 1
            }
            bool en = false;
            bool an = false;
            for (size_t i = 0; i < n; ++i) {
                auto t = bidi_of(p[i]);
                if (rtl) {
                    switch (t) {                        // 2
                        case bidi::r: case bidi::al: case bidi::an: case bidi::en:
                        case bidi::es: case bidi::cs: case bidi::et: case bidi::on:
                        case bidi::bn: case bidi::nsm:
                            break;
                        default:
                            return false;
                    }
                    en = en || t == bidi::en;
                    an = an || t == bidi::an;
                } else {
                    switch (t) {                        // 5
                        case bidi::l: case bidi::en: case bidi::es: case bidi::cs:
                        case bidi::et: case bidi::on: case bidi::bn: case bidi::nsm:
                            break;
                        default:
                            return false;
                    }
                }
            }
            // 3 and 6: what the label ends in, the marks at the end not
            // counting — a mark takes the direction of what it sits on
            size_t last = n;
            while (last > 0 && bidi_of(p[last - 1]) == bidi::nsm) {
                --last;
            }
            if (last == 0) {
                return false;
            }
            auto end = bidi_of(p[last - 1]);
            if (rtl) {
                return (end == bidi::r || end == bidi::al || end == bidi::en || end == bidi::an)
                    && !(en && an);                     // 3 and 4
            }
            return end == bidi::l || end == bidi::en;   // 6
        }

        // Section 4.1, everything but the bidirectional rule, which needs
        // to know about the whole name and is asked after every label has
        // been converted. `nfc` says whether the form has to be checked:
        // a label taken out of the name is in it already, the name having
        // been normalized whole, and only one that came out of punycode
        // has not been.
        inline idna::error idna_validate(const char32_t* p, size_t n, bool check_nfc,
                                         bool transitional, const idna::options& o) {
            if (check_nfc && !idna_is_nfc(p, n)) {
                return idna::error::not_normalized;     // 1
            }
            if (o.check_hyphens) {
                if (n >= 4 && p[2] == U'-' && p[3] == U'-') {
                    return idna::error::hyphen;         // 2
                }
                if (p[0] == U'-' || p[n - 1] == U'-') {
                    return idna::error::hyphen;         // 3
                }
            } else if (n >= 4 && (p[0] | 32) == U'x' && (p[1] | 32) == U'n'
                       && p[2] == U'-' && p[3] == U'-') {
                return idna::error::label_prefix;       // 4
            }
            if (is_mark_fn(p[0])) {
                return idna::error::leading_combining;  // 6
            }
            for (size_t i = 0; i < n; ++i) {
                char32_t c = p[i];
                if (c == U'.') {
                    return idna::error::label_separator; // 5
                }
                auto s = idna_status_of(c);              // 7
                if (s != idna_status::valid && !(!transitional && s == idna_status::deviation)) {
                    return idna::error::disallowed;
                }
                if (o.use_std3_ascii_rules && c < 0x80
                    && !((c >= U'a' && c <= U'z') || (c >= U'0' && c <= U'9') || c == U'-')) {
                    return idna::error::std3;
                }
            }
            if (o.check_joiners && !idna_joiners_ok(p, n)) {
                return idna::error::joiner;             // 8
            }
            return idna::error::none;
        }

        // The whole name, mapped, normalized, broken into labels and
        // every label converted out of punycode: the four steps of
        // section 4. The labels are kept as one run of code points with
        // the stops still between them, and the indices of where each one
        // begins beside it — a vector of vectors would be a container of
        // containers, which is a page of its own per label.
        struct IdnaName {
            vector<char32_t> points;
            std::vector<size_t> starts;             // one past the stop that begins the label
            std::vector<size_t> ends;
            idna::failure reason;

            size_t count() const noexcept {
                return starts.size();
            }

            // The first rule broken wins, and it takes the label with
            // it; where that label stood in the caller's own bytes is
            // filled in afterwards, by idna_locate, and only if there
            // was a failure at all
            void fail(idna::error e, size_t label) noexcept {
                if (reason.rule == idna::error::none) {
                    reason = {e, label, 0, 0};
                }
            }

            void fail_name(idna::error e, size_t bytes) noexcept {
                if (reason.rule == idna::error::none) {
                    reason = {e, idna::failure::whole_name, 0, bytes};
                }
            }
        };

        // Most of the names a program is ever given are already what the
        // DNS carries: lower case letters, digits, hyphens and stops.
        // There is nothing in such a name to map, nothing to normalize,
        // no label to decode, no mark to trip over, no character the
        // bidirectional rule would look at twice and none the joiner
        // rule would — so it is checked over its bytes and comes back as
        // the object it went in as, a string being shared and immutable.
        // Nothing is allocated at all, where the road below allocates six
        // times: "www0.example0.com" cost 370 ns and costs 60.
        //
        // An "xn--" label is not plain, however plain its bytes: it has
        // to be decoded before anything can be said about it.
        inline bool idna_plain(std::string_view v) noexcept {
            bool start = true;
            for (size_t i = 0; i < v.size(); ++i) {
                auto b = uint8_t(v[i]);
                if (!((b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') || b == '-' || b == '.')) {
                    return false;
                }
                if (start && i + 4 <= v.size() && v.compare(i, 4, "xn--") == 0) {
                    return false;
                }
                start = b == '.';
            }
            return true;
        }

        // The criteria that can still bite such a name: the hyphens, an
        // empty label, and the lengths the DNS imposes. They are asked in
        // the order the long road asks them, so that a name wrong in two
        // ways names the same rule either way it was checked.
        inline idna::failure idna_plain_check(std::string_view v, const idna::options& o,
                                              bool dns_length) noexcept {
            idna::failure first;
            auto fail = [&](idna::error e, size_t label, size_t at, size_t size) {
                if (first.rule == idna::error::none) {
                    first = {e, label, at, size};
                }
            };
            size_t labels = 1;
            for (char c : v) {
                labels += c == U'.';
            }
            auto each = [&](auto&& body) {
                size_t at = 0;
                for (size_t k = 0; k < labels; ++k) {
                    size_t end = v.find('.', at);
                    size_t stop = end == std::string_view::npos ? v.size() : end;
                    body(k, at, stop - at);
                    at = stop + 1;
                }
            };
            if (o.check_hyphens) {
                each([&](size_t k, size_t at, size_t n) {
                    if (n >= 4 && v[at + 2] == '-' && v[at + 3] == '-') {
                        fail(idna::error::hyphen, k, at, n);
                    } else if (n && (v[at] == '-' || v[at + n - 1] == '-')) {
                        fail(idna::error::hyphen, k, at, n);
                    }
                });
            }
            // an empty label is an error of VerifyDnsLength (A4_2) and of
            // nothing else since Unicode 15.1: the WHATWG URL Standard,
            // which turns the flag off, takes "x..y" as a host
            if (o.verify_dns_length) {
                each([&](size_t k, size_t at, size_t n) {
                    if (!n && !(k + 1 == labels && k > 0)) {
                        fail(idna::error::empty_label, k, at, n);
                    }
                });
            }
            if (dns_length) {
                each([&](size_t k, size_t at, size_t n) {
                    if (n == 0) {
                        // the root label is not carried either
                        fail(idna::error::empty_label, k, at, n);
                    } else if (n > 63) {
                        fail(idna::error::label_too_long, k, at, n);
                    }
                });
                size_t n = !v.empty() && v.back() == '.' ? v.size() - 1 : v.size();
                if (n < 1 || n > 253) {
                    fail(idna::error::name_too_long, idna::failure::whole_name, 0, v.size());
                }
            }
            return first;
        }

        // Where the label a failure names stood in the text that was
        // handed in. The mapping is run again to find out, which is a
        // second walk over the name — and it is run only after the name
        // has already failed, where nothing is waiting on the answer and
        // the caller is about to draw a red line under something.
        //
        // Doing it this way rather than on the way through is what keeps
        // the measured cost of the whole business at nothing: writing the
        // positions down for every name took to_ascii of a German name
        // from 527 ns to 677 and reading an encoded one back from 498 to
        // 677, all of it two heap arrays and their growth, for an answer
        // that is wanted once in however many thousand names.
        inline void idna_locate(idna::failure& f, std::string_view text, bool transitional) {
            if (f.rule == idna::error::none || f.label == idna::failure::whole_name) {
                return;
            }
            vector<char32_t> points;
            std::vector<pair<size_t, size_t>> stops;
            idna_map(text, transitional, points, &stops);
            // the label begins where the stop before it ends, and ends
            // where the stop after it begins; the last label has no stop
            // after it and the first has none before it
            size_t from = 0;
            if (f.label && f.label - 1 < stops.size()) {
                from = stops[f.label - 1].first + stops[f.label - 1].second;
            } else if (f.label) {
                from = text.size();
            }
            size_t to = f.label < stops.size() ? stops[f.label].first : text.size();
            f.at = from;
            f.size = to > from ? to - from : 0;
        }

        inline IdnaName idna_process(std::string_view text, const idna::options& o) {
            IdnaName name;
            vector<char32_t> mapped;
            bool ascii = idna_map(text, o.transitional, mapped);
            // ASCII is in every normalization form, and so is a name
            // whose letters carry their accents already: the quick check
            // settles both without a decomposition
            vector<char32_t> points = ascii || idna_quick_nfc(mapped)
                                    ? std::move(mapped) : idna_nfc(mapped);

            // Every label is written into `out`, converted where it was
            // punycode, with the stops kept between them
            vector<char32_t> out;
            out.reserve(points.size());
            vector<char32_t> decoded;
            size_t at = 0;
            for (;;) {
                size_t label = name.count();
                size_t end = at;
                while (end < points.size() && points[end] != U'.') {
                    ++end;
                }
                const char32_t* p = points.data() + at;
                size_t n = end - at;
                size_t begin = out.size();
                bool converted = false;
                // a label whose punycode does not read is left exactly as
                // it stands and is not looked at again: the criteria are
                // about the label a reader would see, and there is none
                bool unread = false;
                if (n >= 4 && (p[0] | 32) == U'x' && (p[1] | 32) == U'n'
                    && p[2] == U'-' && p[3] == U'-') {
                    bool pure = true;
                    std::string body;
                    body.reserve(n - 4);
                    for (size_t i = 4; i < n; ++i) {
                        pure = pure && p[i] < 0x80;
                        body.push_back(char(p[i] & 0x7F));
                    }
                    decoded.clear();
                    if (!pure) {
                        name.fail(idna::error::punycode, label);
                        unread = true;
                    } else if (!puny_decode(body, decoded)) {
                        if (!o.ignore_invalid_punycode) {
                            name.fail(idna::error::punycode, label);
                        }
                        unread = true;
                    } else {
                        converted = true;
                        bool any = false;
                        for (auto c : decoded) {
                            any = any || c >= 0x80;
                        }
                        if (decoded.empty() || !any) {
                            name.fail(idna::error::punycode, label);
                        }
                        for (auto c : decoded) {
                            out.push_back(c);
                        }
                    }
                }
                if (!converted) {
                    for (size_t i = 0; i < n; ++i) {
                        out.push_back(p[i]);
                    }
                }
                name.starts.push_back(begin);
                name.ends.push_back(out.size());
                // a punycode label is validated as nontransitional
                // whatever was asked for: what is already encoded was
                // encoded once and is not remapped
                if (out.size() > begin && !unread) {
                    auto bad = idna_validate(out.data() + begin, out.size() - begin, converted,
                                             converted ? false : o.transitional, o);
                    if (bad != idna::error::none) {
                        name.fail(bad, label);
                    }
                }
                if (end >= points.size()) {
                    break;
                }
                out.push_back(U'.');
                at = end + 1;
            }

            // An empty label is a name with two stops in a row, or one
            // that begins with a stop, or nothing at all. The last label
            // of a name may be empty — that is the root of the DNS, the
            // trailing dot of "example.com." — as long as something came
            // before it. Only under VerifyDnsLength, as above.
            for (size_t k = 0; o.verify_dns_length && k < name.count(); ++k) {
                if (name.ends[k] == name.starts[k] && !(k + 1 == name.count() && k > 0)) {
                    name.fail(idna::error::empty_label, k);
                }
            }

            // The bidirectional rule asks about the name and not about a
            // label: a name is a right to left name when anything
            // anywhere in it is, and then every label of it, the ASCII
            // ones included, has to hold to the six conditions
            if (o.check_bidi) {
                bool any = false;
                for (auto c : out) {
                    auto t = bidi_of(c);
                    if (t == bidi::r || t == bidi::al || t == bidi::an) {
                        any = true;
                        break;
                    }
                }
                if (any) {
                    for (size_t k = 0; k < name.count(); ++k) {
                        if (name.ends[k] > name.starts[k]
                            && !idna_bidi_ok(out.data() + name.starts[k], name.ends[k] - name.starts[k])) {
                            name.fail(idna::error::bidi, k);
                        }
                    }
                }
            }
            name.points = std::move(out);
            return name;
        }
    }

    namespace idna {
        // Section 4.3. The name as a reader would write it: every label
        // that was punycode read back into the script it was written in.
        // The text comes out even when something was wrong with it,
        // because that is the text there is to show somebody.
        inline outcome unicode_form(const string& name, options o = {}) {
            if (detail::idna_plain(name.view())) {
                return {name, detail::idna_plain_check(name.view(), o, false)};
            }
            auto made = detail::idna_process(name.view(), o);
            detail::idna_locate(made.reason, name.view(), o.transitional);
            return {detail::encoded(made.points), made.reason};
        }

        // Section 4.2. The name as the DNS carries it: every label with
        // something above ASCII in it encoded and prefixed with "xn--".
        inline outcome ascii_form(const string& name, options o = {}) {
            if (detail::idna_plain(name.view())) {
                return {name, detail::idna_plain_check(name.view(), o, o.verify_dns_length)};
            }
            auto made = detail::idna_process(name.view(), o);
            std::string out;
            out.reserve(made.points.size());
            vector<char32_t> label;
            for (size_t k = 0; k < made.count(); ++k) {
                if (k) {
                    out.push_back('.');
                }
                size_t from = made.starts[k];
                size_t to = made.ends[k];
                size_t was = out.size();
                bool ascii = true;
                for (size_t i = from; i < to; ++i) {
                    ascii = ascii && made.points[i] < 0x80;
                }
                if (ascii) {
                    for (size_t i = from; i < to; ++i) {
                        out.push_back(char(made.points[i]));
                    }
                } else {
                    label.clear();
                    for (size_t i = from; i < to; ++i) {
                        label.push_back(made.points[i]);
                    }
                    out += "xn--";
                    if (!detail::puny_encode(label, out)) {
                        made.fail(error::punycode, k);
                    }
                }
                if (o.verify_dns_length && out.size() - was > 63) {
                    made.fail(error::label_too_long, k);
                }
                if (o.verify_dns_length && out.size() == was) {
                    // the trailing dot of "example.com." is the root of
                    // the DNS, which a lookup does not carry either
                    made.fail(error::empty_label, k);
                }
            }
            if (o.verify_dns_length) {
                // the root label and the stop before it do not count
                size_t n = out.size() && out.back() == '.' ? out.size() - 1 : out.size();
                if (n < 1 || n > 253) {
                    made.fail_name(error::name_too_long, name.size());
                }
            }
            detail::idna_locate(made.reason, name.view(), o.transitional);
            return {string(out.data(), out.size()), made.reason};
        }

        // The two the rest of the library calls. A name that is wrong in
        // any of the ways above is not a name: nothing comes back but the
        // rule it broke, and there is no text to use by mistake.
        inline expected<string, failure> to_unicode(const string& name, options o = {}) {
            auto made = unicode_form(name, o);
            if (!made) {
                return unexpected<failure>(made.reason);
            }
            return made.text;
        }

        inline expected<string, failure> to_ascii(const string& name, options o = {}) {
            auto made = ascii_form(name, o);
            if (!made) {
                return unexpected<failure>(made.reason);
            }
            return made.text;
        }

        // The version of the tables, which is the version of Unicode:
        // what a name maps to is fixed by it, and two programs that
        // disagree about it disagree about where a name points
        inline constexpr const char* version = unicode::version;
    }
}
