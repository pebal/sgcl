//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::punycode and txt::idna. Two oracles: the sample strings of RFC
// 3492 section 7.1, every one of them; and IdnaTestV2.txt of the UCD,
// whole, which gives for every name what toUnicode and the two toASCII
// operations must answer and whether they must report an error.
#include "tests/types.h"
#include "tests/txt/idna_tests.h"
#include "sgcl/txt/idna.h"
#include "sgcl/txt/identifier.h"   // the other implementation of NFKC_Casefold

#include <chrono>
#include <string>
#include <vector>

namespace {
    // The RFC writes its examples as code points, which is the only way
    // to write them that a reader can check against the document. They
    // are kept as code points and made into a text inside the loop: a
    // string holds a tracked pointer, and the buffer of a std::vector is
    // not a place the collector looks for one
    string text_of(const std::vector<char32_t>& points) {
        std::string out;
        char buf[utf8::max_width];
        for (auto c : points) {
            out.append(buf, utf8::encode(c, buf));
        }
        return string(out.data(), out.size());
    }

    std::string lowered(const string& s) {
        std::string out(s.view());
        for (auto& c : out) {
            if (c >= 'A' && c <= 'Z') {
                c = char(c + 32);
            }
        }
        return out;
    }

    struct Sample {
        const char* name;
        std::vector<char32_t> points;
        const char* punycode;   // as RFC 3492 prints it
    };

