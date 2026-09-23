//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../containers/vector.h"
#include "properties.h"
#include "detail/segment_tables.h"

// The boundaries of a text: where one grapheme — what a human counts as a
// character — ends and the next begins (UAX #29). A range is a class
// constructed from the text, as sgcl::runes is: graphemes(s) looks like a
// call and is a construction, it allocates nothing per element, it decodes
// as it walks, and the slice it holds keeps the text's object alive, so a
// loop over a temporary is safe.
namespace sgcl::txt {
    namespace detail {
        constexpr gcb gcb_of(char32_t c) noexcept {
            if (c < 0x300) {
                // ASCII and Latin-1 without a table: only the controls,
                // CR and LF are anything but "other" down here
                if (c == U'\r') {
                    return gcb::cr;
                }
                if (c == U'\n') {
                    return gcb::lf;
                }
                if (c < 0x20 || (c >= 0x7F && c <= 0x9F) || c == 0xAD) {
                    return gcb::control;
                }
                return gcb::other;
            }
            return gcb(value_of(c, segment_tables::GraphemeBreak));
        }

        constexpr incb incb_of(char32_t c) noexcept {
            return c < 0x300 ? incb::other : incb(value_of(c, segment_tables::IndicConjunctBreak));
        }

        // The rules of UAX #29 run left to right over one cluster, so the
        // whole left context a boundary needs is what this scan has seen
        // since the last boundary: whether an Extended_Pictographic is
        // waiting for its ZWJ (GB11), whether a consonant has passed a
        // linker (GB9c), and how many regional indicators are open
        // (GB12, GB13). A cluster is scanned from a boundary, so the state
        // starts empty and is carried forward.
        class cluster_state {
        public:
            constexpr void start(char32_t c) noexcept {
                auto k = gcb_of(c);
                auto i = incb_of(c);
                _pictographic = is_emoji_fn(c);
                _zwj_after_pictographic = false;
                _consonant = i == incb::consonant;
                _linked = false;
                _regional = k == gcb::regional_indicator ? 1 : 0;
            }

            // Whether a boundary falls between the code point last given
            // to start() or advance() and c
            constexpr bool breaks_before(char32_t prev, char32_t c) const noexcept {
                auto a = gcb_of(prev);
                auto b = gcb_of(c);
                if (a == gcb::cr && b == gcb::lf) {                                  // GB3
                    return false;
                }
                if (a == gcb::control || a == gcb::cr || a == gcb::lf) {             // GB4
                    return true;
                }
                if (b == gcb::control || b == gcb::cr || b == gcb::lf) {             // GB5
                    return true;
                }
                if (a == gcb::l && (b == gcb::l || b == gcb::v || b == gcb::lv || b == gcb::lvt)) {   // GB6
                    return false;
                }
                if ((a == gcb::lv || a == gcb::v) && (b == gcb::v || b == gcb::t)) { // GB7
                    return false;
                }
                if ((a == gcb::lvt || a == gcb::t) && b == gcb::t) {                 // GB8
                    return false;
                }
                if (b == gcb::extend || b == gcb::zwj) {                             // GB9
                    return false;
                }
                if (b == gcb::spacing_mark) {                                        // GB9a
                    return false;
                }
                if (a == gcb::prepend) {                                             // GB9b
                    return false;
                }
                if (_linked && incb_of(c) == incb::consonant) {                      // GB9c
                    return false;
                }
                if (_zwj_after_pictographic && is_emoji_fn(c)) {                     // GB11
                    return false;
                }
                if (b == gcb::regional_indicator && (_regional & 1)) {               // GB12, GB13
                    return false;
                }
                return true;                                                         // GB999
            }

            // c joins the cluster: carry what the rules ahead will need
            constexpr void advance(char32_t c) noexcept {
                auto k = gcb_of(c);
                auto i = incb_of(c);
                if (k == gcb::zwj) {
                    _zwj_after_pictographic = _pictographic;
                } else {
                    // GB11 wants Extend* between the picture and the joiner
                    _zwj_after_pictographic = false;
                    _pictographic = k == gcb::extend ? _pictographic : is_emoji_fn(c);
                }
                if (i == incb::linker) {
                    _linked = _consonant;
                } else if (i == incb::consonant) {
                    _consonant = true;
                    _linked = false;
                } else if (i != incb::extend) {
                    // GB9c wants only linkers and InCB extenders between
                    _consonant = false;
                    _linked = false;
                }
                _regional = k == gcb::regional_indicator ? _regional + 1 : 0;
            }

        private:
            bool _pictographic = false;
            bool _zwj_after_pictographic = false;
            bool _consonant = false;
            bool _linked = false;
            unsigned _regional = 0;
        };

        // Word_Break, with the rules of UAX #29 over the code points that
        // rule WB4 does not absorb: X (Extend | Format | ZWJ)* counts as
        // X, so a letter with its accents is one letter to the rules that
        // look left and right. The scan therefore carries two streams —
        // the code point immediately before (rules WB3 to WB3d, which see
        // the absorbed ones) and the last two that survived WB4.
        constexpr bool is_ahletter(wb x) noexcept {
            return x == wb::aletter || x == wb::hebrew_letter;
        }

        constexpr bool is_midnumletq(wb x) noexcept {
            return x == wb::mid_num_let || x == wb::single_quote;
        }

        constexpr wb wb_of(char32_t c) noexcept {
            return wb(value_of(c, segment_tables::WordBreak));
        }

