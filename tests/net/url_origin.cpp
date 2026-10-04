//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net: the path of an origin-form request-target found without the parse
// (url.h: detail::origin_form_path), which net::http routes by, held to the
// parser: for every host and target of a corpus, the answer is the path
// url::parse("http://" + host + target) gives, or a refusal; never another
// path, and never an answer where the parse fails. The corpus: the web
// platform tests' urltestdata (url_oracle.h: its inputs, and the paths and
// queries of its results, as targets; its hosts), the fuzzers' seeds of the
// url and of the HTTP heads (their request-targets), cases of every refusal,
// and ten thousand mutations of all of them.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/url.h"
#include "tests/net/url_oracle.h"

#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace sgcl;

namespace {
    struct Tally {
        size_t checked = 0;
        size_t answered = 0;
        size_t wrong = 0;
        std::string first_wrong;
    };

    void check(const std::string& host, const std::string& target, Tally& t) {
        ++t.checked;
        auto fast = net::detail::origin_form_path(host, target);
        if (!fast) {
            return;
        }
        ++t.answered;
        auto parsed = net::url::parse(string("http://" + host + target));
        if (!parsed || parsed->path().view() != *fast) {
            if (!t.wrong++) {
                t.first_wrong = "host [" + host + "] target [" + target + "] fast [" + std::string(*fast) + "] parse [" + (parsed ? std::string(parsed->path().view()) : std::string("failure")) + "]";
            }
        }
    }