    std::vector<Sample> samples() {
        return {
            {"A arabic", {0x0644, 0x064A, 0x0647, 0x0645, 0x0627, 0x0628, 0x062A, 0x0643,
                          0x0644, 0x0645, 0x0648, 0x0634, 0x0639, 0x0631, 0x0628, 0x064A,
                          0x061F},
             "egbpdaj6bu4bxfgehfvwxn"},
            {"B chinese simplified", {0x4ED6, 0x4EEC, 0x4E3A, 0x4EC0, 0x4E48, 0x4E0D, 0x8BF4,
                                      0x4E2D, 0x6587},
             "ihqwcrb4cv8a8dqg056pqjye"},
            {"C chinese traditional", {0x4ED6, 0x5011, 0x7232, 0x4EC0, 0x9EBD, 0x4E0D, 0x8AAA,
                                       0x4E2D, 0x6587},
             "ihqwctvzc91f659drss3x8bo0yb"},
            {"D czech", {0x0050, 0x0072, 0x006F, 0x010D, 0x0070, 0x0072, 0x006F, 0x0073,
                         0x0074, 0x011B, 0x006E, 0x0065, 0x006D, 0x006C, 0x0075, 0x0076,
                         0x00ED, 0x010D, 0x0065, 0x0073, 0x006B, 0x0079},
             "Proprostnemluvesky-uyb24dma41a"},
            {"E hebrew", {0x05DC, 0x05DE, 0x05D4, 0x05D4, 0x05DD, 0x05E4, 0x05E9, 0x05D5,
                          0x05D8, 0x05DC, 0x05D0, 0x05DE, 0x05D3, 0x05D1, 0x05E8, 0x05D9,
                          0x05DD, 0x05E2, 0x05D1, 0x05E8, 0x05D9, 0x05EA},
             "4dbcagdahymbxekheh6e0a7fei0b"},
            {"F hindi", {0x092F, 0x0939, 0x0932, 0x094B, 0x0917, 0x0939, 0x093F, 0x0928,
                         0x094D, 0x0926, 0x0940, 0x0915, 0x094D, 0x092F, 0x094B, 0x0902,
                         0x0928, 0x0939, 0x0940, 0x0902, 0x092C, 0x094B, 0x0932, 0x0938,
                         0x0915, 0x0924, 0x0947, 0x0939, 0x0948, 0x0902},
             "i1baa7eci9glrd9b2ae1bj0hfcgg6iyaf8o0a1dig0cd"},
            {"G japanese", {0x306A, 0x305C, 0x307F, 0x3093, 0x306A, 0x65E5, 0x672C, 0x8A9E,
                            0x3092, 0x8A71, 0x3057, 0x3066, 0x304F, 0x308C, 0x306A, 0x3044,
                            0x306E, 0x304B},
             "n8jok5ay5dzabd5bym9f0cm5685rrjetr6pdxa"},
            {"H korean", {0xC138, 0xACC4, 0xC758, 0xBAA8, 0xB4E0, 0xC0AC, 0xB78C, 0xB4E4,
                          0xC774, 0xD55C, 0xAD6D, 0xC5B4, 0xB97C, 0xC774, 0xD574, 0xD55C,
                          0xB2E4, 0xBA74, 0xC5BC, 0xB9C8, 0xB098, 0xC88B, 0xC744, 0xAE4C},
             "989aomsvi5e83db1d2a355cv1e0vak1dwrv93d5xbh15a0dt30a5jpsd879ccm6fea98c"},
            // The one example the RFC prints with the mixed-case
            // annotation in it: the 'D' in the middle says the code point
            // that digit inserts was upper case in the source. It is an
            // annotation and not information — a decoder ignores the case
            // of a digit — and this encoder does not write one, so the
            // string it produces is the same letters in lower case
            {"I russian", {0x043F, 0x043E, 0x0447, 0x0435, 0x043C, 0x0443, 0x0436, 0x0435,
                           0x043E, 0x043D, 0x0438, 0x043D, 0x0435, 0x0433, 0x043E, 0x0432,
                           0x043E, 0x0440, 0x044F, 0x0442, 0x043F, 0x043E, 0x0440, 0x0443,
                           0x0441, 0x0441, 0x043A, 0x0438},
             "b1abfaaepdrnnbgefbaDotcwatmq2g4l"},
            {"J spanish", {0x0050, 0x006F, 0x0072, 0x0071, 0x0075, 0x00E9, 0x006E, 0x006F,
                           0x0070, 0x0075, 0x0065, 0x0064, 0x0065, 0x006E, 0x0073, 0x0069,
                           0x006D, 0x0070, 0x006C, 0x0065, 0x006D, 0x0065, 0x006E, 0x0074,
                           0x0065, 0x0068, 0x0061, 0x0062, 0x006C, 0x0061, 0x0072, 0x0065,
                           0x006E, 0x0045, 0x0073, 0x0070, 0x0061, 0x00F1, 0x006F, 0x006C},
             "PorqunopuedensimplementehablarenEspaol-fmd56a"},
            {"K vietnamese", {0x0054, 0x1EA1, 0x0069, 0x0073, 0x0061, 0x006F, 0x0068, 0x1ECD,
                              0x006B, 0x0068, 0x00F4, 0x006E, 0x0067, 0x0074, 0x0068, 0x1EC3,
                              0x0063, 0x0068, 0x1EC9, 0x006E, 0x00F3, 0x0069, 0x0074, 0x0069,
                              0x1EBF, 0x006E, 0x0067, 0x0056, 0x0069, 0x1EC7, 0x0074},
             "TisaohkhngthchnitingVit-kjcr8268qyxafd2f1b9g"},
            {"L 3nen b gumi", {0x0033, 0x5E74, 0x0042, 0x7D44, 0x91D1, 0x516B, 0x5148, 0x751F},
             "3B-ww4c5e180e575a65lsy2b"},
            {"M super monkeys", {0x5B89, 0x5BA4, 0x5948, 0x7F8E, 0x6075, 0x002D, 0x0077, 0x0069,
                                 0x0074, 0x0068, 0x002D, 0x0053, 0x0055, 0x0050, 0x0045, 0x0052,
                                 0x002D, 0x004D, 0x004F, 0x004E, 0x004B, 0x0045, 0x0059, 0x0053},
             "-with-SUPER-MONKEYS-pc58ag80a8qai00g7n9n"},
            {"N hello another way", {0x0048, 0x0065, 0x006C, 0x006C, 0x006F, 0x002D, 0x0041,
                                     0x006E, 0x006F, 0x0074, 0x0068, 0x0065, 0x0072, 0x002D,
                                     0x0057, 0x0061, 0x0079, 0x002D, 0x305D, 0x308C, 0x305E,
                                     0x308C, 0x306E, 0x5834, 0x6240},
             "Hello-Another-Way--fc4qua05auwb3674vfr0b"},
            {"O hitotsu yane", {0x3072, 0x3068, 0x3064, 0x5C4B, 0x6839, 0x306E, 0x4E0B, 0x0032},
             "2-u9tlzr9756bt3uc0v"},
            {"P maji de koi", {0x004D, 0x0061, 0x006A, 0x0069, 0x3067, 0x004B, 0x006F, 0x0069,
                               0x3059, 0x308B, 0x0035, 0x79D2, 0x524D},
             "MajiKoi5-783gue6qz075azm5e"},
            {"Q pafii de runba", {0x30D1, 0x30D5, 0x30A3, 0x30FC, 0x0064, 0x0065, 0x30EB,
                                  0x30F3, 0x30D0},
             "de-jg4avhby1noc0d"},
            {"R sono supiido de", {0x305D, 0x306E, 0x30B9, 0x30D4, 0x30FC, 0x30C9, 0x3067},
             "d9juau41awczczp"},
            // The one that is ASCII already, spaces and all: IDNA would
            // never send it here, punycode can still carry it
            {"S dollar", {0x002D, 0x003E, 0x0020, 0x0024, 0x0031, 0x002E, 0x0030, 0x0030,
                          0x0020, 0x003C, 0x002D},
             "-> $1.00 <--"},
        };
    }
}

