//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// io::glob and io::glob_pattern: `**`, braces, the hidden rule, a trailing
// "/". Python's glob (recursive=True, include_hidden) is the oracle of the
// walk, path for path on a tree made here, and glob.translate's regular
// expression the oracle of match() on pairs of patterns and paths; Python
// has no braces, so a pattern with them is held to the union of Python's
// results for its alternatives written out by hand. Then what Python has no
// counterpart of: links not followed by `**`, the limits, the errors.
#include "tests/types.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
    namespace io = sgcl::io;
    namespace fs = std::filesystem;
    using sgcl::string;
    using sgcl::vector;

    bool have_python() {
        return std::system("python3 -c 'import glob; glob.translate' > /dev/null 2>&1") == 0;
    }

    // The script run on the input, in a directory of its own (not the tree's)
    std::string run_python(const std::string& script, const std::string& input) {
        const fs::path dir = fs::path(io::make_temp_dir({}, "sgcl-glob-oracle-*").value().str());
        {
            std::ofstream s(dir / "oracle.py");
            s << script;
            std::ofstream i(dir / "oracle.in", std::ios::binary);
            i << input;
        }
        std::string cmd = "python3 '" + (dir / "oracle.py").string() + "' < '" + (dir / "oracle.in").string() + "' > '" + (dir / "oracle.out").string() + "' 2>&1";
        EXPECT_EQ(std::system(cmd.c_str()), 0);
        std::ifstream o(dir / "oracle.out", std::ios::binary);
        std::stringstream ss;
        ss << o.rdbuf();
        fs::remove_all(dir);
        return ss.str();
    }

    // The sections ("== pattern" and its lines) that differ, for the failure's message
    std::string differences(const std::string& ours, const std::string& theirs) {
        auto sections = [](const std::string& text) {
            std::vector<std::string> out;
            size_t at = 0;
            while (at < text.size()) {
                size_t next = text.find("==", at + 2);
                while (next != std::string::npos && next > 0 && text[next - 1] != '\n') {
                    next = text.find("==", next + 2);
                }
                out.push_back(text.substr(at, next == std::string::npos ? std::string::npos : next - at));
                at = next == std::string::npos ? text.size() : next;
            }
            return out;
        };
        auto a = sections(ours), b = sections(theirs);
        std::string d;
        for (size_t k = 0; k < std::max(a.size(), b.size()); ++k) {
            std::string x = k < a.size() ? a[k] : "", y = k < b.size() ? b[k] : "";
            if (x != y) {
                d += "ours:\n" + x + "python:\n" + y;
            }
        }
        return d;
    }

    std::string joined(const vector<string>& paths) {
        std::string out;
        for (auto& p : paths) {
            out += std::string(p.view()) + "\n";
        }
        return out;
    }

    struct Tree {
        fs::path root;

        Tree() {
            root = fs::path(io::make_temp_dir({}, "sgcl-glob-*").value().str());
            for (const char* d : {"a/b/c", "a/.h/x", "a/b c", "d", "e/f/g/h", ".top/i", "j[k]", "{x}"}) {
                fs::create_directories(root / d);
            }
            for (const char* f : {"a/f.txt", "a/b/g.txt", "a/b/c/h.txt", "a/b/c/h.cpp", "a/.hid", "a/.h/x/y.txt", "a/b c/s p.txt",
                                  "d/z.cpp", "d/z.h", "top.txt", ".dot", "e/f/g/h/deep.txt", ".top/i/j.txt", "j[k]/l.txt",
                                  "{x}/m.txt", "a/b/ä.txt", "a/b/q?.txt", "e/f/g/x.h"}) {
                std::ofstream(root / f) << "x";
            }
        }

        ~Tree() {
            fs::remove_all(root);
        }

        string at(const std::string& pattern) const {
            return string(root.string() + "/" + pattern);
        }
    };

    const std::vector<std::string> Patterns = {
        "*", "**", "**/", "*/", "**/*", "**/*.txt", "a/**", "a/**/", "a/**/*.txt", "a/*/**", "a/**/c/*", "**/c/**",
        "a/b/*.txt", "a/[bc]*", "a/[!b]*", "a/?", "*/*/*", "**/*.h", "**/**/*.txt", "a/**/**", "e/**/h/*",
        ".*", "a/.*", "**/.*", ".top/**", "a/.h/**", "a/b/c", "a/b/c/", "a/f.txt", "a/f.txt/", "missing", "missing/*",
        "a/b c/*", "a/b/[ä]*", "a/b/q[?].txt", "j[[]k]/*", "{x}/*", "*.txt", "**/[a-d]", "a/b/**/*.cpp",
    };
}

