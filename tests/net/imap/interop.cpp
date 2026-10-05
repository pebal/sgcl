//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::imap::server against clients of others: Python's imaplib through a
// full scripted session (python/session.py: AUTHENTICATE PLAIN, LIST,
// CREATE with a modified UTF-7 name, APPEND with a date, SELECT, SEARCH,
// FETCH, STORE, UID commands, COPY, STATUS, SORT, THREAD, NAMESPACE,
// GETQUOTAROOT, LSUB, IDLE told of another connection's APPEND, EXPUNGE,
// CHECK, CLOSE, RENAME, DELETE, LOGOUT), in the clear and after STARTTLS;
// and curl's IMAP: LIST, a message by UID, APPEND (-T), a custom command
// (-X), STARTTLS (--ssl-reqd) and imaps://. Skipped where python3 or curl
// is missing.
#include "tests/net/imap/helpers.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>

using namespace sgcl;
using namespace sgcl_test::imap;
namespace imap = sgcl::net::imap;

namespace {
    // "name value" lines of the script's output
    std::map<std::string, std::string> lines_of(const std::string& out) {
        std::map<std::string, std::string> m;
        std::istringstream in(out);
        std::string l;
        while (std::getline(in, l)) {
            const size_t sp = l.find(' ');
            m[l.substr(0, sp)] = sp == std::string::npos ? std::string() : l.substr(sp + 1);
        }
        return m;
    }

    std::string script() {
        return (source_root() / "tests/net/imap/python/session.py").string();
    }

    void check_session(const std::map<std::string, std::string>& m, const std::string& out) {
        auto at = [&](const char* k) {
            auto it = m.find(k);
            return it == m.end() ? std::string("<missing>") : it->second;
        };
        EXPECT_TRUE(contains(at("caps"), "IMAP4REV2")) << out;
        EXPECT_EQ(at("auth"), "OK") << out;
        EXPECT_EQ(at("list"), "OK") << out;
        EXPECT_EQ(at("create"), "OK OK") << out;
        EXPECT_TRUE(contains(at("append"), "OK [APPENDUID ")) << out;
        EXPECT_EQ(at("select"), "OK 2") << out;
        EXPECT_EQ(at("search"), "OK 2") << out;
        EXPECT_EQ(at("search_subject"), "OK 2") << out;
        EXPECT_EQ(at("fetch"), "OK 2") << out;
        EXPECT_EQ(at("fetch_body"), "OK True") << out;
        EXPECT_EQ(at("fetch_text"), "OK body text") << out;
        EXPECT_TRUE(contains(at("store"), "\\Seen")) << out;
        EXPECT_EQ(at("uid_search"), "OK 2") << out;
        EXPECT_EQ(at("copy"), "OK") << out;
        EXPECT_TRUE(contains(at("status"), "MESSAGES 2")) << out;
        EXPECT_EQ(at("sort"), "OK 1 2") << out;   // "first" sorts before "hello from python"
        EXPECT_TRUE(contains(at("thread"), "OK (")) << out;
        EXPECT_EQ(at("namespace"), "OK") << out;
        EXPECT_EQ(at("quota"), "OK") << out;
        EXPECT_EQ(at("lsub"), "OK") << out;
        EXPECT_TRUE(contains(at("uid_fetch"), "UID 1")) << out;
        EXPECT_TRUE(contains(at("idle"), "EXISTS 3")) << out;
        EXPECT_TRUE(contains(at("expunge"), "OK")) << out;
        EXPECT_EQ(at("check"), "OK") << out;
        EXPECT_EQ(at("close"), "OK") << out;
        EXPECT_EQ(at("rename"), "OK") << out;
        EXPECT_EQ(at("delete"), "OK") << out;
        EXPECT_EQ(at("logout"), "BYE") << out;
    }
}

TEST(ImapInterop, PythonImaplib) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("first"));
    Server s(mail);
    int status = 0;
    const std::string out = run_command("python3 " + script() + " " + std::to_string(s.port) + " 2>&1", &status);
    EXPECT_EQ(status, 0) << out;
    check_session(lines_of(out), out);
    // the backend saw what Python did
    auto boxes = mail.mailboxes("alice");
    bool zazo = false;
    for (const auto& b : *boxes) {
        zazo |= text(b.name) == "Zazó";
    }
    EXPECT_TRUE(zazo);
}