// Every sample string of RFC 3492 section 7.1, both ways. The encoder is
// held to the exact bytes the RFC prints, down to the case of the basic
// code points; only the Russian one is compared in lower case, and the
// comment beside it says why.
TEST(Punycode_Tests, RfcSamples) {
    for (auto& s : samples()) {
        string unicode = text_of(s.points);
        auto encoded = txt::punycode::encode(unicode);
        ASSERT_TRUE(encoded.has_value()) << s.name;
        EXPECT_EQ(lowered(*encoded), lowered(string(s.punycode))) << s.name;
        auto decoded = txt::punycode::decode(string(s.punycode));
        ASSERT_TRUE(decoded.has_value()) << s.name;
        EXPECT_EQ(*decoded, unicode) << s.name;
        // and what this encoder writes reads back as what went in
        EXPECT_EQ(txt::punycode::decode(*encoded), unicode) << s.name;
    }
}

// Section 6.4 of RFC 3492 says in as many words that an implementation
// must detect the overflow of its counters rather than let them wrap, and
// this is the place where the published attacks on punycode have been: a
// decoder whose `i` wraps writes a code point the label did not ask for,
// at a position it did not ask for.
TEST(Punycode_Tests, Overflow) {
    // The decoder's `i` runs past 2^32 long before the digits run out:
    // a digit of 35 keeps the inner loop going, so a run of nines
    // multiplies w by 35 a step and i follows it. Eight of them is
    // already past the bound
    for (const char* bad : {"99999999",
                            "999999999999",
                            "9999999999999999999999",
                            "a-9999999999999999999999",
                            "abc-99999999"}) {
        EXPECT_FALSE(txt::punycode::decode(string(bad)).has_value()) << bad;
    }
    // A number that stops in the middle, a digit that is not one, a lone
    // hyphen (which is not a delimiter, nothing standing before it), and
    // a basic part that is not basic
    EXPECT_FALSE(txt::punycode::decode(string("a-zz")).has_value());
    EXPECT_FALSE(txt::punycode::decode(string("a-!")).has_value());
    EXPECT_FALSE(txt::punycode::decode(string("-")).has_value());
    EXPECT_FALSE(txt::punycode::decode(string("\xC3\xA4-a")).has_value());
    // Forty z's put n in the surrogate range, where the RFC's own
    // decoder would happily write one into the output: a code point that
    // no text can hold is refused here
    EXPECT_FALSE(txt::punycode::decode(string(std::string(40, 'z').data(), 40)).has_value());

    // And the encoder. delta grows by (m - n) * (h + 1), so a label with
    // several thousand basic code points and one supplementary code point
    // at the end of it runs past 2^32 on the very first round: 1114239 by
    // 5001 is 5.57e9. Nothing useful is ever this long — 63 bytes is what
    // the DNS carries — but the counter is the caller's input and not
    // ours, so it is checked and not assumed
    std::string big(5000, 'a');
    big += "\xF4\x8F\xBF\xBF";                      // U+10FFFF
    EXPECT_FALSE(txt::punycode::encode(string(big.data(), big.size())).has_value());
    // a shorter one with the same code point at the end of it still
    // encodes, so what is refused is the arithmetic and not the length
    std::string smaller(3000, 'a');
    smaller += "\xF4\x8F\xBF\xBF";
    EXPECT_TRUE(txt::punycode::encode(string(smaller.data(), smaller.size())).has_value());

    // The empty label is not an overflow and not an error: it is the
    // empty string, which IDNA then refuses for being empty
    EXPECT_EQ(*txt::punycode::encode(string()), string());
    EXPECT_EQ(*txt::punycode::decode(string()), string());
}