TEST(IoGlob_Tests, AsPythonWalks) {
    if (!have_python()) {
        GTEST_SKIP() << "no python3 with glob.translate (3.13)";
    }
    Tree t;
    std::string input;
    for (bool hidden : {false, true}) {
        for (auto& p : Patterns) {
            input += (hidden ? "1" : "0") + t.at(p).str() + "\n";
        }
    }
    std::string py = run_python(R"(
import glob, sys
for line in sys.stdin.read().split('\n'):
    if not line:
        continue
    hidden, pattern = line[0] == '1', line[1:]
    print('== ' + pattern)
    for p in sorted(set(glob.glob(pattern, recursive=True, include_hidden=hidden))):
        print(p)
)", input);
    std::string ours;
    for (bool hidden : {false, true}) {
        for (auto& p : Patterns) {
            ours += "== " + t.at(p).str() + "\n";
            auto r = io::glob(t.at(p), {.hidden = hidden});
            EXPECT_TRUE(r) << p;
            if (r) {
                ours += joined(*r);
            }
        }
    }
    EXPECT_EQ(ours, py) << differences(ours, py);
}

// match() against glob.translate's expression (Python 3.13), the same
// pairs with the hidden rule and without
TEST(IoGlob_Tests, MatchAsPythonTranslates) {
    if (!have_python()) {
        GTEST_SKIP() << "no python3 with glob.translate (3.13)";
    }
    const std::vector<std::string> patterns = {
        "*", "**", "*/*", "**/*", "**/*.txt", "a/**", "a/**/b", "a/**/b/**", "**/b/**", "a/*/c", "a/?/c", "[ab]/**",
        ".*", "**/.*", "a/.*/c", "*.txt", "a**b", "**b", "a/**/**/c", "a/b", "*/**/*/*", "**/x/*.h",
    };
    const std::vector<std::string> paths = {
        "a", "a/", "b/", "a/b/", "a/b", "a/b/c", "a/x/b", "a/x/y/b", "a/.x/b", ".a", ".a/b", "a/b/.c", "x.txt", ".x.txt", "a/x.txt",
        "a/.b/x.txt", "ab", "axxb", "a/bb", "c", "a/b/c/d/e", "x/y.h", "p/x/y.h", "p/.x/y.h", "a/xb", "b/b/b", "q/a/b/c",
    };
    std::string input;
    for (bool hidden : {false, true}) {
        for (auto& p : patterns) {
            for (auto& q : paths) {
                input += (hidden ? "1" : "0") + p + "\t" + q + "\n";
            }
        }
    }
    std::string py = run_python(R"(
import glob, re, sys
out = []
for line in sys.stdin.read().split('\n'):
    if not line:
        continue
    hidden = line[0] == '1'
    pattern, path = line[1:].split('\t')
    rx = glob.translate(pattern, recursive=True, include_hidden=hidden, seps='/')
    out.append('1' if re.fullmatch(rx, path) else '0')
print(''.join(out))
)", input);
    std::string ours;
    for (bool hidden : {false, true}) {
        for (auto& p : patterns) {
            io::glob_pattern g(string(p), {.hidden = hidden});
            for (auto& q : paths) {
                ours += g.match(string(q)) ? '1' : '0';
            }
        }
    }
    ours += '\n';
    ASSERT_EQ(ours.size(), py.size()) << py;
    // where Python's own two disagree, the walk's is this one's: a class
    // that begins a component takes no hidden name in glob.glob, and does
    // in glob.translate ("[!a]*" against ".x")
    EXPECT_FALSE(io::glob_pattern(string("[!a]*")).match(string(".x")));
    EXPECT_TRUE(io::glob_pattern(string("[!a]*"), {.hidden = true}).match(string(".x")));
    size_t k = 0;
    for (bool hidden : {false, true}) {
        for (auto& p : patterns) {
            for (auto& q : paths) {
                EXPECT_EQ(ours[k], py[k]) << "hidden " << hidden << " pattern " << p << " path " << q;
                ++k;
            }
        }
    }
}

