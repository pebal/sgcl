//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Go's golang.org/x/crypto/acme as the oracle (tests/net/acme/go_acme): its
// client through every flow against acme::test_server (an account, with an
// external account binding; an order of a name, a wildcard and an address;
// finalize, the chain, the alternates, revocation, key rollover, the
// account read, updated, deactivated), and what it computes of a challenge
// for one key (http-01's response, dns-01's record, tls-alpn-01's
// acmeIdentifier) against what acme::client computes. Skipped without go or
// without x/crypto v0.31.0 in the module cache (the build is offline:
// GOPROXY=off).
#include "acme_test.h"
#include "tests/source_root.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace sgcl;
using namespace acme_test;

namespace {
    // The oracle built once in a module of its own, in a temporary directory
    const std::string& oracle() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto dir = std::filesystem::temp_directory_path() / "sgcl_acme_go";
            std::filesystem::create_directories(dir);
            std::filesystem::copy_file(source_root() / "tests/net/acme/go_acme/main.go", dir / "main.go", std::filesystem::copy_options::overwrite_existing);
            std::ofstream(dir / "go.mod") << "module acmeoracle\n\ngo 1.23\n\nrequire golang.org/x/crypto v0.31.0\n";
            auto out = dir / "go_acme";
            std::string cmd = "cd '" + dir.string() + "' && GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off go build -o '" + out.string() + "' . > build.log 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    std::string run(const std::string& args) {
        std::string out;
        FILE* p = ::popen((oracle() + " " + args + " 2>&1").c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        ::pclose(p);
        return out;
    }
}

TEST(AcmeGoClient, EveryFlowAgainstTheTestServer) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go, or no golang.org/x/crypto v0.31.0 in the module cache";
    }
    net::acme::test_server::options so;
    so.skip_validation = true;
    so.terms_of_service = "https://ca.test/terms";
    so.alternate_chains = 1;
    net::acme::test_server ca(so);
    std::string out = run("'" + text(ca.directory_url()) + "'");
    EXPECT_NE(out.find("ok discover true"), std::string::npos) << out;
    EXPECT_NE(out.find("ok register valid"), std::string::npos) << out;
    EXPECT_NE(out.find("ok order pending 3"), std::string::npos) << out;
    EXPECT_NE(out.find("ok ready ready"), std::string::npos) << out;
    EXPECT_NE(out.find("ok certificate 2 example.test 1"), std::string::npos) << out;
    EXPECT_NE(out.find("ok alternates 1"), std::string::npos) << out;
    EXPECT_NE(out.find("ok revoke"), std::string::npos) << out;
    EXPECT_NE(out.find("ok rollover"), std::string::npos) << out;
    EXPECT_NE(out.find("ok update mailto:other@example.test"), std::string::npos) << out;
    EXPECT_NE(out.find("done"), std::string::npos) << out;
    EXPECT_EQ(ca.certificates(), 1u);
}

TEST(AcmeGoClient, ExternalAccountBinding) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go, or no golang.org/x/crypto v0.31.0 in the module cache";
    }
    net::acme::test_server::options so;
    so.skip_validation = true;
    so.require_external_account = true;
    std::string mac = text(encoding::base64::raw_url.encode(crypto::random::bytes(32).as_slice()));
    so.external_accounts.push_back({"go-kid", string(mac)});
    net::acme::test_server ca(so);
    std::string out = run("'" + text(ca.directory_url()) + "' go-kid " + mac);
    EXPECT_NE(out.find("ok register valid"), std::string::npos) << out;
    EXPECT_NE(out.find("done"), std::string::npos) << out;
    // a wrong MAC key is refused
    std::string wrong = text(encoding::base64::raw_url.encode(crypto::random::bytes(32).as_slice()));
    std::string bad = run("'" + text(ca.directory_url()) + "' go-kid " + wrong);
    EXPECT_NE(bad.find("fail register"), std::string::npos) << bad;
}

TEST(AcmeGoClient, TheChallengesValuesAgree) {
    if (oracle().empty()) {
        GTEST_SKIP() << "no go, or no golang.org/x/crypto v0.31.0 in the module cache";
    }
    // x/crypto's JWK has no OKP (Ed25519): the three kinds it has
    for (auto alg : {net::acme::key_algorithm::es256, net::acme::key_algorithm::es384, net::acme::key_algorithm::rs256}) {
        net::acme::account_key key(alg);
        auto pem = key.to_pem();
        auto file = std::filesystem::temp_directory_path() / "sgcl_acme_go_key.pem";
        {
            std::ofstream f(file, std::ios::binary);
            f.write(reinterpret_cast<const char*>(pem.as_slice().data()), long(pem.as_slice().size()));
        }
        std::string out = run("values '" + file.string() + "' tok_EN-123");
        std::filesystem::remove(file);
        net::acme::client c("http://127.0.0.1:1/directory", key);
        EXPECT_NE(out.find("http01 " + text(c.key_authorization("tok_EN-123")) + "\n"), std::string::npos) << out;
        EXPECT_NE(out.find("dns01 " + text(c.dns01_value("tok_EN-123")) + "\n"), std::string::npos) << out;
        EXPECT_NE(out.find("path " + text(net::acme::client::http01_path("tok_EN-123")) + "\n"), std::string::npos) << out;
        auto id = c.tls_alpn01_identity("tok_EN-123", "example.test");
        ASSERT_TRUE(id.has_value());
        std::string ours;
        for (auto& e : id->certificates()[0].extensions()) {
            if (text(e.oid) == "1.3.6.1.5.5.7.1.31") {
                static const char hex[] = "0123456789abcdef";
                for (auto b : e.value) {
                    ours += hex[uint8_t(b) >> 4];
                    ours += hex[uint8_t(b) & 15];
                }
            }
        }
        EXPECT_NE(out.find("alpn true " + ours + " example.test"), std::string::npos) << out << "\nours: " << ours;
    }
}
