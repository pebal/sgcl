//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// txt::catalog against GNU gettext 1.0: the catalogs of
// tests/txt/catalog_vectors.h (tools/gettext_vectors.py) read from their .po
// and from msgfmt's .mo, answering libintl's answers, to_mo writing msgfmt's
// bytes; libintl's form for random plural rules; then the boundaries.
#include "tests/types.h"
#include "tests/txt/catalog_vectors.h"

#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
    string s(const char* t) {
        return string(t);
    }

    slice<const byte> bytes_of(const char* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    slice<const byte> bytes_of(const std::string& b) {
        return bytes_of(b.data(), b.size());
    }

    txt::catalog po(const char* text) {
        auto c = txt::catalog::parse_po(string(text));
        EXPECT_TRUE(c.has_value()) << (c ? "" : c.error().message().c_str());
        return c ? *c : txt::catalog();
    }

    // A .mo of revision 0 (little-endian, or big-endian when swap) holding
    // the given keys and translations, in the order given, no hash table
    std::string mo_of(const std::vector<std::string>& keys, const std::vector<std::string>& values, bool big = false) {
        auto put = [&](std::string& out, uint32_t v) {
            for (int k = 0; k < 4; ++k) {
                out.push_back(char(big ? v >> (8 * (3 - k)) : v >> (8 * k)));
            }
        };
        uint32_t n = uint32_t(keys.size());
        std::string head, tables, blob;
        put(head, 0x950412deu);
        put(head, 0);
        put(head, n);
        put(head, 28);
        put(head, 28 + 8 * n);
        put(head, 0);
        put(head, 28 + 16 * n);
        uint32_t at = 28 + 16 * n;
        std::string otab, ttab;
        for (auto& k : keys) {
            put(otab, uint32_t(k.size()));
            put(otab, at + uint32_t(blob.size()));
            blob += k;
            blob.push_back('\0');
        }
        for (auto& v : values) {
            put(ttab, uint32_t(v.size()));
            put(ttab, at + uint32_t(blob.size()));
            blob += v;
            blob.push_back('\0');
        }
        return head + otab + ttab + blob;
    }

    std::string answer(const txt::catalog& c, const CatalogQuery& q) {
        string r;
        switch (q.kind) {
            case 'g': r = c.translate(s(q.id)); break;
            case 'p': r = c.translate(s(q.id), s(q.context)); break;
            case 'n': r = c.translate(s(q.id), s(q.plural), q.n); break;
            default: r = c.translate(s(q.id), s(q.plural), q.n, s(q.context)); break;
        }
        return std::string(r.view());
    }
}