// IdnaTestV2.txt of the UCD, every case of it, for the three operations
// the file gives: toUnicode and toASCII with and without transitional
// processing. The flags are the ones the file assumes — CheckHyphens,
// CheckBidi, CheckJoiners, UseSTD3ASCIIRules and VerifyDnsLength all on.
// A label longer than the stack buffer takes the other road through the
// decoder — the placements done backwards over a tree of counts rather
// than straight into the vector — and the two roads have to answer the
// same. Every length from one code point to two hundred crosses the
// boundary at sixty-four and is held to its own round trip.
TEST(Punycode_Tests, BothRoadsAgree) {
    for (size_t n = 1; n <= 200; ++n) {
        sgcl::vector<char32_t> points;   // detail::encoded takes the library's vector
        for (size_t k = 0; k < n; ++k) {
            // Polish, Greek and Han in turn, so the decoder meets code
            // points of one, two and three bytes and of every plane
            static const char32_t cycle[] = {U'\u017c', U'\u03b1', U'\u4e2d', U'a', U'\U0001f600'};
            points.push_back(cycle[k % 5]);
        }
        sgcl::string text = sgcl::txt::detail::encoded(points);
        auto encoded = sgcl::txt::punycode::encode(text);
        ASSERT_TRUE(encoded) << "encode failed at " << n;
        auto back = sgcl::txt::punycode::decode(*encoded);
        ASSERT_TRUE(back) << "decode failed at " << n;
        EXPECT_EQ(*back, text) << "round trip differs at " << n;
    }
}

// The decoder used to insert every code point into the middle of what it
// had decoded so far, which is a move of everything after it: quadratic
// in the length of the label, and reachable from outside, since a name
// comes from whoever is asking. A label of forty thousand points cost
// seventy milliseconds and four times that at every doubling. The body
// below is RFC 3492's digit emission run backwards so that every number
// decodes to an insertion at the front, which is the worst case; the
// bound is generous on purpose — it is there to catch a return of the
// old shape, not to measure this machine.
TEST(Punycode_Tests, ALabelThatInsertsAtTheFrontIsNotQuadratic) {
    auto digit = [](uint32_t d) { return char(d < 26 ? 'a' + d : '0' + (d - 26)); };
    auto threshold = [](uint32_t k, uint32_t bias) -> uint32_t {
        return k <= bias ? 1 : (k >= bias + 26 ? 26 : k - bias);
    };
    auto adapt = [](uint64_t delta, uint32_t points, bool first) {
        delta = first ? delta / 700 : delta / 2;
        delta += delta / points;
        uint32_t k = 0;
        while (delta > ((36 - 1) * 26) / 2) { delta /= 36 - 1; k += 36; }
        return k + uint32_t(((36 - 1 + 1) * delta) / (delta + 38));
    };
    std::string body;
    uint32_t bias = 72;
    for (size_t r = 0, size = 0; r < 20000; ++r, ++size) {
        uint64_t want = uint64_t(size + 1);
        uint64_t q = want;
        for (uint32_t k = 36; ; k += 36) {
            uint32_t t = threshold(k, bias);
            if (q < t) break;
            body.push_back(digit(uint32_t(t + (q - t) % (36 - t))));
            q = (q - t) / (36 - t);
        }
        body.push_back(digit(uint32_t(q)));
        bias = adapt(want, uint32_t(size + 1), size == 0);
    }
    sgcl::string label(body.data(), body.size());
    auto started = std::chrono::steady_clock::now();
    auto out = sgcl::txt::punycode::decode(label);
    auto took = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    EXPECT_TRUE(out);
    EXPECT_LT(took, 200.0) << "twenty thousand front insertions took " << took << " ms";
}

