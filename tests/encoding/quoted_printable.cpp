//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// quoted_printable: RFC 2045 §6.7 both ways — the rules of the encoding (the
// literal range, the white space before a line break, the soft breaks at 76),
// the text and binary forms, strict and lenient decoding, the streams fed in
// pieces of every size, and Go's mime/quotedprintable as the oracle.
#include "common.h"
#include "tests/source_root.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;

namespace {
    std::string s(const sgcl::string& t) {
        return std::string(t.view());
    }

    std::string s(const sgcl::vector<byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    std::string decode(std::string_view text, bool lenient = false) {
        auto q = lenient ? quoted_printable::standard.lenient() : quoted_printable::standard;
        auto r = q.decode(sgcl::string(text));
        return r ? s(*r) : "ERROR " + s(r.error().message());
    }

    // every line at most 76 characters, CRLF only, the alphabet of the encoding
    void well_formed(const std::string& t) {
        size_t line = 0;
        for (size_t i = 0; i < t.size(); ++i) {
            char c = t[i];
            if (c == '\r') {
                ASSERT_TRUE(i + 1 < t.size() && t[i + 1] == '\n') << t;
                ASSERT_LE(line, 76u) << t;
                line = 0;
                ++i;
                continue;
            }
            ASSERT_NE(c, '\n') << t;
            ASSERT_TRUE((c >= 33 && c <= 126) || c == ' ' || c == '\t') << int(c);
            ++line;
        }
        ASSERT_LE(line, 76u);
    }
}

TEST(QuotedPrintable_Tests, EncodesTheLiteralRangeAndEscapesTheRest) {
    EXPECT_EQ(s(quoted_printable::standard.encode("")), "");
    EXPECT_EQ(s(quoted_printable::standard.encode("plain text")), "plain text");
    EXPECT_EQ(s(quoted_printable::standard.encode("a=b")), "a=3Db");
    EXPECT_EQ(s(quoted_printable::standard.encode("Grüße")), "Gr=C3=BC=C3=9Fe");
    EXPECT_EQ(s(quoted_printable::standard.encode(sgcl::string(std::string("\x00\x7f\x80\xff", 4)))), "=00=7F=80=FF");
}

TEST(QuotedPrintable_Tests, WhiteSpaceBeforeABreakOrTheEndIsEscaped) {
    EXPECT_EQ(s(quoted_printable::standard.encode("a ")), "a=20");
    EXPECT_EQ(s(quoted_printable::standard.encode("a\t")), "a=09");
    EXPECT_EQ(s(quoted_printable::standard.encode("a \nb")), "a=20\r\nb");
    EXPECT_EQ(s(quoted_printable::standard.encode("a  b")), "a  b");
    EXPECT_EQ(s(quoted_printable::standard.encode("a \r\nb")), "a=20\r\nb");
}

TEST(QuotedPrintable_Tests, TextAndBinaryLineBreaks) {
    EXPECT_EQ(s(quoted_printable::standard.encode("one\ntwo\r\nthree\rfour")), "one\r\ntwo\r\nthree=0Dfour");
    EXPECT_EQ(s(quoted_printable::standard.binary().encode("one\ntwo\r\n")), "one=0Atwo=0D=0A");
    EXPECT_TRUE(quoted_printable::standard.binary().is_binary());
    EXPECT_FALSE(quoted_printable::standard.is_binary());
    EXPECT_EQ(s(quoted_printable::standard.encode("\r")), "=0D");
}

TEST(QuotedPrintable_Tests, SoftBreaksKeepLinesAt76) {
    std::string in(200, 'x');
    auto out = s(quoted_printable::standard.encode(sgcl::string(in)));
    well_formed(out);
    EXPECT_EQ(decode(out), in);
    std::string escapes;
    for (int i = 0; i < 100; ++i) {
        escapes += "ż ";
    }
    out = s(quoted_printable::standard.encode(sgcl::string(escapes)));
    well_formed(out);
    EXPECT_EQ(decode(out), escapes);
    // an escape is never split across a soft break
    for (size_t at = out.find('='); at != std::string::npos; at = out.find('=', at + 1)) {
        ASSERT_TRUE(at + 2 < out.size());
        ASSERT_TRUE(out[at + 1] == '\r' || (isxdigit(uint8_t(out[at + 1])) && isxdigit(uint8_t(out[at + 2])))) << out.substr(at, 5);
    }
}

TEST(QuotedPrintable_Tests, DecodesBothCasesSoftBreaksAndTransportSpace) {
    EXPECT_EQ(decode("Gr=c3=bc=C3=9Fe"), "Grüße");
    EXPECT_EQ(decode("soft=\r\nbreak"), "softbreak");
    EXPECT_EQ(decode("soft=\nbreak"), "softbreak");
    EXPECT_EQ(decode("soft= \t\r\nbreak"), "softbreak");
    EXPECT_EQ(decode("trailing   \r\nnext"), "trailing\r\nnext");
    EXPECT_EQ(decode("lf only\nnext"), "lf only\nnext");
    EXPECT_EQ(decode("end ="), "end ");
    EXPECT_EQ(decode("end   "), "end");
    EXPECT_EQ(decode(""), "");
}

TEST(QuotedPrintable_Tests, StrictRefusesBadEscapesLenientKeepsThem) {
    EXPECT_EQ(decode("a=ZZb"), "ERROR offset 1: invalid quoted-printable escape");
    EXPECT_EQ(decode("a=4"), "ERROR offset 3: quoted-printable escape cut short");
    EXPECT_EQ(decode("a= b"), "ERROR offset 1: invalid quoted-printable escape");
    EXPECT_EQ(decode("a=ZZb", true), "a=ZZb");
    EXPECT_EQ(decode("a=4", true), "a=4");
    EXPECT_EQ(decode("a=4g", true), "a=4g");
    EXPECT_EQ(decode("a= b", true), "a= b");
    EXPECT_EQ(decode("100% = sure", true), "100% = sure");
    auto r = quoted_printable::standard.decode("x=Q");
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::invalid_escape);
    EXPECT_EQ(r.error().offset(), 1u);
}

