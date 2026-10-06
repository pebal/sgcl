//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::align_tabs and txt::tab_writer against Go's text/tabwriter: the
// tables of tests/txt/tab_vectors.h (tools/tab_vectors.py) aligned whole and
// written a byte at a time; then the widths of a terminal (wide and
// combining characters, where Go counts code points), the options one by
// one, the streaming and the boundaries.
#include "tests/types.h"
#include "tests/txt/tab_vectors.h"

#include <string>

namespace {
    string s(const char* t) {
        return string(t);
    }

    std::string str(const string& t) {
        return std::string(t.view());
    }

    std::string aligned(const char* text, const txt::tab_options& o = {}) {
        return str(txt::align_tabs(s(text), o));
    }
}

TEST(TabWriter_Tests, GoVectors) {
    size_t n = 0;
    for (const auto& v : TabVectors) {
        txt::tab_options o{.min_width = v.min_width, .tab_width = v.tab_width, .padding = v.padding,
                           .pad_char = v.pad_char, .align_right = (v.flags & 4) != 0,
                           .discard_empty_columns = (v.flags & 8) != 0, .tab_indent = (v.flags & 16) != 0,
                           .filter_html = (v.flags & 1) != 0, .strip_escape = (v.flags & 2) != 0,
                           .debug = (v.flags & 32) != 0};
        std::string text(v.text, v.text_size), want(v.result, v.result_size);
        EXPECT_EQ(str(txt::align_tabs(string(std::string_view(text)), o)), want) << text;
        // a byte at a time
        txt::tab_writer w(o);
        std::string got;
        for (char c : text) {
            got += str(w.write(string(std::string_view(&c, 1))));
        }
        got += str(w.flush());
        EXPECT_EQ(got, want) << text;
        ++n;
    }
    EXPECT_EQ(n, 1000u);
}

TEST(TabWriter_Tests, Basics) {
    EXPECT_EQ(aligned("a\tb\tc\naaa\tbbbbb\tc\n"), "a   b     c\naaa bbbbb c\n");
    // the last cell of a line is in no column
    EXPECT_EQ(aligned("a\tlong last\nbbb\tc\n"), "a   long last\nbbb c\n");
    // a line of one cell ends the block: the next columns start afresh
    EXPECT_EQ(aligned("a\tb\n\nccc\td\n"), "a b\n\nccc d\n");
    // a form feed ends every column, and is written as a line feed
    EXPECT_EQ(aligned("a\tb\fccc\td\n"), "a b\nccc d\n");
    // nested blocks: column 1 only where the lines have it
    EXPECT_EQ(aligned("a\tb\tc\naa\tb\nx\tyyyy\tz\n"), "a  b c\naa b\nx  yyyy z\n");
    // no line feed at the end: the last line written as it is
    EXPECT_EQ(aligned("a\tb\nccc\td"), "a   b\nccc d");
    EXPECT_EQ(aligned(""), "");
    EXPECT_EQ(aligned("no tabs\n"), "no tabs\n");
    EXPECT_EQ(aligned("\t\t\n"), "  \n");
}

TEST(TabWriter_Tests, TerminalWidths) {
    // a wide character takes two columns, a combining mark none
    EXPECT_EQ(aligned("名前\tsize\nab\t1\n"), "名前 size\nab   1\n");
    EXPECT_EQ(aligned("e\xCC\x81\tx\nab\ty\n"), "e\xCC\x81  x\nab y\n");
    EXPECT_EQ(aligned("ąę\tx\nabc\ty\n"), "ąę  x\nabc y\n");
    // invalid UTF-8: a byte one column, as Go counts it
    EXPECT_EQ(aligned("\xC3\tx\nab\ty\n"), "\xC3  x\nab y\n");
    // an emoji is wide
    EXPECT_EQ(aligned("\xF0\x9F\x99\x82\tx\nabc\ty\n"), "\xF0\x9F\x99\x82  x\nabc y\n");
}

