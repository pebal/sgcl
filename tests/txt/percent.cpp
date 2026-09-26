//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::percent: the escaping of RFC 3986, and the sets the WHATWG URL
// Standard uses instead. The oracle is the two specifications themselves.
#include "tests/types.h"
#include "sgcl/txt/percent.h"

#include <string>

// The escaping of RFC 3986. The set of characters left alone is the
// argument because the RFC gives a different one for every part of a URL,
// and a program that uses one set everywhere writes a slash where it
// meant a separator or a separator where it meant a slash.
TEST(Percent_Tests, Encode) {
    string path("/a b/c?d");
    EXPECT_EQ(txt::percent::encode(path), string("%2Fa%20b%2Fc%3Fd"));
    EXPECT_EQ(txt::percent::encode(path, txt::percent::path), string("/a%20b/c%3Fd"));
    EXPECT_EQ(txt::percent::encode(path, txt::percent::query), string("/a%20b/c?d"));
    EXPECT_EQ(txt::percent::encode(path, txt::percent::segment), string("%2Fa%20b%2Fc%3Fd"));

    // The unreserved set of section 2.3 is never escaped anywhere
    string plain("aZ0-._~");
    EXPECT_EQ(txt::percent::encode(plain), plain);

    // A byte above ASCII is always escaped, one escape a byte: a URL
    // carries bytes, and "ü" is two of them in UTF-8
    EXPECT_EQ(txt::percent::encode(string("\xC3\xBC")), string("%C3%BC"));
    // and the digits are upper case, which section 6.2.2.1 asks for
    EXPECT_EQ(txt::percent::encode(string("\x1F")), string("%1F"));

    // The sets compose, so a program with a set of its own says so
    auto mine = txt::percent::unreserved | txt::percent_set{"/"};
    EXPECT_EQ(txt::percent::encode(path, mine), string("/a%20b/c%3Fd"));
}

TEST(Percent_Tests, Decode) {
    EXPECT_EQ(*txt::percent::decode(string("%2Fa%20b")), string("/a b"));
    EXPECT_EQ(*txt::percent::decode(string("plain")), string("plain"));
    EXPECT_EQ(*txt::percent::decode(string("%C3%BC")), string("\xC3\xBC"));
    // either case of the digits, as section 6.2.2.1 says a consumer must
    EXPECT_EQ(*txt::percent::decode(string("%c3%bc")), *txt::percent::decode(string("%C3%BC")));

    // A truncated escape is not a text with a per cent sign in it. It is
    // a text that was cut, and letting it through is how something gets
    // past a check that ran before the decoding
    EXPECT_FALSE(txt::percent::decode(string("%")).has_value());
    EXPECT_FALSE(txt::percent::decode(string("%2")).has_value());
    EXPECT_FALSE(txt::percent::decode(string("a%")).has_value());
    EXPECT_FALSE(txt::percent::decode(string("%zz")).has_value());
    EXPECT_FALSE(txt::percent::decode(string("%2g")).has_value());

    // The bytes that come back need not be text at all: an escape can
    // spell a byte no UTF-8 has, and the decoding is over bytes
    auto raw = txt::percent::decode(string("%FF%FE"));
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(raw->size(), 2u);

    // Everything the encoder writes reads back
    for (auto& s : {"", "a b", "\xC3\xBC/x?y#z", "%", "100%25"}) {
        string text(s);
        EXPECT_EQ(*txt::percent::decode(txt::percent::encode(text)), text) << s;
    }
}

// The other family. The WHATWG URL Standard writes its sets the other way
// round — as what is to be escaped, over "C0 controls and everything above
// ~" — and escapes a good deal less than RFC 3986 does, which is what a
// browser has to do for names and paths already in the wild.
TEST(Percent_Tests, WhatwgSets) {
    namespace w = txt::percent::whatwg;

    // Every printable ASCII character through each set, against the
    // standard's own lists. What is escaped is exactly what the standard
    // names and nothing else.
    auto escaped_by = [](txt::percent_set set) {
        std::string out;
        for (int c = 0x20; c <= 0x7E; ++c) {
            if (!set.holds(char(c))) {
                out.push_back(char(c));
            }
        }
        return out;
    };
    EXPECT_EQ(escaped_by(w::c0), "");
    EXPECT_EQ(escaped_by(w::fragment), " \"<>`");
    EXPECT_EQ(escaped_by(w::query), " \"#<>");
    EXPECT_EQ(escaped_by(w::special_query), " \"#'<>");
    EXPECT_EQ(escaped_by(w::path), " \"#<>?^`{}");
    EXPECT_EQ(escaped_by(w::userinfo), " \"#/:;<=>?@[\\]^`{|}");
    EXPECT_EQ(escaped_by(w::component), " \"#$%&+,/:;<=>?@[\\]^`{|}");

    // The standard states the form-urlencoded set as a complement, in as
    // many words: everything except the ASCII alphanumeric and * - . _
    std::string kept;
    for (int c = 0x20; c <= 0x7E; ++c) {
        if (w::form_urlencoded.holds(char(c))) {
            kept.push_back(char(c));
        }
    }
    EXPECT_EQ(kept, "*-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz");

    // And the component set is encodeURIComponent, character for character
    std::string component;
    for (int c = 0x20; c <= 0x7E; ++c) {
        if (w::component.holds(char(c))) {
            component.push_back(char(c));
        }
    }
    EXPECT_EQ(component, "!'()*-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~");

    // The query set is not the fragment set less anything, which is the
    // one place the standard stops to say so: it leaves the backquote out
    EXPECT_TRUE(w::query.holds('`'));
    EXPECT_FALSE(w::fragment.holds('`'));

    // Where the two families differ in practice: a browser leaves a
    // sub-delim in a path alone and so does RFC 3986's path, but the
    // unreserved set escapes it
    string path("/a+b,c/d e");
    EXPECT_EQ(txt::percent::encode(path, w::path), string("/a+b,c/d%20e"));
    EXPECT_EQ(txt::percent::encode(path, txt::percent::path), string("/a+b,c/d%20e"));
    EXPECT_EQ(txt::percent::encode(path, txt::percent::unreserved),
              string("%2Fa%2Bb%2Cc%2Fd%20e"));
    // and a byte above ASCII is escaped by every one of them
    EXPECT_EQ(txt::percent::encode(string("\xC3\xBC"), w::c0), string("%C3%BC"));

    // Only the two sets that escape the per cent sign round trip anything
    string tricky("a%2Fb");
    EXPECT_EQ(*txt::percent::decode(txt::percent::encode(tricky, w::component)), tricky);
    EXPECT_NE(*txt::percent::decode(txt::percent::encode(tricky, w::path)), tricky);
}
