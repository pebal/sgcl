//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http::public_suffix, registrable_domain, is_public_suffix: the embedded
// Public Suffix List. The PSL project's test_psl.txt is not on this machine
// (and nothing is downloaded), so the cases are written from the list's
// rules and its documented algorithm: an unlisted TLD, one rule, two-level
// rules, a wildcard alone, wildcards with exceptions, the k12 rules of .us,
// IDN labels in Unicode and as punycode, the private section; then the whole
// list against a reference written plainly from the algorithm over the list's
// text (tools/public_suffix_list.dat, its rules converted by the txt
// module's IDNA rather than the generator's Python punycode): every rule's
// domain, a label under it and two, both sections and the ICANN one alone.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/public_suffix.h"

#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using namespace sgcl;

namespace {
    std::string text(const sgcl::string& s) {
        return std::string(s.data(), s.size());
    }

    std::string reg(const char* host) {
        return text(net::http::registrable_domain(host));
    }

    std::string suffix(const char* host) {
        return text(net::http::public_suffix(host));
    }

    // The list as the reference reads it: each rule's domain (A-labels) with
    // its kinds, per section
    struct Reference {
        enum : int { Rule = 1, Wild = 2, Exception = 4 };
        std::unordered_map<std::string, int> icann;
        std::unordered_map<std::string, int> all;
        size_t rules = 0;
        size_t refused = 0;

        Reference() {
            std::ifstream in(source_root() / "tools/public_suffix_list.dat");
            std::string line;
            bool icann_section = false;
            while (std::getline(in, line)) {
                if (line.rfind("// ===BEGIN ICANN DOMAINS===", 0) == 0) {
                    icann_section = true;
                } else if (line.rfind("// ===END ICANN DOMAINS===", 0) == 0) {
                    icann_section = false;
                }
                size_t a = line.find_first_not_of(" \t");
                if (a == std::string::npos || line.compare(a, 2, "//") == 0) {
                    continue;
                }
                std::string rule = line.substr(a, line.find_first_of(" \t\r", a) - a);
                int kind = Rule;
                if (rule[0] == '!') {
                    kind = Exception;
                    rule.erase(0, 1);
                } else if (rule.rfind("*.", 0) == 0) {
                    kind = Wild;
                    rule.erase(0, 2);
                }
                std::string key;
                bool ascii = true;
                for (char c : rule) {
                    ascii = ascii && uint8_t(c) < 0x80;
                }
                if (ascii) {
                    key = rule;
                } else {
                    auto r = txt::idna::to_ascii(sgcl::string(rule), txt::idna::options::whatwg());
                    if (!r) {
                        ++refused;
                        continue;
                    }
                    key = text(*r);
                }
                ++rules;
                all[key] |= kind;
                if (icann_section) {
                    icann[key] |= kind;
                }
            }
        }

        // The algorithm as publicsuffix.org writes it: every rule that
        // matches, an exception's prevailing, else the one of the most
        // labels, "*" when none matches; an exception's leftmost label off
        std::string suffix(const std::string& host, bool icann_only) const {
            auto& table = icann_only ? icann : all;
            std::vector<std::string> labels;
            size_t from = 0;
            for (;;) {
                size_t dot = host.find('.', from);
                labels.push_back(host.substr(from, dot - from));
                if (dot == std::string::npos) {
                    break;
                }
                from = dot + 1;
            }
            auto join = [&](size_t first) {
                std::string out;
                for (size_t i = first; i < labels.size(); ++i) {
                    out += (i > first ? "." : "") + labels[i];
                }
                return out;
            };
            auto kinds = [&](const std::string& key) {
                auto it = table.find(key);
                return it == table.end() ? 0 : it->second;
            };
            size_t best = 1;   // "*"
            for (size_t first = 0; first < labels.size(); ++first) {
                std::string s = join(first);
                size_t count = labels.size() - first;
                if (kinds(s) & Exception) {
                    return join(first + 1);
                }
                if (kinds(s) & Rule) {
                    best = std::max(best, count);
                }
                if (first + 1 < labels.size() && (kinds(join(first + 1)) & Wild)) {
                    best = std::max(best, count);
                }
            }
            return join(labels.size() - best);
        }
    };

    const Reference& reference() {
        static Reference r;
        return r;
    }
}

