//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt diff, patch and merge: apply_patch against /usr/bin/patch and merge3
// against git merge-file over the cases of tests/txt/diff_vectors.h
// (tools/diff_vectors.py, which also checks unified_diff through patch(1));
// edit scripts that rebuild the new text and, by Myers, are shortest (a
// dynamic program here); then the formats, the options and the boundaries.
#include "tests/types.h"
#include "tests/txt/diff_vectors.h"

#include <random>
#include <string>
#include <vector>

namespace {
    string s(const char* t) {
        return string(t);
    }

    std::string str(const string& t) {
        return std::string(t.view());
    }

    // the new text rebuilt from the old one and the edits, which must cover
    // both texts in order
    std::string rebuild(const std::string& a, const std::string& b, const vector<txt::diff_edit>& edits, bool& ok) {
        std::string out;
        size_t pa = 0, pb = 0;
        ok = true;
        for (const auto& e : edits) {
            ok = ok && e.old_begin == pa && e.new_begin == pb && e.old_begin <= e.old_end && e.new_begin <= e.new_end;
            switch (e.kind) {
                case txt::diff_kind::equal:
                    ok = ok && a.substr(e.old_begin, e.old_end - e.old_begin) == b.substr(e.new_begin, e.new_end - e.new_begin);
                    out += a.substr(e.old_begin, e.old_end - e.old_begin);
                    break;
                case txt::diff_kind::insert:
                    ok = ok && e.old_begin == e.old_end;
                    out += b.substr(e.new_begin, e.new_end - e.new_begin);
                    break;
                case txt::diff_kind::remove:
                    ok = ok && e.new_begin == e.new_end;
                    break;
            }
            pa = e.old_end;
            pb = e.new_end;
        }
        ok = ok && pa == a.size() && pb == b.size();
        return out;
    }

    std::vector<std::string> lines(const std::string& t) {
        std::vector<std::string> out;
        size_t b = 0;
        while (b < t.size()) {
            size_t e = t.find('\n', b);
            e = e == std::string::npos ? t.size() : e + 1;
            out.push_back(t.substr(b, e - b));
            b = e;
        }
        return out;
    }

    size_t lcs(const std::vector<std::string>& a, const std::vector<std::string>& b) {
        std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < b.size(); ++j) {
                cur[j + 1] = a[i] == b[j] ? prev[j] + 1 : std::max(prev[j + 1], cur[j]);
            }
            std::swap(prev, cur);
        }
        return prev[b.size()];
    }

    const char* const Words[] = {"alpha", "beta", "gamma", "x", "", "  indented", "}", "{", "ąę"};

    std::string random_text(std::mt19937& g, size_t n) {
        std::string t;
        for (size_t i = 0; i < n; ++i) {
            t += Words[g() % 9];
            t += '\n';
        }
        if (!t.empty() && g() % 5 == 0) {
            t.pop_back();
        }
        return t;
    }

    std::string mutate(std::mt19937& g, const std::string& t) {
        std::vector<std::string> ls = lines(t);
        for (int k = int(g() % 6); k > 0; --k) {
            size_t at = ls.empty() ? 0 : g() % (ls.size() + 1);
            switch (g() % 3) {
                case 0:
                    ls.insert(ls.begin() + long(at), std::string(Words[g() % 9]) + "\n");
                    break;
                case 1:
                    if (!ls.empty()) {
                        ls.erase(ls.begin() + long(std::min(at, ls.size() - 1)));
                    }
                    break;
                default:
                    if (!ls.empty()) {
                        ls[std::min(at, ls.size() - 1)] = std::string(Words[g() % 9]) + "!\n";
                    }
            }
        }
        std::string out;
        for (auto& l : ls) {
            out += l;
        }
        if (!out.empty() && out.back() == '\n' && g() % 8 == 0) {
            out.pop_back();
        }
        if (!out.empty() && out.back() != '\n' && g() % 2 == 0) {
            out += '\n';
        }
        return out;
    }
}

