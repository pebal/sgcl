//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// pem: the textual encodings of RFC 7468 with the headers of RFC 1421.
// Blocks written as Go's EncodeToMemory writes them, texts read as its
// Decode reads them, the labels of sections 5 to 13, the lax grammar a
// parser is to accept, and the errors — with their lines and columns — of
// a block that is there and malformed.
#include "common.h"
#include "encoding_tests.h"

using namespace sgcl::encoding;

#include <string>

using namespace enc_test;

namespace {
    ordered_map<string, string> headers_of(std::string_view lines) {
        ordered_map<string, string> out;
        while (!lines.empty()) {
            auto nl = lines.find('\n');
            auto line = lines.substr(0, nl);
            auto colon = line.find(": ");
            out.insert_or_assign(string(line.substr(0, colon)), string(line.substr(colon + 2)));
            lines.remove_prefix(nl + 1);
        }
        return out;
    }

    std::string sorted_headers(const pem& p) {
        std::vector<std::string> lines;
        for (auto& [k, v] : p.headers()) {
            lines.push_back(std::string(k.view()) + ": " + std::string(v.view()) + "\n");
        }
        std::sort(lines.begin(), lines.end());
        std::string out;
        for (auto& l : lines) {
            out += l;
        }
        return out;
    }

    sgcl::vector<byte> managed(const std::vector<byte>& v) {
        return sgcl::vector<byte>(v.begin(), v.end());
    }
}

TEST(Pem_Tests, WritesAsGoWrites) {
    for (auto& c : oracle::PemEncodes) {
        pem block(c.type, managed(input(c.bytes)), headers_of(c.headers));
        EXPECT_EQ(block.to_string(), c.text) << c.type;
        auto back = pem::parse(block.to_string());
        ASSERT_TRUE(back.has_value()) << c.type << ": " << back.error().message();
        EXPECT_EQ(back->type(), c.type);
        EXPECT_EQ(bytes_of(back->bytes()), input(c.bytes));
        EXPECT_EQ(back->headers().size(), block.headers().size());
        EXPECT_EQ(sorted_headers(*back), sorted_headers(block));
    }
}

TEST(Pem_Tests, ReadsWhatGoReads) {
    for (auto& c : oracle::PemDecodes) {
        auto r = pem::parse(c.text);
        ASSERT_TRUE(r.has_value()) << c.text << ": " << r.error().message();
        EXPECT_EQ(r->type(), c.type) << c.text;
        EXPECT_EQ(sorted_headers(*r), c.headers) << c.text;
        EXPECT_EQ(bytes_of(r->bytes()), from_hex(c.hex)) << c.text;
        // parse_all takes the first block and the ones after the rest
        auto all = pem::parse_all(c.text);
        ASSERT_TRUE(all.has_value()) << c.text;
        ASSERT_GE(all->size(), 1u);
        EXPECT_EQ((*all)[0].type(), c.type);
        auto rest = pem::parse_all(string(std::string_view(c.text).substr(c.rest)));
        ASSERT_TRUE(rest.has_value());
        EXPECT_EQ(rest->size() + 1, all->size()) << c.text;
    }
}

// The examples of RFC 7468 itself (figures 6 to 19, the non-conforming
// labels of appendix A among them), as the RFC prints them: the same type
// and bytes as Go's pem.Decode, bytes that are one DER value of their own
// length, and a block that writes back as the RFC wrote it
TEST(Pem_Tests, ExamplesOfRfc7468) {
    ASSERT_EQ(std::size(oracle::RfcPems), 14u) << "~/Programming/oracles/rfc7468/rfc7468.txt when the header was generated";
    for (auto& c : oracle::RfcPems) {
        auto r = pem::parse(c.text);
        ASSERT_TRUE(r.has_value()) << c.figure << ": " << r.error().message();
        EXPECT_EQ(r->type(), c.type) << c.figure;
        auto bytes = bytes_of(r->bytes());
        EXPECT_EQ(bytes, from_hex(c.hex)) << c.figure;
        // a DER SEQUENCE whose length is the rest of the bytes
        ASSERT_GE(bytes.size(), 2u) << c.figure;
        EXPECT_EQ(bytes[0], byte(0x30)) << c.figure;
        size_t length = size_t(bytes[1]);
        size_t head = 2;
        if (length & 0x80) {
            size_t n = length & 0x7F;
            length = 0;
            for (size_t i = 0; i < n; ++i) {
                length = length << 8 | size_t(bytes[2 + i]);
            }
            head += n;
        }
        EXPECT_EQ(head + length, bytes.size()) << c.figure;
        auto text = std::string_view(c.text);
        EXPECT_EQ(r->to_string(), text.substr(text.find("-----BEGIN "))) << c.figure;
        EXPECT_EQ(pem::parse_all(c.text).value().size(), 1u) << c.figure;
    }
}

