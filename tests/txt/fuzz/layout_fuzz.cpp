//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::format over a pattern from outside, and the segmentation and bidi
// of any text, without an oracle but std::format. The first byte picks the
// path; a pattern and a text are split at a NUL. What must hold:
//   - format(runtime(pattern), ...) answers nullopt for a pattern that does
//     not fit its values and never throws for one (only {:c} of a number
//     no character holds may, as documented); what it answers is what
//     fits<...>() said, format_to's size is its size, and where
//     std::vformat takes the pattern too, the text is std's — widths of
//     controls, joiners and leading marks included; not compared: text
//     that is not UTF-8 (its width is the implementation's) or holds
//     CR LF (one cluster, two to libc++), and {:#g}
//     below one, where libc++ counts the zero before the point;
//   - graphemes, word_breaks, sentences and line_breaks cut the text into
//     pieces that are its bytes in order, none empty; words are among the
//     word pieces; grapheme_count counts the graphemes, and grapheme_next
//     and grapheme_prev step over their boundaries both ways; wrap's lines
//     lie in order inside the text;
//   - levels() has a level for every code point, none past 126;
//     visual_order is a permutation of the byte positions of code points
//     (the ones X9 removes left out); bidi_runs are pieces of the text that
//     cover the bytes visual_order names; mirrored() keeps the code point
//     count.
// Built with libFuzzer (tests/fuzz/run.sh tests/txt/fuzz/layout_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/txt/txt.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void formatting(std::string_view pattern, std::string_view text, int64_t n, double d, bool b) {
        string p(pattern);
        string s(text);
        optional<string> ours;
        bool threw = false;
        try {
            ours = txt::format(txt::runtime(p), n, d, s, b);
        } catch (const std::exception&) {
            threw = true;
        }
        if (threw) {
            // {:c} of a number no character holds, the one fault of a value
            check(pattern.find('c') != std::string_view::npos);
            return;
        }
        check(ours.has_value() == txt::fits<int64_t, double, string, bool>(txt::runtime(p)));
        if (!ours) {
            return;
        }
        char room[64];
        auto size = txt::format_to(slice<char>(tracked_ptr<const void>(), room, sizeof room), txt::runtime(p), n, d, s, b);
        check(size.has_value() && *size == ours->size());
        check(std::string_view(room, std::min(sizeof room, *size)) == ours->view().substr(0, std::min(sizeof room, *size)));
        // ill-formed UTF-8, whose width the standard leaves to the
        // implementation (a byte a U+FFFD here, libc++ its own count), is
        // not compared
        if (!utf8::valid(text)) {
            return;
        }
        // CR LF, one cluster to UAX #29 (GB3) and two to libc++
        if (text.find("\r\n") != std::string_view::npos) {
            return;
        }
        // libc++ counts the zero before the point of a value below one
        // among the digits of {:#g} ({:#.3g} of 0.5 is "0.50" there,
        // "0.500" in printf and here): '#' with g is not compared
        if (pattern.find('#') != std::string_view::npos
            && (pattern.find('g') != std::string_view::npos || pattern.find('G') != std::string_view::npos)) {
            return;
        }
        std::string theirs;
        try {
            std::string st(text);
            theirs = std::vformat(std::string_view(pattern), std::make_format_args(n, d, st, b));
        } catch (const std::exception&) {
            return;   // a pattern std refuses: nothing to compare
        }
        check(ours->view() == theirs);
    }

    // the pieces as offsets and sizes: a slice holds a tracked pointer,
    // which does not live in the standard vector's memory
    struct Piece {
        size_t at;
        size_t size;
    };

    template<class Range>
    std::vector<Piece> pieces_of(const Range& r, const string& text) {
        std::vector<Piece> out;
        size_t at = 0;
        for (auto piece : r) {
            check(!piece.empty());
            check(piece.data() == text.data() + at);
            out.push_back({at, piece.size()});
            at += piece.size();
        }
        check(at == text.size());
        return out;
    }

    void segments(const string& text) {
        auto g = pieces_of(txt::graphemes(text), text);
        check(txt::grapheme_count(text) == g.size());
        size_t at = 0;
        for (auto& piece : g) {
            size_t next = txt::grapheme_next(text, at);
            check(next == at + piece.size);
            check(txt::grapheme_start(text, at) == at);
            at = next;
        }
        for (size_t k = g.size(); k > 0; --k) {
            size_t prev = txt::grapheme_prev(text, at);
            check(prev == at - g[k - 1].size);
            at = prev;
        }
        check(at == 0);
        auto w = pieces_of(txt::word_breaks(text), text);
        size_t k = 0;
        for (auto word : txt::words(text)) {
            size_t word_at = size_t(word.data() - text.data());
            while (k < w.size() && w[k].at != word_at) {
                ++k;
            }
            check(k < w.size() && w[k].size == word.size());
        }
        (void)pieces_of(txt::sentences(text), text);
        (void)pieces_of(txt::line_breaks(text), text);
        size_t width = 1 + text.size() % 13;
        const char* last = text.data();
        for (auto line : txt::wrap(text, width)) {
            check(line.data() >= last && line.data() + line.size() <= text.data() + text.size());
            last = line.data() + line.size();
        }
    }

    void bidi(const string& text, uint8_t which) {
        auto paragraph = txt::direction(which % 3);
        auto levels = txt::levels(text, paragraph);
        check(levels.size() == text.rune_count());
        for (auto l : levels) {
            check(l <= 126);
        }
        auto order = txt::visual_order(text, paragraph);
        std::vector<size_t> starts;
        for (size_t i = 0; i < text.size();) {
            starts.push_back(i);
            i += utf8::decode(text.view(), i).second;
        }
        std::vector<size_t> sorted(order.begin(), order.end());
        std::sort(sorted.begin(), sorted.end());
        check(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
        for (auto at : sorted) {
            check(std::binary_search(starts.begin(), starts.end(), at));
        }
        size_t covered = 0;
        for (auto& run : txt::bidi_runs(text, paragraph)) {
            check(!run.text.empty());
            check(run.text.data() >= text.data() && run.text.data() + run.text.size() <= text.data() + text.size());
            check(run.level <= 126);
            covered += run.text.size();
        }
        check(covered <= text.size());
        auto m = txt::mirrored(text, paragraph);
        check(m.rune_count() == text.rune_count());
        (void)txt::paragraph_direction(text);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 8192) {
        return 0;
    }
    uint8_t mode = data[0];
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (mode % 3) {
        case 0: {
            size_t nul = rest.find('\0');
            std::string_view pattern = rest.substr(0, nul);
            std::string_view text = nul == std::string_view::npos ? std::string_view() : rest.substr(nul + 1);
            int64_t n = 0;
            double d = 0;
            std::memcpy(&n, text.data(), std::min(sizeof n, text.size()));
            if (text.size() >= 16) {
                std::memcpy(&d, text.data() + 8, sizeof d);
            }
            formatting(pattern, text, n, d, (mode >> 2) & 1);
            break;
        }
        case 1: segments(string(rest)); break;
        case 2: bidi(string(rest), mode >> 2); break;
    }
    return 0;
}