TEST(Diff_Tests, PatchVectors) {
    size_t n = 0;
    for (const auto& v : PatchVectors) {
        auto r = txt::apply_patch(s(v.text), s(v.patch), {.fuzz = v.fuzz, .reverse = v.reverse});
        if (v.result == nullptr) {
            EXPECT_FALSE(r.has_value()) << v.patch << "\non\n" << v.text << "\ngave\n" << (r ? str(*r) : "");
        } else {
            ASSERT_TRUE(r.has_value()) << v.patch << "\non\n" << v.text << "\n" << r.error().message().c_str();
            EXPECT_EQ(str(*r), v.result) << v.patch << "\non\n" << v.text;
        }
        ++n;
    }
    EXPECT_GT(n, 300u);
}

TEST(Diff_Tests, MergeVectors) {
    size_t n = 0;
    for (const auto& v : MergeVectors) {
        auto m = txt::merge3(s(v.base), s(v.ours), s(v.theirs), {.diff3 = v.diff3});
        EXPECT_EQ(str(m.text), v.merged) << "base\n" << v.base << "\nours\n" << v.ours << "\ntheirs\n" << v.theirs;
        EXPECT_EQ(m.conflicts, v.conflicts);
        ++n;
    }
    EXPECT_GT(n, 350u);
}

TEST(Diff_Tests, ScriptsRebuildAndMyersIsShortest) {
    std::mt19937 g(2026);
    for (int round = 0; round < 600; ++round) {
        std::string a = random_text(g, g() % 30);
        std::string b = round % 4 == 0 ? random_text(g, g() % 30) : mutate(g, a);
        for (auto alg : {txt::diff_algorithm::myers, txt::diff_algorithm::patience}) {
            auto edits = txt::diff_lines(string(a), string(b), {.algorithm = alg});
            bool ok;
            EXPECT_EQ(rebuild(a, b, edits, ok), b);
            EXPECT_TRUE(ok) << a << "\n--\n" << b;
            size_t changed = 0;
            for (const auto& e : edits) {
                if (e.kind == txt::diff_kind::remove) {
                    changed += lines(a.substr(e.old_begin, e.old_end - e.old_begin)).size();
                } else if (e.kind == txt::diff_kind::insert) {
                    changed += lines(b.substr(e.new_begin, e.new_end - e.new_begin)).size();
                }
                // runs alternate: no two edits of the same kind in a row
            }
            for (size_t k = 1; k < edits.size(); ++k) {
                EXPECT_FALSE(edits[k].kind == edits[k - 1].kind);
            }
            auto la = lines(a), lb = lines(b);
            if (alg == txt::diff_algorithm::myers) {
                EXPECT_EQ(changed, la.size() + lb.size() - 2 * lcs(la, lb)) << a << "\n--\n" << b;
            }
            // the unified diff of the two applied to a gives b
            auto patch = txt::unified_diff(string(a), string(b), {.algorithm = alg});
            if (a == b) {
                EXPECT_TRUE(patch.empty());
                continue;
            }
            auto back = txt::apply_patch(string(a), patch, {.fuzz = 0});
            ASSERT_TRUE(back.has_value()) << str(patch);
            EXPECT_EQ(str(*back), b) << str(patch);
            auto undo = txt::apply_patch(string(b), patch, {.fuzz = 0, .reverse = true});
            ASSERT_TRUE(undo.has_value()) << str(patch);
            EXPECT_EQ(str(*undo), a) << str(patch);
        }
    }
}