// The labels of RFC 7468 sections 5 to 13, with the explanatory text
// section 5.2 allows before and between, a chain read at once
TEST(Pem_Tests, LabelsOfRfc7468) {
    const char* labels[] = {"CERTIFICATE", "X509 CRL", "CERTIFICATE REQUEST", "PKCS7", "CMS", "PRIVATE KEY",
                            "ENCRYPTED PRIVATE KEY", "ATTRIBUTE CERTIFICATE", "PUBLIC KEY"};
    std::string chain = "Subject: CN=Atlantis\nIssuer: CN=Atlantis\n";
    for (size_t i = 0; i < std::size(labels); ++i) {
        pem block(labels[i], managed(input(30 + 17 * i)));
        chain += std::string(block.to_string().view()) + "text between the blocks\n";
    }
    auto all = pem::parse_all(string(chain));
    ASSERT_TRUE(all.has_value()) << all.error().message();
    ASSERT_EQ(all->size(), std::size(labels));
    for (size_t i = 0; i < std::size(labels); ++i) {
        EXPECT_EQ((*all)[i].type(), labels[i]);
        EXPECT_EQ(bytes_of((*all)[i].bytes()), input(30 + 17 * i));
        EXPECT_TRUE((*all)[i].headers().empty());
    }
    // no block at all: parse_all has none, parse fails at the end
    EXPECT_TRUE(pem::parse_all("just text\n").value().empty());
    auto none = pem::parse("just text\n");
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(none.error().code(), encoding::errc::unexpected_end);
}

// Section 3's lax grammar: white space anywhere in the base64, lines of
// any length, CRLF, white space after the dashes, an empty body
TEST(Pem_Tests, LaxGrammar) {
    auto in = input(100);
    auto b64 = std::string(base64::standard.encode(as_slice(in)).view());
    std::string lax = "-----BEGIN PRIVATE KEY-----  \r\n";
    for (size_t i = 0; i < b64.size(); i += 13) {
        lax += " " + b64.substr(i, 13) + "\t\r\n";
    }
    lax += "-----END PRIVATE KEY-----\t";
    auto r = pem::parse(string(lax));
    ASSERT_TRUE(r.has_value()) << r.error().message();
    EXPECT_EQ(bytes_of(r->bytes()), in);
    EXPECT_TRUE(pem::parse("-----BEGIN X-----\n-----END X-----\n").value().bytes().empty());
    // an empty label is a label
    EXPECT_EQ(pem::parse("-----BEGIN -----\nQQ==\n-----END -----\n").value().type(), "");
}

// The headers of RFC 1421: in their order, a folded value, Proc-Type
// written first whatever the order it was given in
TEST(Pem_Tests, Headers) {
    auto r = pem::parse(
        "-----BEGIN RSA PRIVATE KEY-----\n"
        "Proc-Type: 4,ENCRYPTED\n"
        "DEK-Info: DES-EDE3-CBC,\n"
        " 0123456789ABCDEF\n"
        "Comment:   spaced   \n"
        "\n"
        "QUJD\n"
        "-----END RSA PRIVATE KEY-----\n");
    ASSERT_TRUE(r.has_value()) << r.error().message();
    auto& h = r->headers();
    ASSERT_EQ(h.size(), 3u);
    EXPECT_EQ(h.at(string("Proc-Type")), "4,ENCRYPTED");
    EXPECT_EQ(h.at(string("DEK-Info")), "DES-EDE3-CBC, 0123456789ABCDEF");
    EXPECT_EQ(h.at(string("Comment")), "spaced");
    EXPECT_EQ(h.begin()->first, "Proc-Type");
    EXPECT_EQ(std::string(view_of(bytes_of(r->bytes()))), "ABC");

    // with no empty line after the headers there is no folding: an
    // indented line is the base64, as Go reads it, and never a value
    auto unfolded = pem::parse("-----BEGIN A-----\nA: 1\n QUJD\n-----END A-----\n");
    ASSERT_TRUE(unfolded.has_value()) << unfolded.error().message();
    EXPECT_EQ(unfolded->headers().at(string("A")), "1");
    EXPECT_EQ(std::string(view_of(bytes_of(unfolded->bytes()))), "ABC");

    ordered_map<string, string> given;
    given.insert_or_assign(string("DEK-Info"), string("AES-128-CBC,00"));
    given.insert_or_assign(string("Proc-Type"), string("4,ENCRYPTED"));
    pem block("RSA PRIVATE KEY", managed(bytes_of("ABC")), given);
    EXPECT_EQ(block.to_string(),
              "-----BEGIN RSA PRIVATE KEY-----\n"
              "Proc-Type: 4,ENCRYPTED\n"
              "DEK-Info: AES-128-CBC,00\n"
              "\n"
              "QUJD\n"
              "-----END RSA PRIVATE KEY-----\n");
}