TEST(HttpPublicSuffix_Tests, AnUnlistedTld) {
    EXPECT_EQ(suffix("example"), "example");
    EXPECT_EQ(reg("example"), "");
    EXPECT_EQ(reg("example.example"), "example.example");
    EXPECT_EQ(reg("b.example.example"), "example.example");
    EXPECT_EQ(reg("a.b.example.example"), "example.example");
}

TEST(HttpPublicSuffix_Tests, ATldOfOneRule) {
    EXPECT_EQ(reg("biz"), "");
    EXPECT_EQ(reg("domain.biz"), "domain.biz");
    EXPECT_EQ(reg("b.domain.biz"), "domain.biz");
    EXPECT_EQ(reg("a.b.domain.biz"), "domain.biz");
}

TEST(HttpPublicSuffix_Tests, ATldWithRulesOfTwoLevels) {
    EXPECT_EQ(reg("com"), "");
    EXPECT_EQ(reg("example.com"), "example.com");
    EXPECT_EQ(reg("b.example.com"), "example.com");
    EXPECT_EQ(reg("a.b.example.com"), "example.com");
    EXPECT_EQ(reg("uk.com"), "");
    EXPECT_EQ(reg("example.uk.com"), "example.uk.com");
    EXPECT_EQ(reg("b.example.uk.com"), "example.uk.com");
    EXPECT_EQ(reg("a.b.example.uk.com"), "example.uk.com");
    EXPECT_EQ(reg("test.ac"), "test.ac");
    EXPECT_EQ(suffix("www.example.co.uk"), "co.uk");
    EXPECT_EQ(reg("www.example.co.uk"), "example.co.uk");
}

TEST(HttpPublicSuffix_Tests, ATldOfAWildcardAlone) {
    EXPECT_EQ(reg("mm"), "");
    EXPECT_EQ(reg("c.mm"), "");
    EXPECT_EQ(reg("b.c.mm"), "b.c.mm");
    EXPECT_EQ(reg("a.b.c.mm"), "b.c.mm");
    EXPECT_EQ(suffix("a.b.c.mm"), "c.mm");
}

TEST(HttpPublicSuffix_Tests, WildcardsAndExceptionsOfJapan) {
    EXPECT_EQ(reg("jp"), "");
    EXPECT_EQ(reg("test.jp"), "test.jp");
    EXPECT_EQ(reg("www.test.jp"), "test.jp");
    EXPECT_EQ(reg("ac.jp"), "");
    EXPECT_EQ(reg("test.ac.jp"), "test.ac.jp");
    EXPECT_EQ(reg("www.test.ac.jp"), "test.ac.jp");
    EXPECT_EQ(reg("kyoto.jp"), "");
    EXPECT_EQ(reg("test.kyoto.jp"), "test.kyoto.jp");
    EXPECT_EQ(reg("ide.kyoto.jp"), "");
    EXPECT_EQ(reg("b.ide.kyoto.jp"), "b.ide.kyoto.jp");
    EXPECT_EQ(reg("a.b.ide.kyoto.jp"), "b.ide.kyoto.jp");
    EXPECT_EQ(reg("c.kobe.jp"), "");
    EXPECT_EQ(reg("b.c.kobe.jp"), "b.c.kobe.jp");
    EXPECT_EQ(reg("a.b.c.kobe.jp"), "b.c.kobe.jp");
    EXPECT_EQ(reg("city.kobe.jp"), "city.kobe.jp");
    EXPECT_EQ(reg("www.city.kobe.jp"), "city.kobe.jp");
    EXPECT_EQ(suffix("www.city.kobe.jp"), "kobe.jp");
}

TEST(HttpPublicSuffix_Tests, AWildcardWithExceptions) {
    EXPECT_EQ(reg("ck"), "");
    EXPECT_EQ(reg("test.ck"), "");
    EXPECT_EQ(reg("b.test.ck"), "b.test.ck");
    EXPECT_EQ(reg("a.b.test.ck"), "b.test.ck");
    EXPECT_EQ(reg("www.ck"), "www.ck");
    EXPECT_EQ(reg("www.www.ck"), "www.ck");
    EXPECT_FALSE(net::http::is_public_suffix("www.ck"));
    EXPECT_TRUE(net::http::is_public_suffix("test.ck"));
}