TEST(Diff_Tests, Lines) {
    auto e = txt::diff_lines(s("a\nb\nc\n"), s("a\nx\nc\n"));
    ASSERT_EQ(e.size(), 4u);
    EXPECT_EQ(e[0].kind, txt::diff_kind::equal);
    EXPECT_EQ(e[0].old_end, 2u);
    EXPECT_EQ(e[1].kind, txt::diff_kind::remove);
    EXPECT_EQ(e[1].old_begin, 2u);
    EXPECT_EQ(e[1].old_end, 4u);
    EXPECT_EQ(e[2].kind, txt::diff_kind::insert);
    EXPECT_EQ(e[2].new_begin, 2u);
    EXPECT_EQ(e[2].new_end, 4u);
    EXPECT_EQ(e[3].kind, txt::diff_kind::equal);
    // the same texts: one equal run; empty texts: no run
    e = txt::diff_lines(s("a\nb\n"), s("a\nb\n"));
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].kind, txt::diff_kind::equal);
    EXPECT_EQ(e[0].new_end, 4u);
    EXPECT_TRUE(txt::diff_lines(s(""), s("")).empty());
    e = txt::diff_lines(s(""), s("a\nb"));
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].kind, txt::diff_kind::insert);
    EXPECT_EQ(e[0].new_end, 3u);
    e = txt::diff_lines(s("a\nb"), s(""));
    ASSERT_EQ(e.size(), 1u);
    EXPECT_EQ(e[0].kind, txt::diff_kind::remove);
    // a last line without its line feed is another line than with it
    e = txt::diff_lines(s("a\nb"), s("a\nb\n"));
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[1].kind, txt::diff_kind::remove);
    EXPECT_EQ(e[2].kind, txt::diff_kind::insert);
    // an insertion among equal lines slides down to the canonical place
    e = txt::diff_lines(s("a\nb\n"), s("a\nb\nb\n"));
    ASSERT_EQ(e.size(), 2u);
    EXPECT_EQ(e[1].kind, txt::diff_kind::insert);
    EXPECT_EQ(e[1].new_begin, 4u);
    // CR LF lines compare whole
    e = txt::diff_lines(s("a\r\nb\r\n"), s("a\r\nc\r\n"));
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[0].old_end, 3u);
}

TEST(Diff_Tests, WordsAndChars) {
    string a = s("the quick brown fox"), b = s("the slow brown  fox!");
    auto e = txt::diff_words(a, b);
    bool ok;
    EXPECT_EQ(rebuild(str(a), str(b), e, ok), str(b));
    EXPECT_TRUE(ok);
    std::string removed, inserted;
    for (const auto& x : e) {
        if (x.kind == txt::diff_kind::remove) {
            removed += str(a).substr(x.old_begin, x.old_end - x.old_begin) + "|";
        } else if (x.kind == txt::diff_kind::insert) {
            inserted += str(b).substr(x.new_begin, x.new_end - x.new_begin) + "|";
        }
    }
    EXPECT_EQ(removed, "quick| |");
    EXPECT_EQ(inserted, "slow|  |!|");
    // words of letters beyond ASCII stay whole; code points never split
    e = txt::diff_words(s("zażółć gęślą"), s("zażółć jaźń"));
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[1].old_begin, 11u);
    EXPECT_EQ(e[1].old_end, 19u);
    e = txt::diff_chars(s("kąt"), s("kot"));
    ASSERT_EQ(e.size(), 4u);
    EXPECT_EQ(e[1].kind, txt::diff_kind::remove);
    EXPECT_EQ(e[1].old_end - e[1].old_begin, 2u);
    EXPECT_EQ(e[2].new_end - e[2].new_begin, 1u);
    // invalid UTF-8 still covered byte for byte
    std::string bad = "a\xC3\xFF\xE2\x82";
    e = txt::diff_chars(string(bad), s("a"));
    EXPECT_EQ(rebuild(bad, "a", e, ok), "a");
    EXPECT_TRUE(ok);
    e = txt::diff_words(string(bad), s("b"));
    EXPECT_EQ(rebuild(bad, "b", e, ok), "b");
    EXPECT_TRUE(ok);
}

