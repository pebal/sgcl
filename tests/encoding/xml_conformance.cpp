//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The W3C XML Conformance Test Suite (xmlconf, 2013-09-23) as the oracle of
// what is well formed. The suite is not in the repository: the test reads
// it from ~/Programming/oracles/xmlconf and is skipped when it is not
// there. Its catalogs are read with this module's own reader.
//
// Which tests are taken: those of XML 1.0 in its fifth edition and of
// Namespaces in XML 1.0 — not the tests of XML 1.1 or Namespaces 1.1, not
// those the catalog marks for editions 1 to 4 alone, not those marked as
// not namespace-well-formed on purpose (NAMESPACE="no": this reader always
// reads namespaces), and not TYPE="error" (an error a processor may
// report or not). Of the rest a not-wf document must be refused and a
// valid or invalid one (valid or not, it is well formed) must be read.
//
// Where this reader decides otherwise it does so by design — no DTD is
// read — and every such test is named in xml_conformance_expected.h with
// the reason; the test fails on any outcome that is neither the suite's
// nor listed there, and on a listed one that no longer happens.
//
// Valid documents that come with a canonical form (OUTPUT, James Clark's
// canonical XML) are also written in that form from the tree and compared
// byte for byte: text, attribute values and their normalization,
// references, line endings, instructions.
#include "xml_common.h"
#include "xml_conformance_expected.h"

#include <algorithm>
#include <map>
#include <set>

using namespace sgcl::encoding;
using namespace xml_test;

namespace {
    // James Clark's canonical XML: the root and what is inside it, with
    // attributes sorted by name, &amp; &lt; &gt; &quot; and &#9; &#10;
    // &#13; for text and values, instructions kept, comments and the
    // declaration and DOCTYPE left out
    void canonical_text(std::string& out, std::string_view t) {
        for (char c : t) {
            switch (c) {
                case '&': out += "&amp;"; break;
                case '<': out += "&lt;"; break;
                case '>': out += "&gt;"; break;
                case '"': out += "&quot;"; break;
                case '\t': out += "&#9;"; break;
                case '\n': out += "&#10;"; break;
                case '\r': out += "&#13;"; break;
                default: out += c;
            }
        }
    }

    void canonical(std::string& out, const xml& n) {
        switch (n.type()) {
            case xml::kind::text:
                canonical_text(out, n.text().view());
                break;
            case xml::kind::instruction:
                out += "<?" + std::string(n.name().view()) + " " + std::string(n.text().view()) + "?>";
                break;
            case xml::kind::element: {
                out += "<" + std::string(n.name().view());
                std::vector<std::pair<std::string, std::string>> attributes;
                for (auto& a : n.attributes()) {
                    attributes.emplace_back(std::string(a.name.view()), std::string(a.value.view()));
                }
                std::sort(attributes.begin(), attributes.end());
                for (auto& [name, value] : attributes) {
                    out += " " + name + "=\"";
                    canonical_text(out, value);
                    out += "\"";
                }
                out += ">";
                for (auto& c : n.children()) {
                    canonical(out, c);
                }
                out += "</" + std::string(n.name().view()) + ">";
                break;
            }
            default:
                break;
        }
    }
}

TEST(XmlConformance_Tests, W3CSuite) {
    auto root = xmlconf_root();
    if (root.empty()) {
        GTEST_SKIP() << "the W3C suite is not in ~/Programming/oracles/xmlconf";
    }
    auto tests = tests_of(root);
    std::map<std::string, std::pair<std::string, std::string>> expected;   // id -> (outcome, reason)
    for (auto& e : ConformanceExpected) {
        expected[e.id] = {e.outcome, e.reason};
    }
    size_t passed = 0, differ = 0, canonical_checked = 0, canonical_differ = 0;
    std::map<std::string, size_t> by_reason;
    std::set<std::string> seen;
    xml::options keep;
    keep.keep_whitespace = true;
    for (auto& t : tests) {
        seen.insert(t.id);
        std::string doc = read_file(t.file);
        auto d = dump(doc);
        bool wf = t.type != "not-wf";
        std::string outcome = d.ok == wf ? "pass" : (wf ? "refused" : "accepted");
        auto e = expected.find(t.id);
        if (outcome == "pass") {
            ++passed;
            EXPECT_TRUE(e == expected.end() || e->second.first == "canonical") << t.id << " passes now; listed as " << e->second.first << " (" << e->second.second << ")";
        } else if (e == expected.end() || e->second.first != outcome) {
            ADD_FAILURE() << t.id << " (" << t.type << ", entities " << t.entities << ", " << t.file.substr(root.size()) << "): " << outcome
                          << (d.error ? ": " + std::string(d.error->message().view()) : "");
        } else {
            ++differ;
            ++by_reason[e->second.second];
        }
        if (outcome == "pass" && wf && !t.output.empty()) {
            // the nodes read() gives at the top: instructions and the root
            xml::reader r(string(doc), keep);
            std::string mine;
            while (auto n = r.read()) {
                if (n->type() != xml::kind::text) {
                    canonical(mine, *n);
                }
            }
            EXPECT_FALSE(r.last_error().has_value()) << t.id;
            bool same = mine == read_file(t.output);
            if (!same && (e == expected.end() || e->second.first != "canonical")) {
                ADD_FAILURE() << t.id << " " << t.file.substr(root.size()) << ": canonical form\n" << mine << "\n" << read_file(t.output);
            } else if (!same) {
                ++canonical_differ;
                ++by_reason[e->second.second];
            } else {
                EXPECT_TRUE(e == expected.end()) << t.id << " canonical form passes now";
            }
            ++canonical_checked;
        }
        // a stream in pieces gives the same
        auto cut = dump_stream(doc, 3);
        EXPECT_EQ(cut.tokens, d.tokens) << t.id;
        EXPECT_EQ(cut.ok, d.ok) << t.id;
    }
    for (auto& [id, e] : expected) {
        EXPECT_TRUE(seen.count(id)) << id << " is listed but not among the tests taken";
    }
    std::cout << "[ xmlconf  ] " << tests.size() << " tests: " << passed << " read as the suite says, " << differ
              << " decided otherwise by design; of " << canonical_checked << " with a canonical form, "
              << canonical_checked - canonical_differ << " written as the suite writes it, " << canonical_differ << " otherwise by design\n";
    for (auto& [reason, n] : by_reason) {
        std::cout << "[ xmlconf  ]   " << n << ": " << reason << "\n";
    }
}