TEST(HttpPublicSuffix_Tests, TheK12RulesOfTheUs) {
    EXPECT_EQ(reg("us"), "");
    EXPECT_EQ(reg("test.us"), "test.us");
    EXPECT_EQ(reg("www.test.us"), "test.us");
    EXPECT_EQ(reg("ak.us"), "");
    EXPECT_EQ(reg("test.ak.us"), "test.ak.us");
    EXPECT_EQ(reg("www.test.ak.us"), "test.ak.us");
    EXPECT_EQ(reg("k12.ak.us"), "");
    EXPECT_EQ(reg("test.k12.ak.us"), "test.k12.ak.us");
    EXPECT_EQ(reg("www.test.k12.ak.us"), "test.k12.ak.us");
}

TEST(HttpPublicSuffix_Tests, IdnLabelsInUnicodeAndAsPunycode) {
    EXPECT_EQ(reg("食狮.com.cn"), "xn--85x722f.com.cn");
    EXPECT_EQ(reg("食狮.公司.cn"), "xn--85x722f.xn--55qx5d.cn");
    EXPECT_EQ(reg("www.食狮.公司.cn"), "xn--85x722f.xn--55qx5d.cn");
    EXPECT_EQ(reg("shishi.公司.cn"), "shishi.xn--55qx5d.cn");
    EXPECT_EQ(reg("公司.cn"), "");
    EXPECT_EQ(reg("食狮.中国"), "xn--85x722f.xn--fiqs8s");
    EXPECT_EQ(reg("www.食狮.中国"), "xn--85x722f.xn--fiqs8s");
    EXPECT_EQ(reg("shishi.中国"), "shishi.xn--fiqs8s");
    EXPECT_EQ(reg("中国"), "");
    EXPECT_EQ(reg("xn--85x722f.com.cn"), "xn--85x722f.com.cn");
    EXPECT_EQ(reg("xn--85x722f.xn--55qx5d.cn"), "xn--85x722f.xn--55qx5d.cn");
    EXPECT_EQ(reg("www.xn--85x722f.xn--55qx5d.cn"), "xn--85x722f.xn--55qx5d.cn");
    EXPECT_EQ(reg("shishi.xn--55qx5d.cn"), "shishi.xn--55qx5d.cn");
    EXPECT_EQ(reg("xn--55qx5d.cn"), "");
    EXPECT_EQ(reg("xn--85x722f.xn--fiqs8s"), "xn--85x722f.xn--fiqs8s");
    EXPECT_EQ(reg("www.xn--85x722f.xn--fiqs8s"), "xn--85x722f.xn--fiqs8s");
    EXPECT_EQ(reg("shishi.xn--fiqs8s"), "shishi.xn--fiqs8s");
    EXPECT_EQ(reg("xn--fiqs8s"), "");
    // mapped by UTS #46 first: upper case and full width
    EXPECT_EQ(reg("WWW.BÜCHER.DE"), "xn--bcher-kva.de");
    EXPECT_EQ(suffix("ｅｘａｍｐｌｅ．ｃｏｍ"), "com");
}

TEST(HttpPublicSuffix_Tests, ThePrivateSectionAndTheIcannOneAlone) {
    using net::http::suffix_rules;
    EXPECT_EQ(suffix("me.github.io"), "github.io");
    EXPECT_EQ(reg("me.github.io"), "me.github.io");
    EXPECT_EQ(reg("github.io"), "");
    EXPECT_TRUE(net::http::is_public_suffix("github.io"));
    EXPECT_EQ(text(net::http::public_suffix("me.github.io", suffix_rules::icann)), "io");
    EXPECT_EQ(text(net::http::registrable_domain("me.github.io", suffix_rules::icann)), "github.io");
    EXPECT_FALSE(net::http::is_public_suffix("github.io", suffix_rules::icann));
    EXPECT_EQ(reg("www.blogspot.com"), "www.blogspot.com");
    EXPECT_EQ(text(net::http::registrable_domain("www.blogspot.com", suffix_rules::icann)), "blogspot.com");
    // the ICANN section's own wildcards and exceptions hold alone too
    EXPECT_EQ(text(net::http::registrable_domain("www.city.kobe.jp", suffix_rules::icann)), "city.kobe.jp");
    EXPECT_EQ(text(net::http::registrable_domain("a.b.c.mm", suffix_rules::icann)), "b.c.mm");
}