        constexpr bool wb_ignored(wb x) noexcept {
            return x == wb::extend || x == wb::format || x == wb::zwj;
        }

        // The class of the next code point that WB4 does not absorb,
        // after the one at pos, and nothing when the text ends first
        // A letter of the Latin alphabet, which every rule of this file
        // treats the same way and which most text is made of
        constexpr bool ascii_letter(std::string_view text, size_t pos) noexcept {
            if (pos >= text.size()) {
                return false;
            }
            char c = text[pos];
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        constexpr wb wb_lookahead(std::string_view text, size_t pos) noexcept {
            while (pos < text.size()) {
                auto [c, n] = utf8::decode(text, pos);
                auto k = wb_of(c);
                if (!wb_ignored(k)) {
                    return k;
                }
                pos += n;
            }
            return wb::other;
        }

        // Whether a word boundary falls before cur. prev is the code point
        // immediately before it, p and pp the last two that survived WB4,
        // ri the number of regional indicators open, and next the class
        // after cur for the rules that look one further (WB6, WB7b, WB12)
        constexpr bool word_breaks(char32_t prev, char32_t cur, wb pp, wb p, unsigned ri, wb next) noexcept {
            auto a = wb_of(prev);
            auto b = wb_of(cur);
            if (a == wb::cr && b == wb::lf) {                                        // WB3
                return false;
            }
            if (a == wb::newline || a == wb::cr || a == wb::lf) {                    // WB3a
                return true;
            }
            if (b == wb::newline || b == wb::cr || b == wb::lf) {                    // WB3b
                return true;
            }
            if (a == wb::zwj && is_emoji_fn(cur)) {                                  // WB3c
                return false;
            }
            if (a == wb::wseg_space && b == wb::wseg_space) {                        // WB3d
                return false;
            }
            if (wb_ignored(b)) {                                                     // WB4
                return false;
            }
            if (is_ahletter(p) && is_ahletter(b)) {                                  // WB5
                return false;
            }
            if (is_ahletter(p) && (b == wb::mid_letter || is_midnumletq(b)) && is_ahletter(next)) {   // WB6
                return false;
            }
            if (is_ahletter(pp) && (p == wb::mid_letter || is_midnumletq(p)) && is_ahletter(b)) {     // WB7
                return false;
            }
            if (p == wb::hebrew_letter && b == wb::single_quote) {                   // WB7a
                return false;
            }
            if (p == wb::hebrew_letter && b == wb::double_quote && next == wb::hebrew_letter) {       // WB7b
                return false;
            }
            if (pp == wb::hebrew_letter && p == wb::double_quote && b == wb::hebrew_letter) {         // WB7c
                return false;
            }
            if (p == wb::numeric && b == wb::numeric) {                              // WB8
                return false;
            }
            if (is_ahletter(p) && b == wb::numeric) {                                // WB9
                return false;
            }
            if (p == wb::numeric && is_ahletter(b)) {                                // WB10
                return false;
            }
            if (pp == wb::numeric && (p == wb::mid_num || is_midnumletq(p)) && b == wb::numeric) {    // WB11
                return false;
            }
            if (p == wb::numeric && (b == wb::mid_num || is_midnumletq(b)) && next == wb::numeric) {  // WB12
                return false;
            }
            if (p == wb::katakana && b == wb::katakana) {                            // WB13
                return false;
            }
            if ((is_ahletter(p) || p == wb::numeric || p == wb::katakana || p == wb::extend_num_let)
                && b == wb::extend_num_let) {                                        // WB13a
                return false;
            }
            if (p == wb::extend_num_let && (is_ahletter(b) || b == wb::numeric || b == wb::katakana)) {   // WB13b
                return false;
            }
            if (b == wb::regional_indicator && (ri & 1)) {                           // WB15, WB16
                return false;
            }
            return true;                                                             // WB999
        }

        // The end of the word that starts at pos. A word starts at a
        // boundary, and no rule reaches across one, so the scan starts
        // with no left context
        constexpr size_t word_end(std::string_view text, size_t pos) noexcept {
            if (pos >= text.size()) {
                return text.size();
            }
            auto [first, n] = utf8::decode(text, pos);
            char32_t prev = first;
            wb pp = wb::other;
            wb p = wb_of(first);
            unsigned ri = p == wb::regional_indicator ? 1 : 0;
            pos += n;
            while (pos < text.size()) {
                // A run of Latin letters is one word by WB5, and nothing
                // before that rule can fire between two of them: the
                // rules that would are about marks, quotation and digits,
                // and an ASCII letter is none of those. So the run is
                // walked in one step rather than a step a letter, and the
                // lookahead WB6 needs is not made at all.
                if (ascii_letter(text, pos) && p == wb::aletter) {
                    size_t start = pos;
                    while (ascii_letter(text, pos)) {
                        ++pos;
                    }
                    prev = char32_t(uint8_t(text[pos - 1]));
                    pp = wb::aletter;
                    ri = 0;
                    if (pos > start) {
                        continue;
                    }
                }
                auto [c, w] = utf8::decode(text, pos);
                auto b = wb_of(c);
                if (word_breaks(prev, c, pp, p, ri, wb_lookahead(text, pos + w))) {
                    break;
                }
                if (!wb_ignored(b)) {
                    pp = p;
                    p = b;
                    ri = b == wb::regional_indicator ? ri + 1 : 0;
                }
                prev = c;
                pos += w;
            }
            return pos;
        }

        // The end of the cluster that starts at pos, or the size of the
        // text when pos is its last cluster
        constexpr size_t cluster_end(std::string_view text, size_t pos) noexcept {
            if (pos >= text.size()) {
                return text.size();
            }
            // Two ASCII characters always break apart — the classes they
            // can have are Other, Control, CR and LF, and of every pair
            // of those only CR before LF holds together (GB3). So a run
            // of Latin text is one character to a cluster and none of the
            // rules has to be asked. The character after has to be ASCII
            // too: an accent or a joiner behind a letter is not.
            if (uint8_t(text[pos]) < 0x80
                && (pos + 1 == text.size() || uint8_t(text[pos + 1]) < 0x80)) {
                return pos + (text[pos] == '\r' && pos + 1 < text.size() && text[pos + 1] == '\n' ? 2 : 1);
            }
            auto [prev, n] = utf8::decode(text, pos);
            pos += n;
            cluster_state state;
            state.start(prev);
            while (pos < text.size()) {
                auto [c, w] = utf8::decode(text, pos);
                if (state.breaks_before(prev, c)) {
                    break;
                }
                state.advance(c);
                prev = c;
                pos += w;
            }
            return pos;
        }
    }

