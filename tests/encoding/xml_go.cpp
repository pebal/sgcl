//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Go's encoding/xml as the oracle of the tokens (tools/xml_oracle.go writes
// xml_go_tests.h): the documents it names, whole, and every test of the W3C
// suite the conformance test takes, by a hash of the lines of its tokens.
// Compared only where both read a document to its end: whether a document
// is well formed is the W3C suite's question (xml_conformance.cpp), and Go
// reads much that XML refuses.
#include "xml_common.h"
#include "xml_go_tests.h"

#include <fstream>
#include <map>

using namespace sgcl::encoding;
using namespace xml_test;

TEST(XmlGo_Tests, NamedDocuments) {
    for (auto& g : GoNamed) {
        auto d = dump(g.doc);
        EXPECT_TRUE(g.ok) << shown(g.doc);
        EXPECT_TRUE(d.ok) << shown(g.doc) << ": " << (d.error ? std::string(d.error->message().view()) : "");
        EXPECT_EQ(d.tokens, g.tokens) << shown(g.doc);
    }
}

TEST(XmlGo_Tests, W3CSuite) {
    auto root = xmlconf_root();
    if (root.empty()) {
        GTEST_SKIP() << "the W3C suite is not in ~/Programming/oracles/xmlconf";
    }
    std::map<std::string, std::string> files;
    for (auto& t : tests_of(root)) {
        files[t.id] = t.file;
    }
    size_t compared = 0, go_only = 0, mine_only = 0, neither = 0;
    for (auto& g : GoSuite) {
        if (!g.id) {
            break;
        }
        auto f = files.find(g.id);
        ASSERT_TRUE(f != files.end()) << g.id << ": the oracle took a test the C++ side did not";
        auto d = dump(read_file(f->second));
        if (d.ok && g.ok) {
            EXPECT_EQ(enc_test::fnv(d.tokens), g.tokens) << g.id << " " << f->second.substr(root.size()) << "\n" << d.tokens;
            ++compared;
        } else if (g.ok) {
            ++go_only;
        } else if (d.ok) {
            if (std::getenv("SGCL_XML_GO_LIST")) {
                std::cout << "[ go       ] this reader alone: " << g.id << " " << f->second.substr(root.size()) << "\n";
            }
            ++mine_only;
        } else {
            ++neither;
        }
        files.erase(f);
    }
    EXPECT_TRUE(files.empty()) << files.size() << " tests the oracle did not take";
    std::cout << "[ go       ] " << compared << " documents both read, tokens compared; Go alone read " << go_only
              << ", this reader alone " << mine_only << ", neither " << neither << "\n";
}

// Documents made by mutating the named ones and the suite's
// (tools/xml_oracle.go -fuzz N), when SGCL_XML_FUZZ names the file: where
// both read one to its end, the same tokens; and on every one, whole or
// through a stream in pieces, the same outcome, and a tree that is written
// and read again as itself. What Go alone reads, and what this reader
// alone reads, is counted and shown, for the eye: the W3C suite decides.
TEST(XmlGo_Tests, Fuzz) {
    const char* path = std::getenv("SGCL_XML_FUZZ");
    if (!path) {
        GTEST_SKIP() << "SGCL_XML_FUZZ names no file of tools/xml_oracle.go -fuzz";
    }
    std::ifstream in(path);
    std::string row;
    size_t both = 0, go_only = 0, mine_only = 0, neither = 0, shown_go = 0, shown_mine = 0, instruction_in_subset = 0;
    std::map<std::string, size_t> go_only_why;
    while (std::getline(in, row)) {
        // "<hex> <true|false> <hash>"; the hex of an empty document is empty
        auto a = row.find(' '), b = row.find(' ', a + 1);
        std::string hex = row.substr(0, a), ok = row.substr(a + 1, b - a - 1);
        uint64_t hash = std::stoull(row.substr(b + 1));
        std::string doc;
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            doc += char(std::stoi(hex.substr(i, 2), nullptr, 16));
        }
        auto d = dump(doc);
        auto cut = dump_stream(doc, 3);
        ASSERT_EQ(cut.tokens, d.tokens) << shown(doc);
        ASSERT_EQ(cut.ok, d.ok) << shown(doc);
        if (d.error && cut.error) {
            ASSERT_EQ(*cut.error, *d.error) << shown(doc);
        }
        bool go = ok == "true";
        if (d.ok) {
            auto t = xml::parse(string(doc));
            ASSERT_TRUE(t.has_value()) << shown(doc);
            auto again = xml::parse(t->to_string());
            ASSERT_TRUE(again.has_value()) << shown(doc) << " written " << t->to_string().view();
            ASSERT_EQ(*again, *t) << shown(doc);
        }
        // Go ends a DOCTYPE at a '>' inside an instruction of its internal
        // subset (xml_oracle.go): such a document is compared by no one
        auto doctype = doc.find("<!DOCTYPE");
        auto subset = doctype == std::string::npos ? doctype : doc.find('[', doctype);
        bool go_wrong = subset != std::string::npos && doc.find("<?", subset) < doc.rfind("]>");
        if (d.ok && go && go_wrong) {
            ++instruction_in_subset;
        } else if (d.ok && go) {
            ++both;
            EXPECT_EQ(enc_test::fnv(d.tokens), hash) << shown(doc) << "\n" << d.tokens;
        } else if (go) {
            ++go_only;
            ++go_only_why[std::string(sgcl::encoding::encoding_category().message(int(d.error->code())))];
            auto show = std::getenv("SGCL_XML_FUZZ_SHOW");
            if (show && (std::string(show) == "all" || shown_go++ < 5)) {
                std::cout << "[ fuzz     ] Go alone: " << shown(doc) << ": " << d.error->message().view() << "\n";
            }
        } else if (d.ok) {
            ++mine_only;
            auto show = std::getenv("SGCL_XML_FUZZ_SHOW");
            if (show && (std::string(show) == "all" || shown_mine++ < 20)) {
                std::cout << "[ fuzz     ] this reader alone: " << shown(doc) << "\n";
            }
        } else {
            ++neither;
        }
    }
    std::cout << "[ fuzz     ] " << both + go_only + mine_only + neither + instruction_in_subset << " documents: both read " << both
              << " (tokens compared) and " << instruction_in_subset << " with an instruction in the DTD (not compared), Go alone "
              << go_only << ", this reader alone " << mine_only << ", neither " << neither << "\n";
    for (auto& [why, n] : go_only_why) {
        std::cout << "[ fuzz     ]   Go alone, refused here as " << why << ": " << n << "\n";
    }
}