TEST(Diff_Tests, Patience) {
    // patience keeps the unique lines (the functions) aligned where Myers
    // matches the braces
    std::string a = "int f()\n{\n    return 1;\n}\n\nint g()\n{\n    return 2;\n}\n";
    std::string b = "int g()\n{\n    return 2;\n}\n\nint f()\n{\n    return 1;\n}\n\nint h()\n{\n    return 3;\n}\n";
    for (auto alg : {txt::diff_algorithm::myers, txt::diff_algorithm::patience}) {
        auto e = txt::diff_lines(string(a), string(b), {.algorithm = alg});
        bool ok;
        EXPECT_EQ(rebuild(a, b, e, ok), b);
        EXPECT_TRUE(ok);
    }
    // no unique line: Myers' answer
    auto p = txt::diff_lines(s("x\nx\ny\ny\n"), s("y\ny\nx\nx\n"), {.algorithm = txt::diff_algorithm::patience});
    auto m = txt::diff_lines(s("x\nx\ny\ny\n"), s("y\ny\nx\nx\n"));
    ASSERT_EQ(p.size(), m.size());
    for (size_t k = 0; k < p.size(); ++k) {
        EXPECT_EQ(p[k].kind, m[k].kind);
        EXPECT_EQ(p[k].old_begin, m[k].old_begin);
        EXPECT_EQ(p[k].new_begin, m[k].new_begin);
    }
    // patience aligns the unique lines: "f" moved down shows as one block
    auto e = txt::diff_lines(s("f\na\nb\nc\n"), s("a\nb\nc\nf\n"), {.algorithm = txt::diff_algorithm::patience});
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[0].kind, txt::diff_kind::remove);
    EXPECT_EQ(e[2].kind, txt::diff_kind::insert);
}

TEST(Diff_Tests, IgnoreWhitespace) {
    auto e = txt::diff_lines(s("a b\n  c\nd\n"), s("a  b\nc\t\nD\n"), {.ignore_whitespace = true});
    ASSERT_EQ(e.size(), 3u);
    EXPECT_EQ(e[0].kind, txt::diff_kind::equal);
    EXPECT_EQ(e[0].old_end, 8u);     // "a b\n  c\n"
    EXPECT_EQ(e[0].new_end, 8u);     // "a  b\nc\t\n"
    EXPECT_EQ(e[1].kind, txt::diff_kind::remove);
    // words: only white space between them differs
    auto w = txt::diff_words(s("a b"), s("a   b"), {.ignore_whitespace = true});
    ASSERT_EQ(w.size(), 1u);
    EXPECT_EQ(w[0].kind, txt::diff_kind::equal);
    // a unified diff that ignores white space is empty for such texts
    EXPECT_TRUE(txt::unified_diff(s("a b\n"), s("a  b \n"), {.ignore_whitespace = true}).empty());
}