TEST(QuotedPrintable_Tests, RoundTripsEveryByte) {
    std::string all;
    for (int i = 0; i < 256; ++i) {
        all += char(i);
    }
    for (int k = 0; k < 4; ++k) {
        all += all;
    }
    auto bin = s(quoted_printable::standard.binary().encode(sgcl::string(all)));
    well_formed(bin);
    EXPECT_EQ(decode(bin), all);
    auto in = input(5000);
    sgcl::vector<byte> v(in.begin(), in.end());
    auto enc = s(quoted_printable::standard.binary().encode(v));
    well_formed(enc);
    EXPECT_EQ(decode(enc), std::string(reinterpret_cast<const char*>(in.data()), in.size()));
}

TEST(QuotedPrintable_Tests, StreamsInPiecesOfEverySize) {
    std::string text = "Zażółć gęślą jaźń = 100%, with trailing space \nand a long line " + std::string(150, 'y') + " end \r\nlast";
    auto whole = s(quoted_printable::standard.encode(sgcl::string(text)));
    for (size_t piece : {size_t(1), size_t(2), size_t(3), size_t(7), size_t(64), size_t(10000)}) {
        sgcl::tracked_ptr sink = sgcl::make_tracked<io::buffer>();
        auto enc = quoted_printable::standard.encoder_to(io::writer(sink));
        for (size_t i = 0; i < text.size(); i += piece) {
            ASSERT_TRUE(enc.write(sgcl::string(std::string_view(text).substr(i, piece))));
        }
        ASSERT_TRUE(enc.close());
        EXPECT_TRUE(enc.is_closed());
        EXPECT_FALSE(enc.write(sgcl::string("x")));
        auto got = io::read_all(io::reader(sink));
        ASSERT_TRUE(got);
        EXPECT_EQ(s(*got), whole) << piece;

        sgcl::tracked_ptr src = sgcl::make_tracked<io::buffer>(sgcl::string(whole));
        auto dec = quoted_printable::standard.decoder_from(io::reader(src));
        std::string back;
        sgcl::vector<byte> buf(piece);
        for (;;) {
            auto n = dec.read(buf.as_slice());
            ASSERT_TRUE(n);
            if (*n == 0) {
                break;
            }
            back.append(reinterpret_cast<const char*>(buf.data()), *n);
        }
        EXPECT_EQ(back, decode(whole)) << piece;
    }
}

TEST(QuotedPrintable_Tests, DecoderStreamReportsTheError) {
    sgcl::tracked_ptr src = sgcl::make_tracked<io::buffer>(sgcl::string("ok=ZZ"));
    auto dec = quoted_printable::standard.decoder_from(io::reader(src));
    auto all = io::read_all(dec);
    ASSERT_FALSE(all);
    ASSERT_TRUE(dec.last_error());
    EXPECT_EQ(dec.last_error()->code(), errc::invalid_escape);
    sgcl::tracked_ptr src2 = sgcl::make_tracked<io::buffer>(sgcl::string("ok=ZZ"));
    auto lax = quoted_printable::standard.lenient().decoder_from(io::reader(src2));
    auto got = io::read_all(lax);
    ASSERT_TRUE(got);
    EXPECT_EQ(s(*got), "ok=ZZ");
}

TEST(QuotedPrintable_Tests, DefaultHandlesHoldNothing) {
    quoted_printable::encoder e;
    quoted_printable::decoder d;
    EXPECT_FALSE(e);
    EXPECT_FALSE(d);
    EXPECT_FALSE(quoted_printable::standard.is_lenient());
    EXPECT_TRUE(quoted_printable::standard.lenient().is_lenient());
    EXPECT_TRUE(quoted_printable::standard.lenient().binary().is_lenient());
}

// Go's mime/quotedprintable reads what we write, and we read what it writes
TEST(QuotedPrintableInterop_Tests, Go) {
    if (std::system("command -v go > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no go";
    }
    auto dir = std::filesystem::temp_directory_path() / "sgcl_qp_go";
    std::filesystem::create_directories(dir);
    auto bin = dir / "go_mail";
    auto cmd = "go build -o '" + bin.string() + "' '" + (source_root() / "tests/encoding/go_mail/main.go").string() + "' 2>&1";
    ASSERT_EQ(std::system(cmd.c_str()), 0);
    std::string text = "Zażółć gęślą jaźń = 100%,\r\ntrailing \r\n" + std::string(300, 'q') + "\r\n\tTab line\t\r\n";
    for (int i = 0; i < 200; ++i) {
        text += char('a' + i % 26);
        if (i % 37 == 0) {
            text += "é ";
        }
    }
    auto ours = s(quoted_printable::standard.encode(sgcl::string(text)));
    {
        std::ofstream(dir / "ours.qp", std::ios::binary) << ours;
        std::ofstream(dir / "plain.txt", std::ios::binary) << text;
    }
    auto run = [](const std::string& c) {
        std::string out;
        FILE* p = popen(c.c_str(), "r");
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    };
    EXPECT_EQ(run("'" + bin.string() + "' qp-decode < '" + (dir / "ours.qp").string() + "'"), text);
    auto theirs = run("'" + bin.string() + "' qp-encode < '" + (dir / "plain.txt").string() + "'");
    EXPECT_EQ(decode(theirs), text);
}