    namespace detail {
        // Sentence_Break, the rules of UAX #29 again, with two things
        // the others do not have: the default is *not* to break (SB998),
        // and the tail of a sentence is a little grammar of its own —
        // (STerm | ATerm) Close* Sp* (Sep | CR | LF)? — which the scan
        // keeps as state, because every rule from SB8 on asks where in it
        // the text stands. Rule SB5 absorbs Extend and Format as WB4 does.
        constexpr sb sb_of(char32_t c) noexcept {
            return sb(value_of(c, segment_tables::SentenceBreak));
        }

        constexpr bool sb_ignored(sb x) noexcept {
            return x == sb::extend || x == sb::format;
        }

        constexpr bool sb_paragraph(sb x) noexcept {
            return x == sb::sep || x == sb::cr || x == sb::lf;
        }

        // Rule SB8 looks forward over what cannot end a sentence for a
        // lower case letter: ATerm Close* Sp* × (¬(OLetter | Upper | Lower
        // | Sep | CR | LF | STerm | ATerm))* Lower
        constexpr bool sb_lower_ahead(std::string_view text, size_t pos) noexcept {
            while (pos < text.size()) {
                auto [c, n] = utf8::decode(text, pos);
                auto k = sb_of(c);
                if (k == sb::lower) {
                    return true;
                }
                if (k == sb::oletter || k == sb::upper || k == sb::sterm || k == sb::aterm || sb_paragraph(k)) {
                    return false;
                }
                pos += n;
            }
            return false;
        }

        // The tail the rules from SB8 on ask about
        struct sentence_tail {
            sb term = sb::other;   // the aterm or sterm that opened it
            bool closed = false;   // a Close has passed
            bool spaced = false;   // a Sp has passed

            constexpr bool open() const noexcept {
                return term == sb::aterm || term == sb::sterm;
            }

            constexpr void advance(sb k) noexcept {
                if (k == sb::aterm || k == sb::sterm) {
                    term = k;
                    closed = spaced = false;
                } else if (open()) {
                    if (k == sb::close && !spaced) {
                        closed = true;
                    } else if (k == sb::sp) {
                        spaced = true;
                    } else {
                        term = sb::other;   // the tail is broken, or a paragraph ends it
                        closed = spaced = false;
                    }
                }
            }
        };

        constexpr bool sentence_breaks(std::string_view text, size_t at, char32_t prev, char32_t cur,
                                       sb pp, sb p, const sentence_tail& tail) noexcept {
            auto a = sb_of(prev);
            auto b = sb_of(cur);
            if (a == sb::cr && b == sb::lf) {                                   // SB3
                return false;
            }
            if (sb_paragraph(a)) {                                              // SB4
                return true;
            }
            if (sb_ignored(b)) {                                                // SB5
                return false;
            }
            if (p == sb::aterm && b == sb::numeric) {                           // SB6
                return false;
            }
            if ((pp == sb::upper || pp == sb::lower) && p == sb::aterm && b == sb::upper) {   // SB7
                return false;
            }
            // SB8 reads from cur itself: a lower case letter reached over
            // what cannot end a sentence keeps the sentence open, and an
            // upper case letter or another terminator stops the search
            if (tail.term == sb::aterm && sb_lower_ahead(text, at)) {           // SB8
                return false;
            }
            if (tail.open() && (b == sb::scontinue || b == sb::sterm || b == sb::aterm)) {    // SB8a
                return false;
            }
            if (tail.open() && !tail.spaced && (b == sb::close || b == sb::sp || sb_paragraph(b))) {   // SB9
                return false;
            }
            if (tail.open() && (b == sb::sp || sb_paragraph(b))) {              // SB10
                return false;
            }
            if (tail.open()) {                                                  // SB11
                return true;
            }
            return false;                                                       // SB998
        }

        // Line_Break, UAX #14. Two things set it apart from the rules
        // above. Its default is to break (LB31), and its rules reach
        // across a break opportunity — rule LB15a asks what stood before
        // an opening quotation mark, which may be on the other side of a
        // break — so the scan cannot start afresh at every segment the
        // way the others do, and the state below travels with the
        // iterator. Rule LB1 is already in the table: AI, SG, XX and the
        // unassigned are AL there, CJ is NS, and SA is CM or AL by its
        // category.
        constexpr lb lb_of(char32_t c) noexcept {
            return lb(value_of(c, segment_tables::LineBreak));
        }