TEST(Catalog_Tests, LibintlVectors) {
    vector<txt::catalog> from_po, from_mo;   // handles in managed memory, where the collector looks
    size_t bad = 0;
    for (size_t i = 0; i < std::size(CatalogCases); ++i) {
        const auto& k = CatalogCases[i];
        auto p = txt::catalog::parse_po(s(k.po));
        auto m = txt::catalog::parse_mo(bytes_of(k.mo, k.mo_size));
        ASSERT_TRUE(p.has_value()) << i << ": " << p.error().line() << ":" << p.error().column() << " "
                                   << p.error().message();
        ASSERT_TRUE(m.has_value()) << i << ": " << m.error().message();
        auto written = p->to_mo();
        EXPECT_TRUE(written.size() == k.mo_size && std::memcmp(written.data(), k.mo, k.mo_size) == 0)
            << i << ": to_mo is not msgfmt's .mo";
        EXPECT_EQ(p->size(), m->size()) << i;
        from_po.push_back(*p);
        from_mo.push_back(*m);
    }
    for (const auto& q : CatalogQueries) {
        std::string a1 = answer(from_po[q.c], q), a2 = answer(from_mo[q.c], q);
        if ((a1 != q.answer || a2 != q.answer) && ++bad <= 20) {
            ADD_FAILURE() << "case " << q.c << " " << q.kind << " ctx=" << q.context << " id=" << q.id << " n=" << q.n
                          << "\n  po " << a1 << "\n  mo " << a2 << "\n  want " << q.answer;
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(CatalogQueries);
}

TEST(Catalog_Tests, LibintlPluralRules) {
    std::string all;
    for (int k = 0; k < 10; ++k) {
        if (k) {
            all.push_back('\0');
        }
        all += "f" + std::to_string(k);
    }
    size_t bad = 0;
    vector<txt::catalog> cats;
    for (const char* h : PluralRuleHeaders) {
        std::string file = mo_of({std::string(), std::string("x\0xs", 4)}, {h, all});
        auto c = txt::catalog::parse_mo(bytes_of(file));
        ASSERT_TRUE(c.has_value());
        cats.push_back(*c);
    }
    for (const auto& r : PluralRuleCases) {
        std::string got(cats[r.rule].translate(s("x"), s("xs"), r.n).view());
        std::string want = r.form >= 0 ? "f" + std::to_string(r.form) : "x";
        if (got != want && ++bad <= 20) {
            ADD_FAILURE() << PluralRuleHeaders[r.rule] << " n=" << r.n << " got " << got << " want " << want;
        }
    }
    EXPECT_EQ(bad, 0u) << "of " << std::size(PluralRuleCases);
}

TEST(Catalog_Tests, Lookups) {
    auto c = po(R"(# a comment
msgid ""
msgstr ""
"Language: pl\n"
"Content-Type: text/plain; charset=UTF-8\n"
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);\n"

msgctxt "menu"
msgid "Open"
msgstr "Otwórz"

msgid "Open"
msgstr "Otwieranie"

msgctxt ""
msgid "Open"
msgstr "Pusty kontekst"

msgid "file"
msgid_plural "files"
msgstr[0] "plik"
msgstr[1] "pliki"
msgstr[2] "plików"

msgid "two"
msgid_plural "twos"
msgstr[0] "dwa"
msgstr[1] "dwie"

#, fuzzy
msgid "Fuzzy"
msgstr "Rozmyte"

msgid "Untranslated"
msgstr ""

#~ msgid "Gone"
#~ msgstr "Nie ma"
)");
    EXPECT_EQ(c.translate(s("Open")), s("Otwieranie"));
    EXPECT_EQ(c.translate(s("Open"), s("menu")), s("Otwórz"));
    EXPECT_EQ(c.translate(s("Open"), s("")), s("Pusty kontekst"));   // an empty context is a context
    EXPECT_EQ(c.translate(s("Open"), s("toolbar")), s("Open"));
    EXPECT_EQ(c.translate(s("file"), s("files"), 1), s("plik"));
    EXPECT_EQ(c.translate(s("file"), s("files"), 22), s("pliki"));
    EXPECT_EQ(c.translate(s("file"), s("files"), 25), s("plików"));
    EXPECT_EQ(c.translate(s("file")), s("plik"));                    // gettext of a plural: the first form
    EXPECT_EQ(c.translate(s("Open"), s("Opens"), 5), s("Otwieranie"));   // one form, n's is 2: the first
    EXPECT_EQ(c.translate(s("two"), s("twos"), 5), s("dwa"));            // form 2 missing: the first
    EXPECT_EQ(c.translate(s("two"), s("twos"), 5, s("ctx")), s("twos"));   // not found
    EXPECT_EQ(c.translate(s("Fuzzy")), s("Fuzzy"));
    EXPECT_EQ(c.translate(s("Untranslated")), s("Untranslated"));
    EXPECT_EQ(c.translate(s("Gone")), s("Gone"));
    EXPECT_EQ(c.translate(s("missing"), s("missings"), 1), s("missing"));
    EXPECT_EQ(c.translate(s("missing"), s("missings"), 0), s("missings"));
    EXPECT_TRUE(c.contains(s("Open")));
    EXPECT_TRUE(c.contains(s("Open"), s("menu")));
    EXPECT_FALSE(c.contains(s("Open"), s("toolbar")));
    EXPECT_FALSE(c.contains(s("Fuzzy")));
    EXPECT_EQ(c.size(), 5u);
    EXPECT_EQ(c.header(s("language")), s("pl"));
    EXPECT_EQ(c.header(s("Content-Type")), s("text/plain; charset=UTF-8"));
    EXPECT_EQ(c.header(s("Missing")), s(""));
    EXPECT_EQ(c.plural_forms(), 3u);
    EXPECT_EQ(c.plural_index(1), 0u);
    EXPECT_EQ(c.plural_index(4), 1u);
    EXPECT_EQ(c.plural_index(112), 2u);
    // a translation comes back as the catalog's own string: no copy
    EXPECT_EQ(c.translate(s("Open")).object(), c.translate(s("Open")).object());
    // the round trip through .mo
    auto mo = c.to_mo();
    auto back = txt::catalog::parse_mo(slice<const byte>(mo.data(), mo.size()));
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->translate(s("Open"), s("")), s("Pusty kontekst"));
    EXPECT_EQ(back->translate(s("file"), s("files"), 3), s("pliki"));
    EXPECT_EQ(back->size(), 5u);
}