TEST(TabWriter_Tests, Options) {
    EXPECT_EQ(aligned("a\tb\nccc\td\n", {.min_width = 6}), "a     b\nccc   d\n");
    EXPECT_EQ(aligned("a\tb\nccc\td\n", {.padding = 3}), "a     b\nccc   d\n");
    EXPECT_EQ(aligned("a\tb\nccc\td\n", {.pad_char = '.'}), "a...b\nccc.d\n");
    EXPECT_EQ(aligned("a\tb\nccc\td\n", {.align_right = true}), "   ab\n cccd\n");
    // tabs: columns at tab stops, cells left-aligned whatever align_right says
    EXPECT_EQ(aligned("a\tb\nccccccccc\td\n", {.pad_char = '\t', .align_right = true}),
              "a\t\tb\nccccccccc\td\n");
    EXPECT_EQ(aligned("a\tb\n", {.tab_width = 4, .pad_char = '\t'}), "a\tb\n");
    EXPECT_EQ(aligned("a\tb\n", {.tab_width = 0, .pad_char = '\t'}), "ab\n");
    EXPECT_EQ(aligned("a\tb\tc\n", {.debug = true}), "a |b |c\n");
    EXPECT_EQ(aligned("a\tb\fc\td\n", {.debug = true}), "a |b\n---\nc |d\n");
    // leading empty cells as tabs
    EXPECT_EQ(aligned("\tx\n\t\ty\n", {.tab_indent = true}), "\tx\n\t\ty\n");
    // an empty column of soft tabs dropped; a hard tab keeps it
    EXPECT_EQ(aligned("a\v\vb\nc\v\vd\n", {.discard_empty_columns = true}), "a b\nc d\n");
    EXPECT_EQ(aligned("a\t\tb\nc\t\td\n", {.discard_empty_columns = true}), "a  b\nc  d\n");
    // HTML: a tag no width, an entity one
    EXPECT_EQ(aligned("<b>a</b>\tx\n&amp;bc\ty\n", {.filter_html = true}), "<b>a</b>   x\n&amp;bc y\n");
    // escapes: their text one piece, its tabs included; stripped or kept
    EXPECT_EQ(aligned("\xFF" "a\tb\xFF\tx\nc\ty\n"), "\xFF" "a\tb\xFF x\nc   y\n");
    EXPECT_EQ(aligned("\xFF" "a\tb\xFF\tx\nc\ty\n", {.strip_escape = true}), "a\tb x\nc   y\n");
}

TEST(TabWriter_Tests, Streaming) {
    txt::tab_writer w;
    EXPECT_TRUE(w.write(s("a\tb\n")).empty());           // the block is open
    EXPECT_EQ(str(w.write(s("ccc\td\ne\n"))), "a   b\nccc d\ne\n");
    EXPECT_TRUE(w.write(s("x\ty")).empty());
    EXPECT_EQ(str(w.flush()), "x y");
    EXPECT_TRUE(w.flush().empty());
    // a form feed gives back what it ended
    EXPECT_EQ(str(w.write(s("p\tq\f"))), "p q\n");
    // copies share the writer
    txt::tab_writer copy = w;
    EXPECT_TRUE(copy.write(s("1\t2\n")).empty());
    EXPECT_EQ(str(w.flush()), "1 2\n");
    // a cell cut inside a code point and inside an escape
    txt::tab_writer cut;
    std::string got;
    for (const char* piece : {"\xC4", "\x85\t", "x\n\xFF", "a\tb", "\xFF\ty\nzzz\t", "w\n", "\n"}) {
        got += str(cut.write(s(piece)));
    }
    got += str(cut.flush());
    EXPECT_EQ(got, str(txt::align_tabs(s("\xC4\x85\tx\n\xFF" "a\tb\xFF\ty\nzzz\tw\n\n"))));
    // a default writer flushes nothing
    txt::tab_writer none;
    EXPECT_TRUE(none.flush().empty());
}

TEST(TabWriter_Tests, Large) {
    std::string text;
    for (int i = 0; i < 20000; ++i) {
        text += std::to_string(i) + "\t" + std::string(size_t(i % 13), 'x') + "\tend\n";
    }
    std::string out = str(txt::align_tabs(string(std::string_view(text))));
    // every line: the number padded to 6, the xs to 13
    EXPECT_EQ(out.substr(0, out.find('\n')), "0" + std::string(5 + 13, ' ') + "end");
    EXPECT_EQ(out.find('\t'), std::string::npos);
    size_t lines = 0;
    for (char c : out) {
        lines += c == '\n';
    }
    EXPECT_EQ(lines, 20000u);
}