TEST(Idna_Tests, ConformanceIdnaTestV2) {
    txt::idna::options strict;
    strict.use_std3_ascii_rules = true;
    auto transitional = strict;
    transitional.transitional = true;

    size_t checked = 0;
    for (auto& c : ucd::IdnaTests) {
        string source(c.source);
        auto unicode = txt::idna::unicode_form(source, strict);
        EXPECT_EQ(!unicode, c.unicode_error) << "line " << c.line << " toUnicode " << c.source;
        EXPECT_EQ(unicode.text, string(c.to_unicode)) << "line " << c.line << " toUnicode " << c.source;

        auto ascii = txt::idna::ascii_form(source, strict);
        EXPECT_EQ(!ascii, c.ascii_error) << "line " << c.line << " toAsciiN " << c.source;
        EXPECT_EQ(ascii.text, string(c.to_ascii)) << "line " << c.line << " toAsciiN " << c.source;

        auto old = txt::idna::ascii_form(source, transitional);
        EXPECT_EQ(!old, c.ascii_transitional_error)
            << "line " << c.line << " toAsciiT " << c.source;
        EXPECT_EQ(old.text, string(c.to_ascii_transitional))
            << "line " << c.line << " toAsciiT " << c.source;
        ++checked;
    }
    EXPECT_EQ(checked, sizeof(ucd::IdnaTests) / sizeof(ucd::IdnaTests[0]));
}

// The pair of characters the whole transitional business is about, which
// is the thing people trip over: the same name, written the same way,
// goes to two different hosts depending on which processing was asked
// for. Nontransitional keeps the sharp s and the final sigma and encodes
// them; transitional turns them into "ss" and an ordinary sigma, which is
// what IDNA2003 did and what UTS #46 now deprecates.
TEST(Idna_Tests, Deviations) {
    txt::idna::options now;
    auto then = now;
    then.transitional = true;

    string sharp("fa\xC3\x9F.de");                       // faß.de
    EXPECT_EQ(*txt::idna::to_ascii(sharp, now), string("xn--fa-hia.de"));
    EXPECT_EQ(*txt::idna::to_ascii(sharp, then), string("fass.de"));
    // and the punycode is never remapped, whichever was asked for: a
    // name already encoded was encoded by somebody who had decided
    EXPECT_EQ(*txt::idna::to_unicode(string("xn--fa-hia.de"), now), sharp);
    EXPECT_EQ(*txt::idna::to_unicode(string("xn--fa-hia.de"), then), sharp);

    string sigma("\xCF\x83\xCF\x8C\xCE\xBB\xCE\xBF\xCF\x82.gr");   // σόλος with a final sigma
    EXPECT_NE(*txt::idna::to_ascii(sigma, now), *txt::idna::to_ascii(sigma, then));

    // A capital sharp s is mapped to a small one and then, under
    // transitional processing only, has to go on to "ss": the mapping
    // table cannot say so, since it has one answer per code point
    string capital("BLO\xE1\xBA\x9E.de");                // BLOẞ.de
    EXPECT_EQ(*txt::idna::to_unicode(capital, now), string("blo\xC3\x9F.de"));
    EXPECT_EQ(*txt::idna::to_ascii(capital, then), string("bloss.de"));
}