TEST(Diff_Tests, UnifiedFormat) {
    EXPECT_EQ(str(txt::unified_diff(s("a\nb\nc\nd\ne\nf\ng\nh\n"), s("a\nb\nc\nD\ne\nf\ng\nh\n"))),
              "--- a\n+++ b\n@@ -1,7 +1,7 @@\n a\n b\n c\n-d\n+D\n e\n f\n g\n");
    // names; no context: ranges of no lines name the line before
    EXPECT_EQ(str(txt::unified_diff(s("a\nb\nc\n"), s("a\nc\n"), {.context = 0, .old_name = s("x/f"), .new_name = s("y/f")})),
              "--- x/f\n+++ y/f\n@@ -2 +1,0 @@\n-b\n");
    EXPECT_EQ(str(txt::unified_diff(s("a\nc\n"), s("a\nb\nc\n"), {.context = 0})), "--- a\n+++ b\n@@ -1,0 +2 @@\n+b\n");
    // from and to an empty text
    EXPECT_EQ(str(txt::unified_diff(s(""), s("a\n"))), "--- a\n+++ b\n@@ -0,0 +1 @@\n+a\n");
    EXPECT_EQ(str(txt::unified_diff(s("a\nb\n"), s(""))), "--- a\n+++ b\n@@ -1,2 +0,0 @@\n-a\n-b\n");
    // a last line without its line feed
    EXPECT_EQ(str(txt::unified_diff(s("a\nb"), s("a\nb\n"))),
              "--- a\n+++ b\n@@ -1,2 +1,2 @@\n a\n-b\n\\ No newline at end of file\n+b\n");
    EXPECT_EQ(str(txt::unified_diff(s("a"), s("b"))),
              "--- a\n+++ b\n@@ -1 +1 @@\n-a\n\\ No newline at end of file\n+b\n\\ No newline at end of file\n");
    // changes closer than twice the context share a hunk; farther apart, two
    std::string base;
    for (int i = 0; i < 20; ++i) {
        base += std::to_string(i) + "\n";
    }
    std::string near = base, far = base;
    near.replace(near.find("5\n"), 2, "five\n");
    near.replace(near.find("11\n"), 3, "eleven\n");
    far.replace(far.find("2\n"), 2, "two\n");
    far.replace(far.find("15\n"), 3, "fifteen\n");
    std::string p1 = str(txt::unified_diff(string(base), string(near)));
    std::string p2 = str(txt::unified_diff(string(base), string(far)));
    auto count = [](const std::string& p) {
        size_t n = 0;
        for (size_t at = p.find("@@ -"); at != std::string::npos; at = p.find("@@ -", at + 1)) {
            ++n;
        }
        return n;
    };
    EXPECT_EQ(count(p1), 1u);
    EXPECT_EQ(count(p2), 2u);
    EXPECT_NE(p2.find("@@ -13,7 +13,7 @@"), std::string::npos);
    // equal texts
    EXPECT_TRUE(txt::unified_diff(s("a\n"), s("a\n")).empty());
    EXPECT_TRUE(txt::unified_diff(s(""), s("")).empty());
}

TEST(Diff_Tests, ApplyPatch) {
    string text = s("one\ntwo\nthree\nfour\nfive\nsix\nseven\n");
    string patch = s("--- a\n+++ b\n@@ -2,3 +2,3 @@\n two\n-three\n+THREE\n four\n");
    EXPECT_EQ(str(*txt::apply_patch(text, patch)), "one\ntwo\nTHREE\nfour\nfive\nsix\nseven\n");
    // at an offset
    EXPECT_EQ(str(*txt::apply_patch(s("zero\n") + text, patch)), "zero\none\ntwo\nTHREE\nfour\nfive\nsix\nseven\n");
    // a context line changed: fuzz 1 applies it, fuzz 0 does not
    string changed = s("one\nTWO\nthree\nfour\nfive\nsix\nseven\n");
    EXPECT_EQ(str(*txt::apply_patch(changed, patch)), "one\nTWO\nTHREE\nfour\nfive\nsix\nseven\n");
    auto r = txt::apply_patch(changed, patch, {.fuzz = 0});
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().hunk(), 1u);
    EXPECT_EQ(r.error().line(), 3u);
    // reversed
    EXPECT_EQ(str(*txt::apply_patch(s("one\ntwo\nTHREE\nfour\n"), patch, {.reverse = true})), "one\ntwo\nthree\nfour\n");
    // without the file lines, and two hunks, the second shifted by the first
    string two = s("@@ -1,2 +1,3 @@\n one\n+one and a half\n two\n@@ -6,2 +7,2 @@\n six\n-seven\n+SEVEN\n");
    EXPECT_EQ(str(*txt::apply_patch(text, two)), "one\none and a half\ntwo\nthree\nfour\nfive\nsix\nSEVEN\n");
    // a hunk at the start stays there: context at its end only
    r = txt::apply_patch(s("zero\none\ntwo\n"), s("@@ -1,2 +1,2 @@\n-one\n+ONE\n two\n"));
    EXPECT_FALSE(r.has_value());
    EXPECT_EQ(str(*txt::apply_patch(s("one\ntwo\n"), s("@@ -1,2 +1,2 @@\n-one\n+ONE\n two\n"))), "ONE\ntwo\n");
    // ... and one at the end
    EXPECT_FALSE(txt::apply_patch(s("one\ntwo\nthree\n"), s("@@ -1,2 +1,2 @@\n one\n-two\n+TWO\n")).has_value());
    EXPECT_EQ(str(*txt::apply_patch(s("zero\none\ntwo\n"), s("@@ -1,2 +1,2 @@\n one\n-two\n+TWO\n"))), "zero\none\nTWO\n");
    // fuzz keeps the text's context lines
    EXPECT_EQ(str(*txt::apply_patch(s("a\nB\nc\nd\nE\n"), s("@@ -1,5 +1,5 @@\n a\n b\n-c\n+C\n d\n e\n"), {.fuzz = 2})),
              "a\nB\nC\nd\nE\n");
    // no newline at the end, both ways
    EXPECT_EQ(str(*txt::apply_patch(s("a\nb"), s("@@ -1,2 +1,2 @@\n a\n-b\n\\ No newline at end of file\n+b\n"))), "a\nb\n");
    EXPECT_EQ(str(*txt::apply_patch(s("a\nb\n"), s("@@ -1,2 +1,2 @@\n a\n-b\n\\ No newline at end of file\n+b\n"),
                                    {.reverse = true})),
              "a\nb");
    // only the first file of a multi-file patch
    string files = s("diff --git a/x b/x\n--- a/x\n+++ b/x\n@@ -1 +1 @@\n-x\n+X\ndiff --git a/y b/y\n--- a/y\n+++ b/y\n"
                     "@@ -1 +1 @@\n-y\n+Y\n");
    EXPECT_EQ(str(*txt::apply_patch(s("x\n"), files)), "X\n");
    // to and from the empty text
    EXPECT_EQ(str(*txt::apply_patch(s(""), s("@@ -0,0 +1,2 @@\n+a\n+b\n"))), "a\nb\n");
    EXPECT_EQ(str(*txt::apply_patch(s("a\nb\n"), s("@@ -1,2 +0,0 @@\n-a\n-b\n"))), "");
}