    // The request-targets of the request lines in a file
    void targets_of_heads(const std::filesystem::path& file, std::set<std::string>& out) {
        std::ifstream in(file, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        size_t at = 0;
        while (at < text.size()) {
            size_t eol = text.find('\n', at);
            std::string line = text.substr(at, eol == std::string::npos ? std::string::npos : eol - at);
            size_t a = line.find(' ');
            size_t b = a == std::string::npos ? a : line.find(' ', a + 1);
            if (b != std::string::npos) {
                out.insert(line.substr(a + 1, b - a - 1));
            }
            if (eol == std::string::npos) {
                break;
            }
            at = eol + 1;
        }
    }
}

TEST(NetUrlOrigin_Tests, TheFastPathIsTheParsersOrARefusal) {
    std::set<std::string> targets = {
        "/", "/a", "/a/b", "/a//b", "/a/b/", "/a?x=1", "/a?x=%zz#q", "/?", "/a/b?c/../d",
        // each refusal
        "", "a", "*", "//x", "//", "/%41", "/a/./b", "/a/../b", "/.", "/..", "/a/.", "/a/..", "/a\\b", "/a#f",
        "/a b", "/a\tb", "/a\nb", "/a\"b", "/a<b", "/a>b", "/a`b", "/a{b", "/a}b", "/\x7f", "/\xc3\xa9",
        "/.hidden", "/..x", "/x.", "/a/.../b", "/%2e/", "/a;b", "/a:b", "/a@b", "/~x", "/a+b", "/a'b", "/a|b", "/a[b]",
    };
    std::set<std::string> hosts = {
        "h", "localhost", "example.com", "EXAMPLE.com", "example.com:8080", "a-b.c", "a.b.c.d.e",
        "", "h:", "h:0", "h:65535", "h:65536", "h:123456", "h:x", "a..b", "a.b.", ".a", "1.2.3.4", "a.0x", "a.1", "0x7f.1",
        "a_b", "[::1]", "[::1]:80", "xn--bcher-kva.de", "b\xc3\xbc" "cher.de", "h%41", "a b", "-a", "a-",
        // the parser's own host tests: IPv4 in its forms, numbers it refuses, the bytes it takes
        "127.0.0.1", "127.0.0.1:18093", "0X7F.1", "0x7F.0.0.1", "0x", "0x.1", "a.0X", "08", "a.08", "a.09.1",
        "1.2.3.", "1.2.3.4.", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "4294967295", "4294967296", "0xffffffff",
        "0x100000000", "1.16777215", "1.16777216", "a.b.", "A.B", "..", ".", "a!b", "a$b", "a&b", "a'b", "a(b)",
        "a*b", "a+b", "a,b", "a;b", "a=b", "a~b", "a\"b", "a{b}", "a`b", "a^b", "a|b", "a\\b", "a<b", "a>b",
        "a\x7f", "a\x01", "h:080", "h:00000080", "h::80", "1.2.3.4:80",
    };
    for (auto& c : url_oracle::parse_cases) {
        std::string input(c.input);
        targets.insert(input);
        if (!c.failure) {
            targets.insert(std::string(c.pathname) + std::string(c.search));
            if (!c.hostname.empty()) {
                hosts.insert(std::string(c.host));
            }
        }
    }
    auto root = source_root() / "tests/net";
    for (auto dir : {root / "fuzz" / "seeds" / "url"}) {
        if (std::filesystem::exists(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                std::ifstream in(e.path(), std::ios::binary);
                std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                targets.insert(text);
                auto p = text.find("://");
                if (p != std::string::npos) {
                    auto slash = text.find('/', p + 3);
                    if (slash != std::string::npos) {
                        targets.insert(text.substr(slash));
                    }
                }
            }
        }
    }
    for (auto name : {"http_head", "http_server", "http_fields"}) {
        auto dir = root / "http" / "fuzz" / "seeds" / name;
        if (std::filesystem::exists(dir)) {
            for (auto& e : std::filesystem::directory_iterator(dir)) {
                targets_of_heads(e.path(), targets);
            }
        }
    }
    // ten thousand mutations of the targets: a byte of the interesting ones
    // put in, a byte taken out, a byte changed
    std::vector<std::string> base(targets.begin(), targets.end());
    std::mt19937 rng(20260927);
    const char interesting[] = "/.%\\#?\" <>`{}\t\n\x7f" "\xc3" "\xa9" "azAZ09-_~:;@!$&'()*+,=[]|";
    for (int i = 0; i < 10000; ++i) {
        std::string t = base[rng() % base.size()];
        if (t.empty()) {
            t = "/";
        }
        int edits = 1 + int(rng() % 3);
        for (int k = 0; k < edits; ++k) {
            size_t at = rng() % (t.size() + 1);
            char c = interesting[rng() % (sizeof(interesting) - 1)];
            switch (rng() % 3) {
                case 0: t.insert(t.begin() + long(at), c); break;
                case 1: if (at < t.size()) t.erase(at, 1); break;
                default: if (at < t.size()) t[at] = c; break;
            }
        }
        targets.insert(t);
    }
    Tally tally;
    for (auto& h : hosts) {
        for (auto& t : targets) {
            check(h, t, tally);
        }
    }
    // two thousand mutations of the hosts, against the targets the fast
    // path answers for localhost (three hundred of them, every 1 in k)
    std::vector<std::string> answered;
    for (auto& t : targets) {
        if (net::detail::origin_form_path("localhost", t)) {
            answered.push_back(t);
        }
    }
    std::vector<std::string> sample;
    for (size_t i = 0; i < answered.size(); i += answered.size() / 300 + 1) {
        sample.push_back(answered[i]);
    }
    std::vector<std::string> host_base(hosts.begin(), hosts.end());
    const char host_bytes[] = ".:%[]0123456789xXabfgzAZ-_!$&'()*+,;=~\"{}`^|\\<> \t\x7f\x01" "\xc3" "\xbc";
    std::set<std::string> mutated;
    for (int i = 0; i < 2000; ++i) {
        std::string h = host_base[rng() % host_base.size()];
        int edits = 1 + int(rng() % 3);
        for (int k = 0; k < edits; ++k) {
            size_t at = rng() % (h.size() + 1);
            char c = host_bytes[rng() % (sizeof(host_bytes) - 1)];
            switch (rng() % 3) {
                case 0: h.insert(h.begin() + long(at), c); break;
                case 1: if (at < h.size()) h.erase(at, 1); break;
                default: if (at < h.size()) h[at] = c; break;
            }
        }
        if (!hosts.count(h)) {
            mutated.insert(h);
        }
    }
    for (auto& h : mutated) {
        for (auto& t : sample) {
            check(h, t, tally);
        }
    }
    std::printf("hosts %zu (+%zu mutated) targets %zu checked %zu answered %zu\n", hosts.size(), mutated.size(), targets.size(), tally.checked, tally.answered);
    EXPECT_EQ(tally.wrong, 0u) << tally.first_wrong;
    EXPECT_GT(tally.answered, 10000u);   // the fast path answers many of them, not a handful
    // the common case is answered, as a slice of the target
    std::string target = "/items/42?sort=asc";
    auto p = net::detail::origin_form_path("localhost:8080", target);
    ASSERT_TRUE(p);
    EXPECT_EQ(*p, "/items/42");
    EXPECT_EQ(p->data(), target.data());
}