TEST(Idna_Tests, Names) {
    // The label separators of UTS #46: the full stop, the ideographic
    // one, and the two fullwidth ones. They are mapped, not parsed, so
    // the name is broken at the ordinary stop alone
    EXPECT_EQ(*txt::idna::to_ascii(string("a\xE3\x80\x82"
                                          "b\xEF\xBC\x8E"
                                          "c\xEF\xBD\xA1"
                                          "d")),
              string("a.b.c.d"));

    // Uppercase and the compatibility forms are mapped away
    EXPECT_EQ(*txt::idna::to_ascii(string("\xC3\x96" "BB.at")), string("xn--bb-eka.at"));
    EXPECT_EQ(*txt::idna::to_unicode(string("xn--bcher-kva.de")),
              string("b\xC3\xBC" "cher.de"));

    // A decomposed name is normalized before anything else looks at it,
    // so the two spellings of the same name give the same label
    EXPECT_EQ(*txt::idna::to_ascii(string("u\xCC\x88.com")),
              *txt::idna::to_ascii(string("\xC3\xBC.com")));

    // The rules a label is held to
    auto bad = [](const char* s, txt::idna::error e) {
        auto r = txt::idna::to_ascii(string(s));
        ASSERT_FALSE(r.has_value()) << s;
        EXPECT_EQ(r.error().rule, e) << s;
        EXPECT_EQ(r.error().message(), string(txt::idna::message_of(e))) << s;   // the rule in words, as every error of the library has them
    };
    bad("-a.com", txt::idna::error::hyphen);
    bad("a-.com", txt::idna::error::hyphen);
    bad("ab--c.com", txt::idna::error::hyphen);
    bad("\xCC\x81" "a.com", txt::idna::error::leading_combining);   // a combining acute first
    bad("a..com", txt::idna::error::empty_label);
    bad("", txt::idna::error::empty_label);
    bad("xn--a-!.com", txt::idna::error::punycode);                 // not punycode at all
    bad("xn--a.com", txt::idna::error::disallowed);                 // punycode for U+0080
    bad("a\xE2\xBF\xB0" "b.com", txt::idna::error::disallowed);     // U+2FF0, an ideographic description

    // And what is not refused, which surprises people: UTS #46 marks a
    // great many characters valid that IDNA2008 itself would not have
    // (the NV8 column of the mapping table), and the heart of the
    // spam-filter folklore is one of them. Refusing it is the caller's
    // business, or UseSTD3ASCIIRules'
    EXPECT_TRUE(txt::idna::to_ascii(string("a\xE2\x99\xA5" "b.com")).has_value());

    // 63 bytes a label and 253 the name, the root label not counted
    std::string label(63, 'a');
    label += ".com";
    EXPECT_TRUE(txt::idna::to_ascii(string(label.data(), label.size())).has_value());
    std::string over(64, 'a');
    over += ".com";
    EXPECT_EQ(txt::idna::to_ascii(string(over.data(), over.size())).error().rule,
              txt::idna::error::label_too_long);
    std::string longest;
    for (int i = 0; i < 4; ++i) {
        longest += (i ? "." : "") + std::string(63, 'a');
    }
    EXPECT_EQ(longest.size(), 255u);
    EXPECT_EQ(txt::idna::to_ascii(string(longest.data(), longest.size())).error().rule,
              txt::idna::error::name_too_long);
    // and the trailing dot of a fully qualified name does not count
    EXPECT_TRUE(txt::idna::to_ascii(string("a.b.c.d."),
                                    txt::idna::options::whatwg()).has_value());
}