TEST(ImapInterop, PythonImaplibStartTls) {
    if (!have("python3")) {
        GTEST_SKIP() << "no python3";
    }
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("first"));
    Server s(mail, false, [](imap::server& srv) { srv.tls = server_tls(); });
    int status = 0;
    const std::string out = run_command("python3 " + script() + " " + std::to_string(s.port) + " tls " + testdata("ca.pem") + " 2>&1", &status);
    EXPECT_EQ(status, 0) << out;
    auto m = lines_of(out);
    EXPECT_EQ(m["starttls"], "OK") << out;
    EXPECT_FALSE(contains(m["caps"], "LOGINDISABLED")) << out;
    check_session(m, out);
}

TEST(ImapInterop, Curl) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("first", "the first body\r\n"));
    mail.create("alice", "Work", "");
    Server s(mail);
    const std::string base = "imap://127.0.0.1:" + std::to_string(s.port);
    const std::string user = " --user alice:secret --max-time 10 ";
    // LIST
    std::string out = run_command("curl -s" + user + base + "/");
    EXPECT_TRUE(contains(out, "\"/\" INBOX")) << out;
    EXPECT_TRUE(contains(out, "\"/\" Work")) << out;
    // a message by UID
    out = run_command("curl -s" + user + "'" + base + "/INBOX;UID=1'");
    EXPECT_TRUE(contains(out, "Subject: first")) << out;
    EXPECT_TRUE(contains(out, "the first body")) << out;
    // a section
    out = run_command("curl -s" + user + "'" + base + "/INBOX;UID=1;SECTION=TEXT'");
    EXPECT_EQ(out, "the first body\r\n");
    // APPEND
    const std::string dir = scratch_dir("curl");
    const std::string file = dir + "/m.eml";
    {
        std::ofstream f(file, std::ios::binary);
        f << "From: curl@example.com\r\nSubject: from curl\r\n\r\nuploaded\r\n";
    }
    int status = 0;
    out = run_command("curl -s" + user + "-T " + file + " '" + base + "/Work'", &status);
    EXPECT_EQ(status, 0) << out;
    auto st = mail.open("alice", "Work");
    ASSERT_TRUE(st);
    EXPECT_EQ(st->messages.size(), 1u);
    // a command of its own
    out = run_command("curl -s" + user + "-X 'UID SEARCH SUBJECT curl' '" + base + "/Work'");
    EXPECT_TRUE(contains(out, "SEARCH 1")) << out;
    out = run_command("curl -s" + user + "-X 'EXAMINE INBOX' '" + base + "/'");
    EXPECT_TRUE(contains(out, "1 EXISTS")) << out;
    std::filesystem::remove_all(dir);
}

TEST(ImapInterop, CurlStartTlsAndImaps) {
    if (!have("curl")) {
        GTEST_SKIP() << "no curl";
    }
    auto mail = standard_mail();
    mail.append("alice", "INBOX", message("secure", "over tls\r\n"));
    Server s(mail, false, [](imap::server& srv) { srv.tls = server_tls(); });
    const std::string args = " -s --max-time 10 --user alice:secret --cacert " + testdata("ca.pem") + " --resolve localhost:" + std::to_string(s.port) + ":127.0.0.1 ";
    std::string out = run_command("curl" + args + "--ssl-reqd 'imap://localhost:" + std::to_string(s.port) + "/INBOX;UID=1' 2>&1");
    EXPECT_TRUE(contains(out, "over tls")) << out;
    // without STARTTLS the server refuses the login
    int status = 0;
    out = run_command("curl -s --max-time 10 --user alice:secret 'imap://127.0.0.1:" + std::to_string(s.port) + "/INBOX;UID=1' 2>&1", &status);
    EXPECT_NE(status, 0) << out;
    Server s2(mail, true);
    const std::string args2 = " -s --max-time 10 --user alice:secret --cacert " + testdata("ca.pem") + " --resolve localhost:" + std::to_string(s2.port) + ":127.0.0.1 ";
    out = run_command("curl" + args2 + "'imaps://localhost:" + std::to_string(s2.port) + "/INBOX;UID=1' 2>&1");
    EXPECT_TRUE(contains(out, "over tls")) << out;
}