TEST(Catalog_Tests, Empty) {
    txt::catalog c;
    EXPECT_EQ(c.translate(s("a")), s("a"));
    EXPECT_EQ(c.translate(s("a"), s("ctx")), s("a"));
    EXPECT_EQ(c.translate(s("a"), s("as"), 1), s("a"));
    EXPECT_EQ(c.translate(s("a"), s("as"), 2), s("as"));
    EXPECT_EQ(c.translate(s(""), s(""), 2), s(""));
    EXPECT_FALSE(c.contains(s("a")));
    EXPECT_EQ(c.size(), 0u);
    EXPECT_TRUE(c.empty());
    EXPECT_EQ(c.header(s("Language")), s(""));
    EXPECT_EQ(c.plural_forms(), 2u);
    EXPECT_EQ(c.plural_index(0), 1u);
    EXPECT_EQ(c.plural_index(1), 0u);
    auto mo = c.to_mo();
    EXPECT_EQ(mo.size(), 28u);
    auto back = txt::catalog::parse_mo(slice<const byte>(mo.data(), mo.size()));
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->empty());
    // an empty .po, and one of comments only
    EXPECT_TRUE(txt::catalog::parse_po(s(""))->empty());
    EXPECT_TRUE(txt::catalog::parse_po(s("# nothing\n\n#, fuzzy\n"))->empty());
    // a header only: no messages, the header read
    auto h = po("msgid \"\"\nmsgstr \"Language: de\\n\"\n");
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(h.header(s("Language")), s("de"));
    EXPECT_EQ(h.translate(s("")), s("Language: de\n"));   // gettext("") is the header
}

TEST(Catalog_Tests, PoSyntax) {
    // a BOM, CRLF, strings across lines, every escape, a translator's previous id
    auto c = po("\xEF\xBB\xBFmsgid \"a\"\r\n"
                "msgstr \"\"\r\n"
                "\"x\\n\\t\\\"\\\\\\a\\b\\f\\v\\r\\?\\'\"\r\n"
                "\"\\101\\x42\\303\\263\"\r\n"
                "\r\n"
                "#| msgid \"old\"\r\n"
                "msgid \"b\"   \t\r\n"
                "msgstr \"B\"");
    EXPECT_EQ(c.translate(s("a")), s("x\n\t\"\\\a\b\f\v\r?'AB\xC3\xB3"));
    EXPECT_EQ(c.translate(s("b")), s("B"));
    // the hex escape takes every hex digit after it, as gettext's does
    EXPECT_EQ(po("msgid \"c\"\nmsgstr \"\\x4142\"\n").translate(s("c")), s("\x42"));
    // the fuzzy flag among others
    EXPECT_FALSE(po("#, c-format, fuzzy ,no-wrap\nmsgid \"d\"\nmsgstr \"D\"\n").contains(s("d")));
    EXPECT_TRUE(po("#, c-format, fuzzyish\nmsgid \"d\"\nmsgstr \"D\"\n").contains(s("d")));
    // the header is read even when fuzzy
    EXPECT_EQ(po("#, fuzzy\nmsgid \"\"\nmsgstr \"Plural-Forms: nplurals=1; plural=0;\\n\"\n").plural_forms(), 1u);
}