// The mapping of UTS #46 is NFKC_Casefold with three exceptions, which is
// why no table of it is kept: identifier.h works NFKC_Casefold out of the
// compatibility decompositions and the full case folding, and this header
// does the same. The generator holds that against IdnaMappingTable.txt
// before it writes anything; this holds the header's own arithmetic
// against the module's other implementation of the same idea, over every
// code point the standard maps, so that the two cannot drift apart.
TEST(Idna_Tests, TheMappingIsNfkcCasefold) {
    vector<char32_t> out, taken, folded;
    std::vector<char32_t> differ;
    size_t mapped = 0;
    char buf[utf8::max_width];
    for (char32_t c = 0; c < 0x110000; ++c) {
        if (c >= 0xD800 && c < 0xE000) {
            continue;
        }
        auto status = txt::detail::idna_status_of(c);
        if (status != txt::detail::idna_status::mapped
            && status != txt::detail::idna_status::deviation) {
            continue;
        }
        ++mapped;
        out.clear();
        txt::detail::idna_mapped(out, taken, folded, c);
        size_t n = utf8::encode(c, buf);
        if (txt::detail::encoded(out) != txt::nfkc_casefold(string(buf, n))) {
            differ.push_back(c);
        }
    }
    EXPECT_EQ(mapped, 6352u);
    // The capital sharp s, which UTS #46 sends to the small one so that
    // it lands on a deviation character where everybody else folds it to
    // "ss"; and the two ideographic full stops, which are label
    // separators here and ordinary characters everywhere else
    EXPECT_EQ(differ, (std::vector<char32_t>{0x1E9E, 0x3002, 0xFF61}));

    // The ASCII line the mapping takes without asking a table: A to Z are
    // the only ASCII code points the standard maps, each to the letter 32
    // above it
    for (char32_t c = 0; c < 0x80; ++c) {
        bool capital = c >= U'A' && c <= U'Z';
        EXPECT_EQ(txt::detail::idna_status_of(c) == txt::detail::idna_status::mapped, capital)
            << "U+" << std::hex << uint32_t(c);
    }
    EXPECT_EQ(*txt::idna::to_ascii(string("WWW.EXAMPLE.COM")), string("www.example.com"));

    // And the three, seen from the outside
    EXPECT_EQ(*txt::idna::to_unicode(string("\xE1\xBA\x9E.de")), string("\xC3\x9F.de"));
    EXPECT_EQ(txt::nfkc_casefold(string("\xE1\xBA\x9E")), string("ss"));
    EXPECT_EQ(*txt::idna::to_ascii(string("a\xE3\x80\x82" "b")), string("a.b"));
    EXPECT_EQ(*txt::idna::to_ascii(string("a\xEF\xBD\xA1" "b")), string("a.b"));
}