        constexpr bool lb_east_asian(char32_t c) noexcept {
            return in_set(c, segment_tables::EastAsian);
        }

        constexpr bool lb_combining(lb x) noexcept {
            return x == lb::cm || x == lb::zwj;
        }

        // Rule LB9 attaches CM and ZWJ to what they follow, unless that
        // is a break of its own (LB10 then makes them AL)
        constexpr bool lb_attaches(lb x) noexcept {
            return x != lb::bk && x != lb::cr && x != lb::lf && x != lb::nl && x != lb::sp && x != lb::zw;
        }

        struct lb_point {
            lb kind = lb::al;    // with LB10 applied by the caller
            lb last = lb::al;    // the last code point passed, for LB8a
            char32_t c = 0;
            size_t next = 0;
            bool eot = true;
        };

        // The code point at pos with rules LB9 and LB10 applied: the
        // combining marks that attach to it are passed over, so that a
        // lookahead sees what the rules speak about
        constexpr lb_point lb_read(std::string_view text, size_t pos) noexcept {
            if (pos >= text.size()) {
                return {};
            }
            auto [c, n] = utf8::decode(text, pos);
            lb_point out{lb_of(c), lb_of(c), c, pos + n, false};
            if (lb_attaches(out.kind)) {
                while (out.next < text.size()) {
                    auto [x, w] = utf8::decode(text, out.next);
                    auto k = lb_of(x);
                    if (!lb_combining(k)) {
                        break;
                    }
                    out.last = k;
                    out.next += w;
                }
            }
            // LB10: a combining mark the scan lands on is one no code
            // point could attach it to, and it counts as a letter
            if (lb_combining(out.kind)) {
                out.kind = lb::al;
            }
            return out;
        }

        // What the rules from LB8 on need to know about the text to the
        // left of the position being decided
        struct line_state {
            lb pp = lb::al;            // the class two code points back
            lb p = lb::al;             // the class one code point back
            char32_t pp_char = 0;
            char32_t p_char = 0;
            lb pre_sp = lb::al;        // the class before the run of spaces
            char32_t pre_sp_char = 0;
            lb p_raw = lb::al;         // the last code point read, marks and all (LB8a)
            bool sot = true;           // nothing read yet
            bool sot_before_p = true;  // p was the first code point
            bool zw = false;           // a ZW is open, spaces may follow (LB8)
            bool qu_pi = false;        // an initial quotation mark is open (LB15a)
            bool nu = false;           // NU (SY | IS)* so far (LB25)
            bool nu_close = false;     // ... and then one CL or CP
            unsigned ri = 0;           // the regional indicators open (LB30a)
        };

        constexpr bool lb_pi(char32_t c) noexcept {
            return category_of_fn(c) == category::initial_punctuation;
        }

        constexpr bool lb_pf(char32_t c) noexcept {
            return category_of_fn(c) == category::final_punctuation;
        }

        constexpr bool lb_dotted_circle(char32_t c) noexcept {
            return c == 0x25CC;
        }

        constexpr bool lb_brahmic(lb x, char32_t c) noexcept {
            return x == lb::ak || x == lb::as || lb_dotted_circle(c);
        }

        constexpr bool lb_al_hl(lb x) noexcept {
            return x == lb::al || x == lb::hl;
        }

        constexpr bool lb_hangul(lb x) noexcept {
            return x == lb::jl || x == lb::jv || x == lb::jt || x == lb::h2 || x == lb::h3;
        }