TEST(Catalog_Tests, PoErrors) {
    struct Bad {
        const char* text;
        size_t line, column;
    };
    const Bad cases[] = {
        {"msgid \"a\nmsgstr \"b\"\n", 1, 7},                       // a string not closed
        {"msgid \"a\\q\"\nmsgstr \"b\"\n", 1, 9},                  // an escape C does not have
        {"msgid \"a\\0\"\nmsgstr \"b\"\n", 1, 9},                  // a NUL
        {"msgid \"a\\x\"\nmsgstr \"b\"\n", 1, 9},                  // \x without digits
        {"msgstr \"b\"\n", 1, 1},                                   // msgstr without msgid
        {"msgid \"a\"\n", 1, 1},                                    // msgid without msgstr
        {"msgid \"a\"\nmsgid \"b\"\nmsgstr \"c\"\n", 2, 1},        // msgid twice
        {"msgid \"a\"\nmsgstr \"b\"\nmsgstr \"c\"\n", 3, 1},       // msgstr twice
        {"msgid \"a\"\nmsgstr[0] \"b\"\n", 2, 1},                   // msgstr[] without msgid_plural
        {"msgid \"a\"\nmsgid_plural \"as\"\nmsgstr \"b\"\n", 3, 1},  // msgstr where msgstr[0] is wanted
        {"msgid \"a\"\nmsgid_plural \"as\"\nmsgstr[1] \"b\"\n", 3, 1},   // out of order
        {"msgid \"a\"\nmsgid_plural \"as\"\nmsgstr[x] \"b\"\n", 3, 8},   // not an index
        {"msgid \"a\"\nmsgid_plural \"as\"\n", 1, 1},               // no forms
        {"msgid \"a\"\nmsgstr \"b\"\n\nmsgid \"a\"\nmsgstr \"c\"\n", 4, 1},   // defined twice: the second
        {"msgid \"a\"\nmsgstr \"\"\n\nmsgid \"a\"\nmsgstr \"b\"\n", 4, 1},    // twice, once untranslated
        {"msgid \"\"\nmsgstr \"x\"\n\nmsgid \"\"\nmsgstr \"y\"\n", 4, 1},     // the header twice
        {"\"a\"\n", 1, 1},                                          // a string without a keyword
        {"msgid\nmsgstr \"b\"\n", 1, 6},                            // a keyword without its string
        {"msgfoo \"a\"\n", 1, 1},                                   // not a keyword
        {"msgid \"a\"\nmsgctxt \"c\"\nmsgstr \"b\"\n", 2, 1},      // msgctxt after msgid
        {"msgid \"\\na\"\nmsgstr \"b\"\n", 1, 1},                  // line feeds at the start
        {"msgid \"a\\n\"\nmsgid_plural \"as\"\nmsgstr[0] \"b\\n\"\n", 1, 1},   // and at the end
        {"msgid \"\"\nmsgstr \"Plural-Forms: nplurals=2; plural=n+;\\n\"\n", 1, 1},   // not a rule
        {"msgid \"\"\nmsgstr \"Plural-Forms: nplurals=x; plural=n;\\n\"\n", 1, 1},
        {"msgid \"a\\004b\"\nmsgstr \"c\"\n", 1, 1},          // a byte 4, the .mo's context separator
    };
    for (const auto& b : cases) {
        auto c = txt::catalog::parse_po(s(b.text));
        ASSERT_FALSE(c.has_value()) << b.text;
        EXPECT_EQ(c.error().line(), b.line) << b.text << " " << c.error().message();
        EXPECT_EQ(c.error().column(), b.column) << b.text << " " << c.error().message();
        EXPECT_FALSE(c.error().message().empty());
    }
    // a translation's line feeds that agree with its id's
    EXPECT_TRUE(txt::catalog::parse_po(s("msgid \"\\na\\n\"\nmsgstr \"\\nb\\n\"\n")).has_value());
    // an untranslated or fuzzy entry is not checked, as msgfmt does not
    EXPECT_TRUE(txt::catalog::parse_po(s("msgid \"\\na\"\nmsgstr \"\"\n")).has_value());
    EXPECT_TRUE(txt::catalog::parse_po(s("#, fuzzy\nmsgid \"\\na\"\nmsgstr \"b\"\n")).has_value());
}

TEST(Catalog_Tests, MoFiles) {
    std::vector<std::string> keys = {std::string(), std::string("c\x04" "a"), std::string("x\0xs", 4)};
    std::vector<std::string> values = {"Plural-Forms: nplurals=2; plural=n>1;\n", "A", std::string("X\0XS", 4)};
    for (bool big : {false, true}) {
        std::string file = mo_of(keys, values, big);
        auto c = txt::catalog::parse_mo(bytes_of(file));
        ASSERT_TRUE(c.has_value()) << big;
        EXPECT_EQ(c->translate(s("a"), s("c")), s("A"));
        EXPECT_EQ(c->translate(s("a")), s("a"));
        EXPECT_EQ(c->translate(s("x"), s("xs"), 1), s("X"));
        EXPECT_EQ(c->translate(s("x"), s("xs"), 0), s("X"));    // n > 1: 0 is form 0
        EXPECT_EQ(c->translate(s("x"), s("xs"), 2), s("XS"));
        EXPECT_EQ(c->size(), 2u);
    }
    std::string good = mo_of(keys, values);
    // every truncation is refused, never read past
    for (size_t n = 0; n < good.size(); ++n) {
        auto c = txt::catalog::parse_mo(bytes_of(good.data(), n));
        EXPECT_FALSE(c.has_value()) << n;
    }
    std::string magic = good;
    magic[0] = 0;
    EXPECT_FALSE(txt::catalog::parse_mo(bytes_of(magic)).has_value());
    std::string revision = good;
    revision[6] = 2;   // major revision 2
    EXPECT_FALSE(txt::catalog::parse_mo(bytes_of(revision)).has_value());
    std::string minor = good;
    minor[4] = 1;      // a minor revision is read
    EXPECT_TRUE(txt::catalog::parse_mo(bytes_of(minor)).has_value());
    std::string count = good;
    count[11] = 0x10;  // a table past the end
    EXPECT_FALSE(txt::catalog::parse_mo(bytes_of(count)).has_value());
    std::string unterminated = good;
    unterminated.back() = 'x';   // a string without its NUL
    EXPECT_FALSE(txt::catalog::parse_mo(bytes_of(unterminated)).has_value());
    // a rule that is not one: libintl's Germanic rule
    auto g = txt::catalog::parse_mo(bytes_of(mo_of({std::string(), std::string("x\0xs", 4)},
                                                   {"Plural-Forms: nplurals=3; plural=n+;\n",
                                                    std::string("0\0" "1\0" "2", 5)})));
    ASSERT_TRUE(g.has_value());
    EXPECT_EQ(g->plural_forms(), 2u);
    EXPECT_EQ(g->translate(s("x"), s("xs"), 5), s("1"));
    // a key twice: one is kept
    auto twice = txt::catalog::parse_mo(bytes_of(mo_of({"a", "a"}, {"1", "2"})));
    ASSERT_TRUE(twice.has_value());
    EXPECT_EQ(twice->size(), 1u);
}