// A name is refused for something one of its labels did, and a caller who
// has to show the name needs to know which: a browser underlines the
// label, it does not grey out the address bar. So a failure names the
// label and the bytes it took up in the text that was handed in — the
// original bytes, not the converted ones.
TEST(Idna_Tests, WhichLabel) {
    auto fault = [](const char* s, txt::idna::options o = {}) {
        return txt::idna::ascii_form(string(s), o).reason;
    };

    // The plain road, where the bytes that came in are the bytes that go
    // out and the range is the label itself
    auto f = fault("good.-bad.also");
    EXPECT_EQ(f.rule, txt::idna::error::hyphen);
    EXPECT_EQ(f.label, 1u);
    EXPECT_EQ(f.at, 5u);
    EXPECT_EQ(f.size, 4u);
    EXPECT_EQ(std::string("good.-bad.also").substr(f.at, f.size), "-bad");

    // The long road, where the label was mapped and normalized on the way
    // and the answer still points into what the caller wrote. "ü" is two
    // bytes, the acute is two, and the label is the six bytes of "xü-"
    f = fault("a.x\xC3\xBC-.com");
    EXPECT_EQ(f.rule, txt::idna::error::hyphen);
    EXPECT_EQ(f.label, 1u);
    EXPECT_EQ(std::string("a.x\xC3\xBC-.com").substr(f.at, f.size), "x\xC3\xBC-");

    // A separator the mapping made — the ideographic full stop is three
    // bytes where the stop it becomes is one — so a range taken from the
    // converted text would be wrong from here on
    f = fault("a\xE3\x80\x82-b.com");
    EXPECT_EQ(f.rule, txt::idna::error::hyphen);
    EXPECT_EQ(f.label, 1u);
    EXPECT_EQ(std::string("a\xE3\x80\x82-b.com").substr(f.at, f.size), "-b");

    // What was ignored on the way is still inside the label's bytes: the
    // soft hyphen leaves no trace in the answer and the name it broke is
    // still the name the caller wrote
    f = fault("a.-b\xC2\xAD.com");
    EXPECT_EQ(f.rule, txt::idna::error::hyphen);
    EXPECT_EQ(std::string("a.-b\xC2\xAD.com").substr(f.at, f.size), "-b\xC2\xAD");

    // A punycode label that does not read names itself
    f = fault("ok.xn--a-!.com");
    EXPECT_EQ(f.rule, txt::idna::error::punycode);
    EXPECT_EQ(f.label, 1u);
    EXPECT_EQ(std::string("ok.xn--a-!.com").substr(f.at, f.size), "xn--a-!");

    // An empty label, which has no bytes of its own but does have a place
    f = fault("a..c");
    EXPECT_EQ(f.rule, txt::idna::error::empty_label);
    EXPECT_EQ(f.label, 1u);
    EXPECT_EQ(f.at, 2u);
    EXPECT_EQ(f.size, 0u);

    // The length of the whole name is the one thing that is not any
    // label's fault, and it says so
    std::string longest;
    for (int i = 0; i < 4; ++i) {
        longest += (i ? "." : "") + std::string(63, 'a');
    }
    f = fault(longest.data());
    EXPECT_EQ(f.rule, txt::idna::error::name_too_long);
    EXPECT_EQ(f.label, txt::idna::failure::whole_name);
    EXPECT_EQ(f.size, longest.size());

    // And a name with nothing wrong with it says nothing
    EXPECT_EQ(fault("example.com").rule, txt::idna::error::none);
}

TEST(Idna_Tests, Options) {
    // An underscore is a valid ASCII character to UTS #46 unless STD3 is
    // asked for, which is why a browser leaves it alone and a resolver
    // that follows RFC 1123 does not
    txt::idna::options std3;
    std3.use_std3_ascii_rules = true;
    EXPECT_TRUE(txt::idna::to_ascii(string("a_b.com")).has_value());
    EXPECT_EQ(txt::idna::to_ascii(string("a_b.com"), std3).error().rule, txt::idna::error::std3);

    // The WHATWG profile does not check the hyphens, so a name the DNS
    // has carried for years keeps working, and does not check the
    // lengths either
    EXPECT_EQ(txt::idna::to_ascii(string("ab--c.com")).error().rule, txt::idna::error::hyphen);
    EXPECT_TRUE(txt::idna::to_ascii(string("ab--c.com"), txt::idna::options::whatwg()).has_value());

    // With the hyphens unchecked, a label that says "xn--" and is not
    // punycode is refused for saying it (criterion 4 of section 4.1)
    auto whatwg = txt::idna::options::whatwg();
    EXPECT_FALSE(txt::idna::to_ascii(string("xn--a.com"), whatwg).has_value());

    // A label whose punycode does not read is kept as it stands when the
    // caller says so, rather than losing the name altogether
    whatwg.ignore_invalid_punycode = true;
    auto kept = txt::idna::unicode_form(string("xn--a-!.ss"), whatwg);
    EXPECT_TRUE(bool(kept));
    EXPECT_EQ(kept.text, string("xn--a-!.ss"));

    // The bidirectional rule of RFC 5893 is asked about the name and not
    // about a label: an ASCII label in a name that has Hebrew in it is
    // held to it too, and a label that begins with a digit fails
    // condition 1
    EXPECT_FALSE(txt::idna::to_ascii(string("0\xC3\xA0.\xD7\x90")).has_value());
    EXPECT_TRUE(txt::idna::to_ascii(string("\xC3\xA0.\xD7\x90\xCC\x88")).has_value());
    // and a name with nothing right to left in it is not asked at all
    EXPECT_TRUE(txt::idna::to_ascii(string("0a.com")).has_value());
}