        // Whether a line may be broken before the code point at `at`
        constexpr bool line_breaks_before(const line_state& s, std::string_view text, size_t at) noexcept {
            auto cur = lb_read(text, at);
            auto b = cur.kind;
            auto next = lb_read(text, cur.next);
            if (s.sot) {                                                        // LB2
                return false;
            }
            if (s.p == lb::bk) {                                                // LB4
                return true;
            }
            if (s.p == lb::cr && b == lb::lf) {                                 // LB5
                return false;
            }
            if (s.p == lb::cr || s.p == lb::lf || s.p == lb::nl) {              // LB5
                return true;
            }
            if (b == lb::bk || b == lb::cr || b == lb::lf || b == lb::nl) {     // LB6
                return false;
            }
            if (b == lb::sp || b == lb::zw) {                                   // LB7
                return false;
            }
            if (s.zw) {                                                         // LB8
                return true;
            }
            if (s.p_raw == lb::zwj) {                                           // LB8a
                return false;
            }
            if (b == lb::wj || s.p == lb::wj) {                                 // LB11
                return false;
            }
            if (s.p == lb::gl) {                                                // LB12
                return false;
            }
            if (b == lb::gl && s.p != lb::sp && s.p != lb::ba && s.p != lb::hy) {   // LB12a
                return false;
            }
            if (b == lb::cl || b == lb::cp || b == lb::ex || b == lb::sy) {     // LB13
                return false;
            }
            if (s.pre_sp == lb::op) {                                           // LB14
                return false;
            }
            if (s.qu_pi) {                                                      // LB15a
                return false;
            }
            if (b == lb::qu && lb_pf(cur.c)) {                                  // LB15b
                auto k = next.kind;
                if (next.eot || k == lb::sp || k == lb::gl || k == lb::wj || k == lb::cl || k == lb::qu
                    || k == lb::cp || k == lb::ex || k == lb::is || k == lb::sy || k == lb::bk
                    || k == lb::cr || k == lb::lf || k == lb::nl || k == lb::zw) {
                    return false;
                }
            }
            if (s.p == lb::sp && b == lb::is && next.kind == lb::nu) {          // LB15c
                return true;
            }
            if (b == lb::is) {                                                  // LB15d
                return false;
            }
            if ((s.pre_sp == lb::cl || s.pre_sp == lb::cp) && b == lb::ns) {    // LB16
                return false;
            }
            if (s.pre_sp == lb::b2 && b == lb::b2) {                            // LB17
                return false;
            }
            if (s.p == lb::sp) {                                                // LB18
                return true;
            }
            if (b == lb::qu && !lb_pi(cur.c)) {                                 // LB19
                return false;
            }
            if (s.p == lb::qu && !lb_pf(s.p_char)) {                            // LB19
                return false;
            }
            if (b == lb::qu && !lb_east_asian(s.p_char)) {                      // LB19a
                return false;
            }
            if (b == lb::qu && (next.eot || !lb_east_asian(next.c))) {          // LB19a
                return false;
            }
            if (s.p == lb::qu && !lb_east_asian(cur.c)) {                       // LB19a
                return false;
            }
            if (s.p == lb::qu && (s.sot_before_p || !lb_east_asian(s.pp_char))) {   // LB19a
                return false;
            }
            if (b == lb::cb || s.p == lb::cb) {                                 // LB20
                return true;
            }
            if ((s.p == lb::hy || s.p_char == 0x2010) && b == lb::al            // LB20a
                && (s.sot_before_p || s.pp == lb::bk || s.pp == lb::cr || s.pp == lb::lf || s.pp == lb::nl
                    || s.pp == lb::sp || s.pp == lb::zw || s.pp == lb::cb || s.pp == lb::gl)) {
                return false;
            }
            if (b == lb::ba || b == lb::hy || b == lb::ns || s.p == lb::bb) {   // LB21
                return false;
            }
            if (s.pp == lb::hl && b != lb::hl                                   // LB21a
                && (s.p == lb::hy || (s.p == lb::ba && !lb_east_asian(s.p_char)))) {
                return false;
            }
            if (s.p == lb::sy && b == lb::hl) {                                 // LB21b
                return false;
            }
            if (b == lb::in) {                                                  // LB22
                return false;
            }
            if ((lb_al_hl(s.p) && b == lb::nu) || (s.p == lb::nu && lb_al_hl(b))) {   // LB23
                return false;
            }
            if ((s.p == lb::pr && (b == lb::id || b == lb::eb || b == lb::em))   // LB23a
                || ((s.p == lb::id || s.p == lb::eb || s.p == lb::em) && b == lb::po)) {
                return false;
            }
            if (((s.p == lb::pr || s.p == lb::po) && lb_al_hl(b))                // LB24
                || (lb_al_hl(s.p) && (b == lb::pr || b == lb::po))) {
                return false;
            }
            if (s.nu && (b == lb::po || b == lb::pr)) {                          // LB25
                return false;
            }
            if (s.nu && !s.nu_close && b == lb::nu) {                            // LB25
                return false;
            }
            if ((s.p == lb::po || s.p == lb::pr) && b == lb::nu) {               // LB25
                return false;
            }
            if ((s.p == lb::po || s.p == lb::pr) && b == lb::op) {               // LB25
                auto after = next.kind == lb::is ? lb_read(text, next.next) : next;
                if (after.kind == lb::nu && !after.eot) {
                    return false;
                }
            }
            if ((s.p == lb::hy || s.p == lb::is) && b == lb::nu) {               // LB25
                return false;
            }
            if ((s.p == lb::jl && (b == lb::jl || b == lb::jv || b == lb::h2 || b == lb::h3))   // LB26
                || ((s.p == lb::jv || s.p == lb::h2) && (b == lb::jv || b == lb::jt))
                || ((s.p == lb::jt || s.p == lb::h3) && b == lb::jt)) {
                return false;
            }
            if ((lb_hangul(s.p) && b == lb::po) || (s.p == lb::pr && lb_hangul(b))) {   // LB27
                return false;
            }
            if (lb_al_hl(s.p) && lb_al_hl(b)) {                                  // LB28
                return false;
            }
            if (s.p == lb::ap && (lb_brahmic(b, cur.c) && b != lb::vf && b != lb::vi)) {   // LB28a
                return false;
            }
            if (lb_brahmic(s.p, s.p_char) && (b == lb::vf || b == lb::vi)) {     // LB28a
                return false;
            }
            if (lb_brahmic(s.pp, s.pp_char) && s.p == lb::vi                     // LB28a
                && (b == lb::ak || lb_dotted_circle(cur.c))) {
                return false;
            }
            if (lb_brahmic(s.p, s.p_char) && lb_brahmic(b, cur.c) && next.kind == lb::vf && !next.eot) {   // LB28a
                return false;
            }
            if (s.p == lb::is && lb_al_hl(b)) {                                  // LB29
                return false;
            }
            if (((lb_al_hl(s.p) || s.p == lb::nu) && b == lb::op && !lb_east_asian(cur.c))   // LB30
                || (s.p == lb::cp && !lb_east_asian(s.p_char) && (lb_al_hl(b) || b == lb::nu))) {
                return false;
            }
            if (b == lb::ri && (s.ri & 1)) {                                     // LB30a
                return false;
            }
            if (s.p == lb::eb && b == lb::em) {                                  // LB30b
                return false;
            }
            if (b == lb::em && is_emoji_fn(s.p_char) && category_of_fn(s.p_char) == category::unassigned) {   // LB30b
                return false;
            }
            return true;                                                         // LB31
        }