TEST(Diff_Tests, PatchErrors) {
    auto fails = [](const char* text, const char* patch, size_t hunk, size_t line) {
        auto r = txt::apply_patch(s(text), s(patch));
        ASSERT_FALSE(r.has_value()) << patch;
        EXPECT_EQ(r.error().hunk(), hunk) << patch;
        EXPECT_EQ(r.error().line(), line) << patch;
        EXPECT_FALSE(r.error().message().empty());
    };
    fails("a\n", "", 0, 0);                                           // no hunk
    fails("a\n", "just text\n", 0, 0);
    fails("a\n", "--- a\n+++ b\n", 0, 0);
    fails("a\n", "@@ -1 +1\n-a\n+b\n", 1, 1);                          // a broken header
    fails("a\n", "@@ -x +1 @@\n-a\n+b\n", 1, 1);
    fails("a\n", "@@ -1,99999999999999999999999 +1 @@\n-a\n", 1, 1);   // a count past size_t
    fails("a\n", "@@ -1,2 +1,2 @@\n-a\n+b\n", 1, 1);                  // shorter than its header
    fails("a\n", "@@ -1 +1 @@\n*a\n+b\n", 1, 2);                       // a line of no mark
    fails("a\nb\n", "@@ -1 +1 @@\n-a\n+A\n@@ -2 +2 @@\n-c\n+C\n", 2, 4);   // the second hunk not in the text
}