TEST(Catalog_Tests, PluralRules) {
    auto rule = [](const char* r) {
        std::string h = std::string("Plural-Forms: ") + r + "\n";
        return txt::catalog::parse_mo(bytes_of(mo_of({std::string()}, {h}))).value();
    };
    // a division by zero (a trap in libintl): form 0
    EXPECT_EQ(rule("nplurals=3; plural=2 + n/0;").plural_index(5), 0u);
    EXPECT_EQ(rule("nplurals=3; plural=n%0;").plural_index(5), 0u);
    // && and || and ?: are short: the division is never made
    EXPECT_EQ(rule("nplurals=3; plural=n==5 || 1/0;").plural_index(5), 1u);
    EXPECT_EQ(rule("nplurals=3; plural=n!=5 && 1/0;").plural_index(5), 0u);
    EXPECT_EQ(rule("nplurals=3; plural=n==5 ? 2 : 1/0;").plural_index(5), 2u);
    // unsigned arithmetic, wrapping
    EXPECT_EQ(rule("nplurals=3; plural=0 - 1 > n;").plural_index(18446744073709551615ull), 0u);
    EXPECT_EQ(rule("nplurals=3; plural=n - 2 < 3;").plural_index(1), 0u);
    // past nplurals: form 0
    EXPECT_EQ(rule("nplurals=2; plural=n;").plural_index(7), 0u);
    // precedence and ?: to the right
    EXPECT_EQ(rule("nplurals=9; plural=1 + 2 * 3;").plural_index(0), 7u);
    EXPECT_EQ(rule("nplurals=9; plural=n==1 ? 1 : n==2 ? 2 : 3;").plural_index(2), 2u);
    EXPECT_EQ(rule("nplurals=9; plural=!!n;").plural_index(4), 1u);
    // a constant past 64 bits saturates, as strtoul does
    EXPECT_EQ(rule("nplurals=9; plural=99999999999999999999999 == 18446744073709551615;").plural_index(0), 1u);
    // nesting deeper than the reader allows: the Germanic rule
    std::string deep = "nplurals=3; plural=" + std::string(300, '(') + "n" + std::string(300, ')') + ";";
    EXPECT_EQ(rule(deep.c_str()).plural_forms(), 2u);
    // the expression ends at ';' or a line feed; anything else after it is not a rule
    EXPECT_EQ(rule("nplurals=3; plural=n%3").plural_index(5), 2u);
    EXPECT_EQ(rule("nplurals=3; plural=n%3 x;").plural_forms(), 2u);
}

TEST(Catalog_Tests, SharedAcrossThreads) {
    auto c = po("msgid \"a\"\nmsgstr \"A\"\n\nmsgid \"b\"\nmsgid_plural \"bs\"\nmsgstr[0] \"B\"\nmsgstr[1] \"BS\"\n");
    std::atomic<size_t> wrong{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&c, &wrong] {
            for (int i = 0; i < 2000; ++i) {
                wrong += c.translate(string("a")) != string("A");
                wrong += c.translate(string("b"), string("bs"), uint64_t(i)) != string(i == 1 ? "B" : "BS");
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(wrong.load(), 0u);
}