TEST(HttpPublicSuffix_Tests, MixedCaseAndADotAtTheEnd) {
    EXPECT_EQ(suffix("COM"), "com");
    EXPECT_EQ(reg("COM"), "");
    EXPECT_EQ(reg("example.COM"), "example.com");
    EXPECT_EQ(reg("WwW.example.COM"), "example.com");
    EXPECT_EQ(reg("www.example.com."), "example.com");
    EXPECT_EQ(suffix("com."), "com");
    EXPECT_TRUE(net::http::is_public_suffix("co.uk."));
}

TEST(HttpPublicSuffix_Tests, WhatIsNoHostHasNoSuffix) {
    for (const char* none : {"", ".", "..", ".com", ".example", ".example.com", ".example.example", "a..com", "example..",
                             "127.0.0.1", "10.1", "0x7f.1", "1.2.3.04", "::1", "[::1]", "2001:db8::1", "a\xff.com"}) {
        EXPECT_EQ(suffix(none), "") << none;
        EXPECT_EQ(reg(none), "") << none;
        EXPECT_FALSE(net::http::is_public_suffix(none)) << none;
    }
    // a name whose last label begins with a letter is a name
    EXPECT_EQ(reg("1.2.3.com"), "3.com");
    EXPECT_EQ(suffix("localhost"), "localhost");
    EXPECT_TRUE(net::http::is_public_suffix("localhost"));
    EXPECT_EQ(reg("app.localhost"), "app.localhost");
}

TEST(HttpPublicSuffix_Tests, LongNames) {
    std::string label(63, 'a');
    std::string host;
    for (int i : range(30)) {
        (void)i;
        host += label + ".";
    }
    host += "example.co.uk";
    EXPECT_EQ(reg(host.c_str()), "example.co.uk");
    std::string one(5000, 'b');
    EXPECT_EQ(suffix(one.c_str()), one);
    EXPECT_EQ(reg((one + ".com").c_str()), one + ".com");
}

// Every rule of the list: its domain, one label under it and two, against
// the reference, in both modes
TEST(HttpPublicSuffix_Tests, TheWholeListAgainstTheReference) {
    const Reference& ref = reference();
    ASSERT_GT(ref.rules, 9000u);
    EXPECT_EQ(ref.refused, 0u);
    size_t checked = 0, wrong = 0;
    for (auto& kv : ref.all) {
        for (const char* under : {"", "x.", "y.x."}) {
            std::string host = under + kv.first;
            for (bool icann : {false, true}) {
                auto mode = icann ? net::http::suffix_rules::icann : net::http::suffix_rules::all;
                std::string want = ref.suffix(host, icann);
                std::string got = text(net::http::public_suffix(sgcl::string(host), mode));
                std::string want_reg;
                if (want.size() < host.size()) {
                    size_t dot = host.rfind('.', host.size() - want.size() - 2);
                    want_reg = host.substr(dot == std::string::npos ? 0 : dot + 1);
                }
                std::string got_reg = text(net::http::registrable_domain(sgcl::string(host), mode));
                ++checked;
                if (got != want || got_reg != want_reg) {
                    if (++wrong < 20) {
                        ADD_FAILURE() << host << (icann ? " (icann)" : "") << ": " << got << " / " << got_reg << ", the reference " << want << " / " << want_reg;
                    }
                }
            }
        }
    }
    EXPECT_EQ(wrong, 0u);
    EXPECT_GT(checked, 50000u);
}

// The table of the header is the list of the tree: its date and that every
// rule's domain is a key the table knows
TEST(HttpPublicSuffix_Tests, TheTableIsTheListOfTheTree) {
    EXPECT_STREQ(net::http::detail::PslDate, "2024-01-07");
    size_t missing = 0;
    for (auto& kv : reference().all) {
        uint32_t h = 2166136261u;
        for (size_t i = kv.first.size(); i-- > 0;) {
            h = (h ^ uint8_t(kv.first[i])) * 16777619u;
        }
        int bits = net::http::detail::psl_bits(kv.first, h);
        missing += bits < 0 || ((bits | (bits >> 3)) & 7) == 0;
    }
    EXPECT_EQ(missing, 0u);
}