        // The code point at `at` is taken into the line: carry the state
        // Whether the rules can be passed over for the letter at `at`.
        // LB28 holds two letters of an alphabet together, and between two
        // Latin ones no earlier rule can fire — but only while nothing is
        // open that reaches across them: a zero width space, an initial
        // quotation mark, a run of digits, a regional indicator.
        constexpr bool lb_plain_letter(const line_state& s, std::string_view text, size_t at) noexcept {
            return ascii_letter(text, at) && s.p == lb::al && !s.sot
                && !s.zw && !s.qu_pi && !s.nu && !s.nu_close && s.ri == 0;
        }

        constexpr void line_advance(line_state& s, std::string_view text, size_t at) noexcept {
            auto cur = lb_read(text, at);
            auto b = cur.kind;
            if (b == lb::sp) {
                // The run of spaces leaves pre_sp where it was
            } else if (b == lb::zw) {
                s.zw = true;
                s.pre_sp = b;
                s.pre_sp_char = cur.c;
            } else {
                s.zw = false;
                s.pre_sp = b;
                s.pre_sp_char = cur.c;
            }
            if (b != lb::sp) {
                // LB15a: an initial quotation mark with the right context
                // stays open across the spaces that follow it
                s.qu_pi = b == lb::qu && lb_pi(cur.c)
                    && (s.sot || s.p == lb::bk || s.p == lb::cr || s.p == lb::lf || s.p == lb::nl
                        || s.p == lb::op || s.p == lb::qu || s.p == lb::gl || s.p == lb::sp || s.p == lb::zw);
                if (b == lb::nu) {
                    s.nu = true;
                    s.nu_close = false;
                } else if (s.nu && !s.nu_close && (b == lb::sy || b == lb::is)) {
                    // the run goes on
                } else if (s.nu && !s.nu_close && (b == lb::cl || b == lb::cp)) {
                    s.nu_close = true;
                } else {
                    s.nu = false;
                    s.nu_close = false;
                }
                s.ri = b == lb::ri ? s.ri + 1 : 0;
            }
            s.pp = s.p;
            s.pp_char = s.p_char;
            s.p = b;
            s.p_char = cur.c;
            s.p_raw = cur.last;
            s.sot_before_p = s.sot;
            s.sot = false;
        }

        // The end of the sentence that starts at pos
        constexpr size_t sentence_end(std::string_view text, size_t pos) noexcept {
            if (pos >= text.size()) {
                return text.size();
            }
            auto [first, n] = utf8::decode(text, pos);
            char32_t prev = first;
            sb pp = sb::other;
            sb p = sb_of(first);
            sentence_tail tail;
            tail.advance(p);
            pos += n;
            while (pos < text.size()) {
                auto [c, w] = utf8::decode(text, pos);
                auto b = sb_of(c);
                if (sentence_breaks(text, pos, prev, c, pp, p, tail)) {
                    break;
                }
                if (!sb_ignored(b)) {
                    pp = p;
                    p = b;
                    tail.advance(b);
                }
                prev = c;
                pos += w;
            }
            return pos;
        }
    }

    namespace detail {
        // The ranges of this header differ in one thing only — the
        // function that finds the end of a segment — so they are one
        // class over it, as runes is over the code points: constructed
        // from the text, holding the slice so that a loop over a
        // temporary is safe, allocating nothing per element, deciding as
        // it walks. The element is a slice of the text.
        template<auto End>
        class segment_range
        : public mixin::enumerable<segment_range<End>> {
        public:
            using value_type = slice<const char>;
            using size_type = size_t;

            class iterator {
            public:
                using iterator_category = std::forward_iterator_tag;
                using value_type = slice<const char>;
                using difference_type = ptrdiff_t;
                using reference = slice<const char>;
                using pointer = void;

                iterator() noexcept = default;

                slice<const char> operator*() const noexcept {
                    return _text.subslice(_pos, _end - _pos);
                }

                iterator& operator++() noexcept {
                    _pos = _end;
                    _end = End(_text.view(), _pos);
                    return *this;
                }

                iterator operator++(int) noexcept {
                    iterator t = *this;
                    ++*this;
                    return t;
                }

                friend bool operator==(const iterator& a, const iterator& b) noexcept {
                    return a._pos == b._pos;
                }

                // The byte position of the segment in the text, and its size
                size_t pos() const noexcept {
                    return _pos;
                }

                size_t size() const noexcept {
                    return _end - _pos;
                }

            private:
                friend class segment_range;

                iterator(slice<const char> text, size_t pos) noexcept
                : _text(text)
                , _pos(pos)
                , _end(End(text.view(), pos)) {
                }

                slice<const char> _text;
                size_t _pos = 0;
                size_t _end = 0;
            };

            using const_iterator = iterator;

            segment_range() noexcept = default;

            explicit segment_range(slice<const char> text) noexcept
            : _text(text) {
            }

            explicit segment_range(const string& text) noexcept
            : _text(text.as_slice()) {
            }

            iterator begin() const noexcept {
                return iterator(_text, 0);
            }

            iterator end() const noexcept {
                return iterator(_text, _text.size());
            }

            bool empty() const noexcept {
                return _text.empty();
            }

            // Walked and counted, not stored
            size_type count() const noexcept {
                size_t n = 0;
                for (size_t pos = 0; pos < _text.size(); ++n) {
                    pos = End(_text.view(), pos);
                }
                return n;
            }

            const slice<const char>& text() const noexcept {
                return _text;
            }

        private:
            slice<const char> _text;
        };
    }

