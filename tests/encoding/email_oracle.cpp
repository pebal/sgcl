//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::email against two oracles: Python 3's email package
// (email_oracle/oracle.py) and Go's net/mail, mime, mime/multipart and
// mime/quotedprintable (go_mail/main.go). Each reads the messages built
// here and must take out of them what was put in; each builds messages of
// its own, which must read here as it reads them. A summary is the subject,
// the addresses, the date, the Message-ID, the text and HTML bodies and the
// attachments with their sizes and FNV-1a hashes.
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

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    bool have(const char* tool) {
        return std::system((std::string("command -v ") + tool + " > /dev/null 2>&1").c_str()) == 0;
    }

    std::filesystem::path work() {
        auto d = std::filesystem::temp_directory_path() / "sgcl_email_oracle";
        std::filesystem::create_directories(d);
        return d;
    }

    const std::string& go_mail() {
        static std::string path = [] {
            if (!have("go")) {
                return std::string();
            }
            auto out = work() / "go_mail";
            auto cmd = "go build -o '" + out.string() + "' '" + (source_root() / "tests/encoding/go_mail/main.go").string() + "' 2>&1";
            return std::system(cmd.c_str()) == 0 ? out.string() : std::string();
        }();
        return path;
    }

    std::string lf(std::string t) {
        std::string out;
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] == '\r' && i + 1 < t.size() && t[i + 1] == '\n') {
                continue;
            }
            out += t[i];
        }
        return out;
    }

    std::string hash(const sgcl::vector<byte>& v) {
        char b[17];
        std::snprintf(b, sizeof b, "%016llx", (unsigned long long)fnv(std::string_view(reinterpret_cast<const char*>(v.data()), v.size())));
        return b;
    }

    std::string addr_list(const sgcl::vector<email::address>& list) {
        std::string out;
        for (auto& a : list) {
            out += "[" + s(a.name()) + "|" + s(a.addr()) + "]";
        }
        return out;
    }

    std::string addr_list(const json& list) {
        std::string out;
        for (size_t i = 0; i < list.size(); ++i) {
            out += "[" + s(list[i][0].as_string("")) + "|" + s(list[i][1].as_string("")) + "]";
        }
        return out;
    }

    // What the oracle took out of a message, against what this library takes
    void same(const json& theirs, const email& ours, const std::string& what, bool attachments = true) {
        SCOPED_TRACE(what);
        EXPECT_EQ(s(theirs["subject"].as_string("")), s(ours.subject()));
        sgcl::vector<email::address> from;
        if (ours.from()) {
            from.push_back(*ours.from());
        }
        EXPECT_EQ(addr_list(theirs["from"]), addr_list(from));
        EXPECT_EQ(addr_list(theirs["to"]), addr_list(ours.to()));
        EXPECT_EQ(addr_list(theirs["cc"]), addr_list(ours.cc()));
        EXPECT_EQ(theirs["date"].as_int(0), ours.date() ? ours.date()->unix() : 0);
        EXPECT_EQ(s(theirs["message_id"].as_string("")), s(ours.message_id()));
        EXPECT_EQ(lf(s(theirs["text"].as_string(""))), s(ours.text()));
        EXPECT_EQ(lf(s(theirs["html"].as_string(""))), s(ours.html()));
        if (!attachments) {
            return;
        }
        auto atts = ours.attachments();
        const json& t = theirs["attachments"];
        ASSERT_EQ(t.size(), atts.size());
        for (size_t i = 0; i < atts.size(); ++i) {
            EXPECT_EQ(s(t[i][0].as_string("")), s(atts[i].filename()));
            EXPECT_EQ(s(t[i][1].as_string("")), s(atts[i].content_type()));
            if (t[i][2].as_int(0) >= 0) {
                EXPECT_EQ(t[i][2].as_int(0), int64_t(atts[i].content().size()));
                EXPECT_EQ(s(t[i][3].as_string("")), hash(atts[i].content()));
            }
        }
    }

    sgcl::vector<byte> vbytes(std::string_view t) {
        sgcl::vector<byte> v;
        for (char c : t) {
            v.push_back(byte(uint8_t(c)));
        }
        return v;
    }

    // The messages this library builds for the oracles to read
    sgcl::vector<email> ours() {
        sgcl::vector<email> out;
        {
            email m("Alice Example <alice@example.com>", "Bob <bob@example.org>, carol@example.net", "Plain ASCII", "Hello, Bob.\nA second line.\n");
            m.set_date(time::datetime::from_unix(1791203400, time::zone::utc()));
            m.set_message_id("one@example.com");
            out.push_back(m);
        }
        {
            email m("Łucja Żółć <lucja@example.pl>", "\"Doe, John\" <john@example.com>",
                    "Zażółć gęślą jaźń — the encoded subject goes on long enough to be folded twice or more, with words", "Treść: zażółć.\n");
            m.add_cc("Zoë <zoe@example.fr>");
            m.set_html("<p>Treść <b>po polsku</b></p>\n");
            m.set_date(time::datetime::from_unix(1791203400, time::zone::fixed(std::chrono::hours(2))));
            out.push_back(m);
        }
        {
            email m("sender@example.com", "rcpt@example.com", "Attachments", "See the files.\n");
            auto big = input(300000);
            m.attach("data.bin", sgcl::vector<byte>(big.begin(), big.end()));
            m.attach("ąę notatka.txt", vbytes("zażółć\n"));
            m.attach("a very long file name that will need the continuations of RFC 2231 to fit — ąęść.pdf", vbytes("%PDF-1.4 fake"));
            m.attach("plain.csv", vbytes("a,b\r\n1,2\r\n"));
            out.push_back(m);
        }
        {
            email m("a@example.com", "b@example.com", "Long lines", sgcl::string(std::string(1200, 'x') + "\n" + std::string(600, 'y') + " ą\n"));
            out.push_back(m);
        }
        {
            email m("a@example.com", "b@example.com", "Dots and spaces", ".leading dot\n..two\ntrailing space \n\ttab\nFrom the start\n=equals=\n");
            out.push_back(m);
        }
        {
            email inner("inner@example.com", "outer@example.com", "The forwarded one", "Inner body.\n");
            email m("a@example.com", "b@example.com", "Forward", "Forwarding.\n");
            m.attach("fwd.eml", vbytes(s(inner.to_string())), "message/rfc822");
            out.push_back(m);
        }
        return out;
    }
}