TEST(Diff_Tests, Merge) {
    string base = s("a\nb\nc\nd\ne\n");
    // changes of different lines: taken both
    auto m = txt::merge3(base, s("A\nb\nc\nd\ne\n"), s("a\nb\nc\nd\nE\n"));
    EXPECT_EQ(m.conflicts, 0u);
    EXPECT_EQ(str(m.text), "A\nb\nc\nd\nE\n");
    // the same change on both: once
    m = txt::merge3(base, s("a\nB\nc\nd\ne\n"), s("a\nB\nc\nd\ne\n"));
    EXPECT_EQ(m.conflicts, 0u);
    EXPECT_EQ(str(m.text), "a\nB\nc\nd\ne\n");
    // one side unchanged: the other
    m = txt::merge3(base, base, s("x\n"));
    EXPECT_EQ(m.conflicts, 0u);
    EXPECT_EQ(str(m.text), "x\n");
    // different changes of one line: a conflict, with the labels
    m = txt::merge3(base, s("a\nb\nmine\nd\ne\n"), s("a\nb\nyours\nd\ne\n"),
                    {.ours_label = s("HEAD"), .theirs_label = s("feature")});
    EXPECT_EQ(m.conflicts, 1u);
    EXPECT_EQ(str(m.text), "a\nb\n<<<<<<< HEAD\nmine\n=======\nyours\n>>>>>>> feature\nd\ne\n");
    m = txt::merge3(base, s("a\nb\nmine\nd\ne\n"), s("a\nb\nyours\nd\ne\n"), {.diff3 = true});
    EXPECT_EQ(str(m.text), "a\nb\n<<<<<<< ours\nmine\n||||||| base\nc\n=======\nyours\n>>>>>>> theirs\nd\ne\n");
    // changes of touching lines conflict too (git's rule)
    m = txt::merge3(base, s("a\nB\nc\nd\ne\n"), s("a\nb\nC\nd\ne\n"));
    EXPECT_EQ(m.conflicts, 1u);
    // a conflict trimmed to the lines that differ (git's zealous merge), not with diff3
    m = txt::merge3(base, s("a\nX\nc\nY\ne\n"), s("a\nX\nc\nZ\ne\n"));
    EXPECT_EQ(m.conflicts, 1u);
    EXPECT_EQ(str(m.text), "a\nX\nc\n<<<<<<< ours\nY\n=======\nZ\n>>>>>>> theirs\ne\n");
    // a side ending without its line feed: the markers stay on their lines
    m = txt::merge3(s("a\n"), s("b"), s("c"));
    EXPECT_EQ(m.conflicts, 1u);
    EXPECT_EQ(str(m.text), "<<<<<<< ours\nb\n=======\nc\n>>>>>>> theirs\n");
    // empty texts
    m = txt::merge3(s(""), s(""), s(""));
    EXPECT_EQ(m.conflicts, 0u);
    EXPECT_TRUE(m.text.empty());
    m = txt::merge3(s(""), s("a\n"), s(""));
    EXPECT_EQ(str(m.text), "a\n");
    m = txt::merge3(s(""), s("a\n"), s("b\n"));
    EXPECT_EQ(m.conflicts, 1u);
    // two conflicts
    m = txt::merge3(s("1\n2\n3\n4\n5\n6\n7\n8\n"), s("x\n2\n3\n4\n5\n6\n7\ny\n"), s("X\n2\n3\n4\n5\n6\n7\nY\n"));
    EXPECT_EQ(m.conflicts, 2u);
}

TEST(Diff_Tests, Large) {
    // 20,000 lines, 40 scattered changes: fast, and the round trip holds
    std::mt19937 g(7);
    std::string a, b;
    for (int i = 0; i < 20000; ++i) {
        std::string l = "line " + std::to_string(g() % 5000) + "\n";
        a += l;
        b += i % 500 == 17 ? "changed " + l : l;
    }
    auto patch = txt::unified_diff(string(a), string(b));
    auto r = txt::apply_patch(string(a), patch);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(str(*r), b);
    auto e = txt::diff_lines(string(a), string(b), {.algorithm = txt::diff_algorithm::patience});
    bool ok;
    EXPECT_EQ(rebuild(a, b, e, ok), b);
    EXPECT_TRUE(ok);
    auto m = txt::merge3(string(a), string(b), string(a));
    EXPECT_EQ(m.conflicts, 0u);
    EXPECT_EQ(str(m.text), b);
}