    // graphemes: the grapheme clusters of a text, each a slice of it —
    // what a human counts as a character, which a code point is not: "é"
    // written as e plus a combining acute is one, a flag is two regional
    // indicators, an emoji with a skin tone is three code points, and a
    // Devanagari \u0915\u093F is a consonant with its vowel sign.
    using graphemes = detail::segment_range<detail::cluster_end>;

    // words: the text cut at every word boundary — the words themselves
    // and the runs between them, which is what UAX #29 defines and what a
    // double click and a search by whole words need. A run of spaces is
    // one segment (rule WB3d) while each punctuation mark is its own;
    // "don't" and "3.14" are one each, because the rules keep an
    // apostrophe and a decimal point inside a word. To count the words rather than the segments, ask for the
    // ones with something alphanumeric in them:
    // words(s).count_of([](auto w) { return w.runes().exists(is_alnum); })
    using words = detail::segment_range<detail::word_end>;

    // sentences: the text cut where one sentence ends and the next
    // begins. A full stop is not enough and not always needed: "i.e." and
    // "U.S.A." stay inside one, a stop followed by a lower case letter
    // ends nothing, and a line separator ends a sentence without any
    // punctuation at all. Every byte of the text belongs to exactly one
    // sentence, the space after the stop with the sentence it closes.
    //
    // These are the rules of the annex and nothing more, so an initial
    // before a capitalised name — "Pan J. Kowalski" — is two sentences:
    // to do better one needs a list of the abbreviations of a language,
    // which is a tailoring the annex leaves to the caller.
    using sentences = detail::segment_range<detail::sentence_end>;

    // line_breaks: the text cut at every place a line may be broken
    // (UAX #14) — after a space, after a hyphen, between two ideographs,
    // and never between a number and its decimal mark or inside "(a)".
    // Every element is a piece that must stay together, its trailing
    // spaces included, so a renderer lays them one after another and
    // starts a new line where the next will not fit; wrap() does exactly
    // that over columns().
    //
    // Its iterator carries state, where the other ranges of this header
    // start afresh at every segment: rule LB15a asks what stood before an
    // opening quotation mark, and that may be on the other side of a
    // break opportunity, so the scan has to run from the beginning of the
    // text. Walking the range is still linear.
    class line_breaks
    : public mixin::enumerable<line_breaks> {
    public:
        using value_type = slice<const char>;
        using size_type = size_t;

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = slice<const char>;
            using difference_type = ptrdiff_t;
            using reference = slice<const char>;
            using pointer = void;

            iterator() noexcept = default;

            slice<const char> operator*() const noexcept {
                return _text.subslice(_pos, _end - _pos);
            }

            iterator& operator++() noexcept {
                _pos = _end;
                _scan();
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._pos == b._pos;
            }

            size_t pos() const noexcept {
                return _pos;
            }

            size_t size() const noexcept {
                return _end - _pos;
            }

        private:
            friend class line_breaks;

            iterator(slice<const char> text, size_t pos) noexcept
            : _text(text)
            , _pos(pos)
            , _end(pos) {
                if (pos == 0) {
                    _scan();
                }
            }

            void _scan() noexcept {
                auto v = _text.view();
                size_t i = _pos;
                if (i >= v.size()) {
                    _end = i;
                    return;
                }
                // The first code point of a piece is taken without a
                // question: the break that ended the piece before it has
                // already been answered, and a piece is never empty
                detail::line_advance(_state, v, i);
                i = detail::lb_read(v, i).next;
                while (i < v.size() && !detail::line_breaks_before(_state, v, i)) {
                    if (detail::lb_plain_letter(_state, v, i)) {
                        // a run of Latin letters: the state it leaves is
                        // the state it found, so the rules are asked once
                        // for the run rather than once for a letter
                        do {
                            _state.pp = detail::lb::al;
                            _state.pp_char = _state.p_char;
                            _state.p_char = char32_t(uint8_t(v[i]));
                            _state.p_raw = detail::lb::al;
                            _state.sot_before_p = false;
                            ++i;
                        } while (detail::ascii_letter(v, i));
                        continue;
                    }
                    detail::line_advance(_state, v, i);
                    i = detail::lb_read(v, i).next;
                }
                _end = i;
            }

            slice<const char> _text;
            detail::line_state _state;
            size_t _pos = 0;
            size_t _end = 0;
        };

        using const_iterator = iterator;

        line_breaks() noexcept = default;

        explicit line_breaks(slice<const char> text) noexcept
        : _text(text) {
        }

        explicit line_breaks(const string& text) noexcept
        : _text(text.as_slice()) {
        }

        iterator begin() const noexcept {
            return iterator(_text, 0);
        }

        iterator end() const noexcept {
            return iterator(_text, _text.size());
        }

        bool empty() const noexcept {
            return _text.empty();
        }