TEST(EmailOracle_Tests, PythonReadsWhatWeWrite) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    auto dir = work() / "ours_py";
    std::filesystem::create_directories(dir);
    auto msgs = ours();
    std::string files;
    for (size_t i = 0; i < msgs.size(); ++i) {
        auto p = dir / (std::to_string(i) + ".eml");
        std::ofstream(p, std::ios::binary) << s(msgs[i].to_string());
        files += " '" + p.string() + "'";
    }
    auto out = run("python3 '" + (source_root() / "tests/encoding/email_oracle/oracle.py").string() + "' summary" + files);
    auto lines = sgcl::string(out).split('\n');
    size_t i = 0;
    for (auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        ASSERT_LT(i, msgs.size());
        auto j = json::parse(sgcl::string(line));
        ASSERT_TRUE(j) << s(sgcl::string(line));
        auto back = email::parse(msgs[i].to_string());
        ASSERT_TRUE(back);
        same(*j, *back, "message " + std::to_string(i));
        ++i;
    }
    EXPECT_EQ(i, msgs.size());
}

TEST(EmailOracle_Tests, WeReadWhatPythonWrites) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    auto dir = work() / "py";
    std::filesystem::remove_all(dir);
    run("python3 '" + (source_root() / "tests/encoding/email_oracle/oracle.py").string() + "' build '" + dir.string() + "'");
    std::ifstream summary(dir / "summary.jsonl");
    std::string line;
    int i = 0;
    while (std::getline(summary, line)) {
        char name[16];
        std::snprintf(name, sizeof name, "%02d.eml", i);
        auto m = email::load(sgcl::string((dir / name).string()));
        ASSERT_TRUE(m) << name;
        auto j = json::parse(sgcl::string(line));
        ASSERT_TRUE(j);
        // Python lists a message/rfc822 attachment but not a related part's image
        same(*j, *m, name, true);
        ++i;
    }
    EXPECT_GE(i, 7);
}

TEST(EmailOracle_Tests, GoReadsWhatWeWrite) {
    if (go_mail().empty()) {
        GTEST_SKIP() << "no go";
    }
    auto dir = work() / "ours_go";
    std::filesystem::create_directories(dir);
    auto msgs = ours();
    std::string files;
    for (size_t i = 0; i < msgs.size(); ++i) {
        auto p = dir / (std::to_string(i) + ".eml");
        std::ofstream(p, std::ios::binary) << s(msgs[i].to_string());
        files += " '" + p.string() + "'";
    }
    auto out = run("'" + go_mail() + "' summary" + files);
    auto lines = sgcl::string(out).split('\n');
    size_t i = 0;
    for (auto& line : lines) {
        if (line.empty()) {
            continue;
        }
        auto j = json::parse(sgcl::string(line));
        ASSERT_TRUE(j) << s(sgcl::string(line));
        auto back = email::parse(msgs[i].to_string());
        ASSERT_TRUE(back);
        // Go's walk lists a message/rfc822 part by its name as any other
        same(*j, *back, "message " + std::to_string(i), i != 5);
        ++i;
    }
    EXPECT_EQ(i, msgs.size());
}

TEST(EmailOracle_Tests, WeReadWhatGoWrites) {
    if (go_mail().empty()) {
        GTEST_SKIP() << "no go";
    }
    auto dir = work() / "go";
    std::filesystem::remove_all(dir);
    run("'" + go_mail() + "' build '" + dir.string() + "'");
    std::ifstream summary(dir / "summary.jsonl");
    std::string line;
    int i = 0;
    while (std::getline(summary, line)) {
        char name[16];
        std::snprintf(name, sizeof name, "%02d.eml", i);
        auto m = email::load(sgcl::string((dir / name).string()));
        ASSERT_TRUE(m) << name;
        auto j = json::parse(sgcl::string(line));
        ASSERT_TRUE(j);
        same(*j, *m, name);
        ++i;
    }
    EXPECT_EQ(i, 3);
}
