//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// http/2: h2spec (RFC 9113 and RFC 7541 conformance, ~146 cases) against
// the module's server, by prior knowledge (h2c) and over TLS. h2spec is an
// oracle outside the tree (/opt/homebrew/bin/h2spec, or H2SPEC): without it
// the tests are skipped. Each run is bounded by h2spec's own timeout per
// case (-o 2) and by `timeout` around it.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/http/http.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

using namespace sgcl;

namespace {
    std::string testdata(const std::string& name) {
        return (source_root() / "tests/net/tls_testdata" / name).string();
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path);
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }

    std::string h2spec() {
        if (const char* e = std::getenv("H2SPEC")) {
            return e;
        }
        return std::filesystem::exists("/opt/homebrew/bin/h2spec") ? "/opt/homebrew/bin/h2spec" : "";
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) {
            out.append(buf, n);
        }
        pclose(p);
        return out;
    }

    net::http::server server() {
        net::http::server s;
        s.h2c = true;
        s.not_found([](net::http::request r, net::http::response_writer w) -> async::task<> {
            (void)co_await r.async_bytes();
            w.set_header("Content-Type", "text/plain");
            w.write("h2spec\n");
        });
        return s;
    }

    // "146 tests, 140 passed, 0 skipped, 6 failed" and the failures' lines
    struct Summary {
        int tests = -1;
        int passed = -1;
        int failed = -1;
        std::string report;
    };

    Summary spec(bool secure) {
        net::http::server s = server();
        net::listener l;
        if (secure) {
            net::tls::config c;
            c.identities = {net::tls::identity(sgcl::string(slurp(testdata("ecdsa.pem"))), sgcl::string(slurp(testdata("ecdsa.key"))))};
            c.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
            l = *net::tls::listen("127.0.0.1:0", c);
        } else {
            l = *net::tcp::listen("127.0.0.1:0");
        }
        const uint16_t port = l.local_endpoint().port();
        auto serving = async::spawn(s.async_serve(l));
        std::string cmd = "perl -e 'alarm 600; exec @ARGV' '" + h2spec() + "' -o 2 -h 127.0.0.1 -p " + std::to_string(port) + (secure ? " -t -k" : "") + (std::getenv("H2SPEC_STRICT") ? " -S" : "") + " 2>&1";
        Summary r;
        r.report = run(cmd);
        s.close();
        (void)serving.wait();
        std::smatch m;
        if (std::regex_search(r.report, m, std::regex(R"((\d+) tests, (\d+) passed, (\d+) skipped, (\d+) failed)"))) {
            r.tests = std::stoi(m[1]);
            r.passed = std::stoi(m[2]);
            r.failed = std::stoi(m[4]);
        }
        return r;
    }
}

// By prior knowledge on a port shared with HTTP/1.1 (sketch-http2.md, 2):
// bytes that are not the preface are a request of HTTP/1.1, answered 400
// and closed, not GOAWAY. h2spec's "3.5/2 Sends invalid connection
// preface" is the one case that differs, by design; over TLS, where ALPN
// said h2 and the machine reads the preface, it passes
TEST(H2Spec_Tests, H2c) {
    if (h2spec().empty()) {
        GTEST_SKIP() << "no h2spec (H2SPEC or /opt/homebrew/bin/h2spec)";
    }
    auto r = spec(false);
    ASSERT_GT(r.tests, 0) << r.report;
    EXPECT_EQ(r.failed, 1) << r.report;
    EXPECT_NE(r.report.find("× 2: Sends invalid connection preface"), std::string::npos) << r.report;
}

TEST(H2Spec_Tests, Tls) {
    if (h2spec().empty()) {
        GTEST_SKIP() << "no h2spec (H2SPEC or /opt/homebrew/bin/h2spec)";
    }
    auto r = spec(true);
    ASSERT_GT(r.tests, 0) << r.report;
    EXPECT_EQ(r.failed, 0) << r.report;
}