        size_type count() const noexcept {
            size_t n = 0;
            for (auto it = begin(); it != end(); ++it) {
                ++n;
            }
            return n;
        }

        const slice<const char>& text() const noexcept {
            return _text;
        }

    private:
        slice<const char> _text;
    };


    // wrap: the text as lines no wider than the given number of columns,
    // cut only where UAX #14 allows and at the hard breaks the text
    // already has. A line is a slice of the text with its trailing spaces
    // dropped, so nothing is copied; a piece that is wider than the limit
    // on its own gets a line of its own and overflows it, because the
    // alternative is cutting a word in half. The width is counted in
    // terminal cells (columns), which is what a monospaced renderer and a
    // table of columns need.
    inline vector<slice<const char>> wrap(slice<const char> text, size_t width) {
        vector<slice<const char>> out;
        auto v = text.view();
        size_t start = 0;     // the first byte of the line being built
        size_t end = 0;       // past its last non-space byte
        size_t used = 0;      // the columns of [start, end)
        size_t gap = 0;       // the columns of the spaces waiting after it
        bool building = false;
        for (auto it = line_breaks(text).begin(); it != line_breaks(text).end(); ++it) {
            size_t from = it.pos();
            size_t to = from + it.size();
            size_t content = to;
            bool hard = false;
            while (content > from) {
                auto [c, n] = utf8::decode_last(v, content);
                auto k = detail::lb_of(c);
                if (k == detail::lb::bk || k == detail::lb::cr || k == detail::lb::lf || k == detail::lb::nl) {
                    hard = true;
                } else if (k != detail::lb::sp) {
                    break;
                }
                content -= n;
            }
            size_t w = columns(text.subslice(from, content - from));
            if (building && used + gap + w > width) {
                out.push_back(text.subslice(start, end - start));
                start = from;
                used = w;
            } else {
                used += gap + w;
                if (!building) {
                    start = from;
                    used = w;
                }
            }
            end = content;
            building = true;
            gap = columns(text.subslice(content, to - content));
            if (hard) {
                out.push_back(text.subslice(start, end - start));
                start = to;
                end = to;
                used = 0;
                gap = 0;
                building = false;
            }
        }
        if (building) {
            out.push_back(text.subslice(start, end - start));
        }
        return out;
    }

    inline vector<slice<const char>> wrap(const string& text, size_t width) {
        return wrap(text.as_slice(), width);
    }

    // truncate: the text cut to fit the given number of columns, with the
    // ellipsis counted inside that number and the cut made at a grapheme
    // boundary — never in the middle of a character, however many code
    // points it is. A text that already fits comes back as it was, the
    // same object; a limit too small for the ellipsis alone gives the
    // widest prefix of the ellipsis that fits.
    inline string truncate(const string& text, size_t width, const string& ellipsis) {
        if (columns(text) <= width) {
            return text;
        }
        size_t room = width > columns(ellipsis) ? width - columns(ellipsis) : 0;
        size_t used = 0;
        size_t cut = 0;
        for (auto g : graphemes(text)) {
            size_t w = columns(g);
            if (used + w > room) {
                break;
            }
            used += w;
            cut += g.size();
        }
        return text.substr(0, cut) + ellipsis;
    }

    inline string truncate(const string& text, size_t width) {
        return truncate(text, width, "…");
    }

    // The number of graphemes: what to count when a limit is a number of
    // characters a reader would count, where size() is bytes and
    // rune_count() code points
    inline size_t grapheme_count(slice<const char> text) noexcept {
        return graphemes(text).count();
    }

    inline size_t grapheme_count(const string& text) noexcept {
        return graphemes(text).count();
    }

    // Moving a cursor over the text: the start of the grapheme that holds
    // pos (pos itself when it is a boundary), the boundary after pos and
    // the last one before it. What a caret, a left arrow and a backspace
    // move by — never half a character, however many code points it is. A
    // position past the end is the end, and prev of 0 is 0; from a
    // position inside a grapheme, next goes to its end and prev to its
    // start, so a cursor that starts astray is put right by either.
    inline size_t grapheme_start(slice<const char> text, size_t pos) noexcept {
        auto v = text.view();
        if (pos >= v.size()) {
            return v.size();
        }
        size_t start = 0;
        while (start < pos) {
            size_t next = detail::cluster_end(v, start);
            if (next > pos) {
                break;
            }
            start = next;
        }
        return start;
    }

    inline size_t grapheme_next(slice<const char> text, size_t pos) noexcept {
        return detail::cluster_end(text.view(), grapheme_start(text, pos));
    }

    // Scanned from the start of the text, the rules of UAX #29 running one
    // way only: a slice is usually a line, and a cursor is not a loop
    inline size_t grapheme_prev(slice<const char> text, size_t pos) noexcept {
        auto v = text.view();
        pos = std::min(pos, v.size());
        size_t prev = 0;
        for (size_t start = 0; start < pos;) {
            size_t next = detail::cluster_end(v, start);
            if (next >= pos) {
                return start;
            }
            prev = start;
            start = next;
        }
        return prev;
    }

    inline size_t grapheme_start(const string& text, size_t pos) noexcept {
        return grapheme_start(text.as_slice(), pos);
    }

    inline size_t grapheme_next(const string& text, size_t pos) noexcept {
        return grapheme_next(text.as_slice(), pos);
    }

    inline size_t grapheme_prev(const string& text, size_t pos) noexcept {
        return grapheme_prev(text.as_slice(), pos);
    }
}