TEST(IoGlob_Tests, BracesAsTheUnionOfTheirAlternatives) {
    if (!have_python()) {
        GTEST_SKIP() << "no python3";
    }
    Tree t;
    struct Case {
        std::string pattern;
        std::vector<std::string> alternatives;
    };
    const std::vector<Case> cases = {
        {"**/*.{txt,cpp}", {"**/*.txt", "**/*.cpp"}},
        {"{a,d}/*", {"a/*", "d/*"}},
        {"a/{b/{c,x},f.txt}", {"a/b/c", "a/b/x", "a/f.txt"}},
        {"{a,a}/f.txt", {"a/f.txt"}},
        {"{,a/}*.txt", {"*.txt", "a/*.txt"}},
        {"e/**/{g,h}/*", {"e/**/g/*", "e/**/h/*"}},
        {"{x}/*", {"{x}/*"}},          // no comma: braces as they are
        {"[{]x}/*", {"{x}/*"}},        // a brace in a class is the character
    };
    std::string input;
    for (auto& c : cases) {
        input += "==\n";
        for (auto& a : c.alternatives) {
            input += t.at(a).str() + "\n";
        }
    }
    std::string py = run_python(R"(
import glob, sys
groups = sys.stdin.read().split('==\n')[1:]
for g in groups:
    found = set()
    for pattern in g.split('\n'):
        if pattern:
            found |= set(glob.glob(pattern, recursive=True))
    print('==')
    for p in sorted(found):
        print(p)
)", input);
    std::string ours;
    for (auto& c : cases) {
        ours += "==\n";
        auto r = io::glob(t.at(c.pattern));
        EXPECT_TRUE(r) << c.pattern;
        if (r) {
            ours += joined(*r);
        }
    }
    EXPECT_EQ(ours, py) << differences(ours, py);
}

TEST(IoGlob_Tests, LinksAreNotFollowedByDoubleStar) {
    Tree t;
    fs::create_directory_symlink(t.root / "a", t.root / "d/loop");   // a loop for a walk that follows links
    auto all = io::glob(t.at("d/**"));
    ASSERT_TRUE(all);
    EXPECT_EQ(joined(*all), t.at("d/").str() + "\n" + t.at("d/loop").str() + "\n" + t.at("d/z.cpp").str() + "\n" + t.at("d/z.h").str() + "\n");
    // a wildcard in the middle goes through it, as Go and Python do
    auto through = io::glob(t.at("d/*/f.txt"));
    ASSERT_TRUE(through);
    EXPECT_EQ(joined(*through), t.at("d/loop/f.txt").str() + "\n");
    // and a trailing "/" takes the link for the directory it is
    auto dirs = io::glob(t.at("d/*/"));
    ASSERT_TRUE(dirs);
    EXPECT_EQ(joined(*dirs), t.at("d/loop/").str() + "\n");
}

TEST(IoGlob_Tests, MalformedPatterns) {
    for (const char* bad : {"a/[b", "[z-a]", "x\\", "a/[]"}) {
        auto r = io::glob(string(bad));
        ASSERT_FALSE(r) << bad;
        EXPECT_EQ(r.error().code(), io::errc::invalid_pattern);
        EXPECT_EQ(r.error().path(), string(bad));
        EXPECT_FALSE(io::glob_pattern::parse(string(bad)));
        EXPECT_THROW(io::glob_pattern(string(bad)), sgcl::bad_expected_access<io::error>);
    }
    // braces past 1024 alternatives: eleven pairs are 2048
    std::string many;
    for (int i = 0; i < 11; ++i) {
        many += "{a,b}";
    }
    EXPECT_EQ(io::glob(string(many)).error().code(), io::errc::invalid_pattern);
    std::string ten;
    for (int i = 0; i < 10; ++i) {
        ten += "{a,b}";
    }
    EXPECT_TRUE(io::glob_pattern::parse(string(ten)));   // 1024: the most
    // the empty pattern names nothing; unmatched braces are characters
    EXPECT_TRUE(io::glob(string())->empty());
    EXPECT_TRUE(io::glob_pattern(string("{a,b")).match(string("{a,b")));
    EXPECT_TRUE(io::glob_pattern(string("a,b}")).match(string("a,b}")));
    EXPECT_TRUE(io::glob_pattern(string("\\{a,b}")).match(string("{a,b}")));   // escaped
}