// A block that is there and malformed is an error with its place, not a
// block passed over
TEST(Pem_Tests, Errors) {
    struct Case {
        const char* text;
        encoding::errc code;
        uint32_t line;
        uint32_t column;
    };
    const Case cases[] = {
        {"-----BEGIN A-----\nQUJD\n", encoding::errc::unexpected_end, 3, 1},                         // no END
        {"x\n-----BEGIN A-----\nQUJD\n-----END B-----\n", encoding::errc::syntax, 4, 10},            // END of another type
        {"-----BEGIN A-----\nQUJD\nQU*D\n-----END A-----\n", encoding::errc::invalid_character, 3, 3},
        {"-----BEGIN A-----\nQUJD\nQUI\n-----END A-----\n", encoding::errc::unexpected_end, 4, 1},   // the base64 cut short
        {"-----BEGIN A-----\nQR==\n-----END A-----\n", encoding::errc::syntax, 2, 2},                 // bits past the data
        {"-----BEGIN A----\nQUJD\n-----END A-----\n", encoding::errc::syntax, 1, 17},                 // four dashes
        {"-----BEGIN A--B-----\nQUJD\n-----END A--B-----\n", encoding::errc::syntax, 1, 12},          // not a label
        {"-----BEGIN A----- x\nQUJD\n-----END A-----\n", encoding::errc::syntax, 1, 19},              // text after the dashes
        {"-----BEGIN A-----\nQUJD\n-----END A----\n", encoding::errc::syntax, 3, 15},
        {"-----BEGIN A-----\n: no name\n\nQUJD\n-----END A-----\n", encoding::errc::syntax, 2, 1},
        {"-----BEGIN A-----\nQUJD\n-----END A-----\n-----BEGIN B-----\nQQ\n-----END B-----\n", encoding::errc::unexpected_end, 6, 1},
        {"-----BEGIN Ą-----\nQUJD\n-----END Ą-----\n", encoding::errc::syntax, 1, 12},
    };
    for (auto& c : cases) {
        auto r = pem::parse_all(c.text);
        ASSERT_FALSE(r.has_value()) << c.text;
        auto& e = r.error();
        EXPECT_EQ(e.code(), c.code) << c.text << ": " << e.message();
        EXPECT_EQ(e.line(), c.line) << c.text << ": " << e.message();
        EXPECT_EQ(e.column(), c.column) << c.text << ": " << e.message();
    }
    EXPECT_EQ(pem::parse("x\n-----BEGIN A-----\nQUJD\n-----END B-----\n").error().message(), "4:10: END B does not match BEGIN A");
    EXPECT_EQ(pem::parse("-----BEGIN A-----\nQU*D\n-----END A-----\n").error().message(), "2:3: invalid character '*'");
    // a BEGIN that does not start a line is text
    EXPECT_TRUE(pem::parse_all("x -----BEGIN A-----\n").value().empty());
}

// What could not be written and read back is refused when the block is made
TEST(Pem_Tests, InvalidBlocks) {
    EXPECT_THROW(pem("A--B", {}), std::invalid_argument);
    EXPECT_THROW(pem("A\nB", {}), std::invalid_argument);
    EXPECT_THROW(pem(" A", {}), std::invalid_argument);
    ordered_map<string, string> colon;
    colon.insert_or_assign(string("A:B"), string("1"));
    EXPECT_THROW(pem("A", {}, colon), std::invalid_argument);
    ordered_map<string, string> line;
    line.insert_or_assign(string("A"), string("1\n2"));
    EXPECT_THROW(pem("A", {}, line), std::invalid_argument);
    ordered_map<string, string> empty;
    empty.insert_or_assign(string(""), string("1"));
    EXPECT_THROW(pem("A", {}, empty), std::invalid_argument);
    for (const char* value : {" 1", "1 ", "\t1", "1\t", "1\x01", "\x7F", "a\rb"}) {
        ordered_map<string, string> h;
        h.insert_or_assign(string("A"), string(value));
        EXPECT_THROW(pem("A", {}, h), std::invalid_argument) << value;
    }
    ordered_map<string, string> boundary;
    boundary.insert_or_assign(string("-----END"), string("1"));
    EXPECT_THROW(pem("A", {}, boundary), std::invalid_argument);
    EXPECT_NO_THROW(pem("X509 CRL", {}));
    // what is accepted writes and reads back as it was: a tab inside a
    // value, an empty value, a colon in a value, a name of any printable
    // characters
    ordered_map<string, string> kept;
    kept.insert_or_assign(string("A"), string("x\ty"));
    kept.insert_or_assign(string("B"), string(""));
    kept.insert_or_assign(string("C"), string("k: v"));
    kept.insert_or_assign(string("D-e.f_!"), string("1"));
    pem block("A", managed(bytes_of("ABC")), kept);
    auto back = pem::parse(block.to_string());
    ASSERT_TRUE(back.has_value()) << back.error().message();
    EXPECT_EQ(back->headers().size(), 4u);
    for (auto& [k, v] : kept) {
        EXPECT_EQ(back->headers().at(k), v) << k.view();
    }
}