TEST(IoGlob_Tests, ThePattern) {
    io::glob_pattern p(string("src/**/*.{cpp,h}"));
    EXPECT_EQ(p.text(), "src/**/*.{cpp,h}");
    EXPECT_FALSE(p.is_literal());
    EXPECT_TRUE(p.match(string("src/a.cpp")));
    EXPECT_TRUE(p.match(string("src/x/y/z.h")));
    EXPECT_FALSE(p.match(string("src/x/y/z.hpp")));
    EXPECT_FALSE(p.match(string("src/.x/z.h")));    // hidden
    EXPECT_FALSE(p.match(string("/src/a.cpp")));    // rooted is another path
    EXPECT_FALSE(p.match(string("lib/a.cpp")));
    EXPECT_TRUE(io::glob_pattern(string("a/b.txt")).is_literal());
    EXPECT_FALSE(io::glob_pattern(string("a/{b,c}")).is_literal());
    EXPECT_TRUE(io::glob_pattern(string("a/{b}")).is_literal());
    io::glob_pattern copy = p;
    EXPECT_EQ(copy.text(), p.text());
    // hidden names by the option, never "." and ".."
    io::glob_pattern all(string("**/*"), {.hidden = true});
    EXPECT_TRUE(all.match(string(".x/.y")));
    EXPECT_FALSE(all.match(string("a/../b")));
    EXPECT_FALSE(io::glob_pattern(string(".*")).match(string("..")));
    EXPECT_TRUE(io::glob_pattern(string(".*")).match(string(".x")));
    // a trailing "/": directories
    io::glob_pattern dirs(string("a/**/"));
    EXPECT_TRUE(dirs.match(string("a/")));
    EXPECT_TRUE(dirs.match(string("a/b/")));
    EXPECT_FALSE(dirs.match(string("a/b")));
    // the root
    EXPECT_TRUE(io::glob_pattern(string("/")).match(string("/")));
    auto root = io::glob(string("/"));
    ASSERT_TRUE(root);
    EXPECT_EQ(joined(*root), "/\n");
    // many `**`: no blow-up on a long path that does not match
    io::glob_pattern stars(string("**/a/**/a/**/a/**/a/**/a/**/b"));
    std::string long_path;
    for (int i = 0; i < 200; ++i) {
        long_path += "a/";
    }
    long_path += "c";
    EXPECT_FALSE(stars.match(string(long_path)));
}

namespace {
    sgcl::async::task<std::string> glob_in_a_task(string pattern) {
        auto r = co_await io::async_glob(pattern);
        co_return r ? std::to_string(r->size()) : std::string(r.error().message().view());
    }
}

TEST(IoGlob_Tests, FromATask) {
    Tree t;
    EXPECT_EQ(sgcl::async::spawn(glob_in_a_task(t.at("**/*.txt"))).wait(), "10");
    EXPECT_EQ(sgcl::async::spawn(glob_in_a_task(string("[x"))).wait(), "glob [x: invalid pattern");
    // the glob of a compiled pattern
    io::glob_pattern p(t.at("a/b/*"));
    auto r = io::glob(p);
    ASSERT_TRUE(r);
    EXPECT_EQ(joined(*r), t.at("a/b/c").str() + "\n" + t.at("a/b/g.txt").str() + "\n" + t.at("a/b/q?.txt").str() + "\n" + t.at("a/b/ä.txt").str() + "\n");
}
