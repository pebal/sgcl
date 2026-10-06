//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Encrypted Client Hello (RFC 9849) in net::tls: our client and our server
// on the loopback (accepted, through a HelloRetryRequest, rejected with
// retry_configs that connect() retries with, rejected by a server without
// ECH, a public name's certificate that does not verify), Go's crypto/tls
// both ways (tests/net/go_ech: Go's client to our server, our client to
// Go's server, accepted, through a HelloRetryRequest and rejected), the wire
// of ECHConfigList, of the outer extension and of the inner hello with
// ech_outer_extensions (tls/detail/ech.h), and the refusals of the API.
#include "tests/types.h"
#include "tests/source_root.h"

#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace sgcl;
namespace tls = sgcl::net::tls;

namespace {
    // A CA and identities of the names it issues, made here
    struct Ca {
        crypto::p256::private_key key = crypto::p256::private_key::generate();
        optional<crypto::x509::certificate> root;
        crypto::x509::certificate_pool pool;

        Ca() {
            crypto::x509::certificate_template t;
            t.common_name = "ech test root";
            t.is_ca = true;
            root = crypto::x509::create_certificate(t, key);
            pool.add(*root);
        }

        // the leaf's PEM and the key's PEM
        pair<string, crypto::secret_bytes> issue_pem(const std::string& name) const {
            auto k = crypto::p256::private_key::generate();
            crypto::x509::certificate_template t;
            t.dns_names = {string(name)};
            auto spki = k.public_key().to_pkix_der();
            auto leaf = crypto::x509::create_certificate(t, spki.as_slice(), *root, key);
            auto pem = encoding::pem("CERTIFICATE", vector<byte>(leaf.raw().begin(), leaf.raw().end())).to_string();
            return pair<string, crypto::secret_bytes>(pem, k.to_pem());
        }

        tls::identity issue(const std::string& name) const {
            auto [pem, key_pem] = issue_pem(name);
            return tls::identity(pem, key_pem.as_slice());
        }

        string root_pem() const {
            return encoding::pem("CERTIFICATE", vector<byte>(root->raw().begin(), root->raw().end())).to_string();
        }
    };

    std::string text(const string& s) {
        return std::string(s.data(), s.size());
    }

    std::string hex(const slice<const byte>& b) {
        return text(encoding::hex::encode(b));
    }

    // An accept loop answering one line: "ech=<accepted> sni=<name>"
    async::task<> answer(net::listener l) {
        for (;;) {
            auto c = co_await l.async_accept();
            if (!c) {
                co_return;
            }
            auto st = tls::state_of(*c);
            std::string line = std::string("ech=") + (st && st->ech_accepted ? "true" : "false") + " sni=" + (st ? text(st->server_name) : "") + "\n";
            (void)co_await c->async_write(string(line));
            (void)co_await c->async_close();
        }
    }

    std::string line_of(const net::connection& c) {
        auto l = c.read_line();
        if (!l || !*l) {
            return "";
        }
        return std::string((*l)->data(), (*l)->size());
    }

    std::string at(const net::listener& l) {
        return "127.0.0.1:" + std::to_string(l.local_endpoint().port());
    }

    // The server: identities of the public name and the secret name, and
    // the ECH keys given
    tls::config server_config(const Ca& ca, const vector<tls::ech_key>& keys) {
        tls::config sc;
        sc.identities = {ca.issue("public.test"), ca.issue("secret.test")};
        sc.ech_keys = keys;
        return sc;
    }

    tls::config client_config(const Ca& ca, const vector<byte>& list) {
        tls::config c;
        c.server_name = "secret.test";
        c.roots = ca.pool;
        c.ech_config_list = list;
        return c;
    }
}

TEST(TlsEch, AcceptedOnTheLoopback) {
    Ca ca;
    auto key = tls::ech_key::generate("public.test");
    auto l = tls::listen("127.0.0.1:0", server_config(ca, {key}));
    ASSERT_TRUE(l) << l.error().message().view();
    auto serving = async::spawn(answer(*l));
    for (auto suite : {crypto::hpke::aead::aes128_gcm, crypto::hpke::aead::chacha20_poly1305}) {
        auto k = tls::ech_key::generate("public.test", {.suites = {crypto::hpke::suite{.aead = suite}}});
        (void)k;
    }
    auto c = tls::connect(string(at(*l)), client_config(ca, tls::ech_config_list({key})));
    ASSERT_TRUE(c) << c.error().message().view();
    auto st = tls::state_of(*c);
    EXPECT_TRUE(st->ech_accepted);
    EXPECT_EQ(text(st->server_name), "secret.test");
    EXPECT_EQ(text(st->peer_certificates[0].dns_names()[0]), "secret.test");
    EXPECT_EQ(line_of(*c), "ech=true sni=secret.test");
    // without ECH the same server serves the name in the clear
    tls::config plain;
    plain.server_name = "secret.test";
    plain.roots = ca.pool;
    auto p = tls::connect(string(at(*l)), plain);
    ASSERT_TRUE(p) << p.error().message().view();
    EXPECT_FALSE(tls::state_of(*p)->ech_accepted);
    EXPECT_EQ(line_of(*p), "ech=false sni=secret.test");
    (void)l->close();
    serving.wait();
}

TEST(TlsEch, EverySuiteAndKem) {
    Ca ca;
    for (auto kem : {crypto::hpke::kem::dhkem_x25519, crypto::hpke::kem::dhkem_p256, crypto::hpke::kem::dhkem_p384, crypto::hpke::kem::dhkem_p521}) {
        for (auto aead : {crypto::hpke::aead::aes128_gcm, crypto::hpke::aead::aes256_gcm, crypto::hpke::aead::chacha20_poly1305}) {
            for (auto kdf : {crypto::hpke::kdf::hkdf_sha256, crypto::hpke::kdf::hkdf_sha512}) {
                auto key = tls::ech_key::generate("public.test", {.kem = kem, .suites = {crypto::hpke::suite{kdf, aead}}, .max_name_length = 40});
                auto l = tls::listen("127.0.0.1:0", server_config(ca, {key}));
                ASSERT_TRUE(l);
                auto serving = async::spawn(answer(*l));
                auto c = tls::connect(string(at(*l)), client_config(ca, tls::ech_config_list({key})));
                ASSERT_TRUE(c) << int(kem) << " " << int(kdf) << " " << int(aead) << ": " << c.error().message().view();
                EXPECT_EQ(line_of(*c), "ech=true sni=secret.test") << int(kem) << " " << int(kdf) << " " << int(aead);
                (void)l->close();
                serving.wait();
            }
        }
    }
}

TEST(TlsEch, ThroughAHelloRetryRequest) {
    Ca ca;
    auto key = tls::ech_key::generate("public.test");
    auto sc = server_config(ca, {key});
    sc.groups = {tls::group::x25519};
    auto l = tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l);
    auto serving = async::spawn(answer(*l));
    auto cc = client_config(ca, tls::ech_config_list({key}));
    cc.groups = {tls::group::secp384r1, tls::group::x25519};   // the first share P-384's: the server asks for X25519
    auto c = tls::connect(string(at(*l)), cc);
    ASSERT_TRUE(c) << c.error().message().view();
    EXPECT_TRUE(tls::state_of(*c)->ech_accepted);
    EXPECT_EQ(tls::state_of(*c)->group, tls::group::x25519);
    EXPECT_EQ(line_of(*c), "ech=true sni=secret.test");
    (void)l->close();
    serving.wait();
}

TEST(TlsEch, RejectedWithRetryConfigs) {
    Ca ca;
    auto old_key = tls::ech_key::generate("public.test", {.config_id = 1});
    auto new_key = tls::ech_key::generate("public.test", {.config_id = 2});
    auto l = tls::listen("127.0.0.1:0", server_config(ca, {new_key}));
    ASSERT_TRUE(l);
    auto serving = async::spawn(answer(*l));
    // client(): the rejection, ech_required, the server checked for the public name
    {
        auto t = net::tcp::connect(string(at(*l)));
        ASSERT_TRUE(t);
        auto c = tls::client(*t, client_config(ca, tls::ech_config_list({old_key})));
        ASSERT_FALSE(c);
        EXPECT_TRUE(c.error().code() == tls::alert::ech_required) << c.error().message().view();
        // the server's retry_configs, read back from the error: the new key's list, which a client takes
        auto retry = tls::ech_retry_configs(c.error());
        EXPECT_TRUE(retry == tls::ech_config_list({new_key})) << c.error().message().view();
        auto t2 = net::tcp::connect(string(at(*l)));
        ASSERT_TRUE(t2);
        auto c2 = tls::client(*t2, client_config(ca, retry));
        ASSERT_TRUE(c2) << c2.error().message().view();
        EXPECT_TRUE(tls::state_of(*c2)->ech_accepted);
        EXPECT_EQ(line_of(*c2), "ech=true sni=secret.test");
        // nothing of any other error, of an error of another code with the marker in its text, of a marker of no base64
        EXPECT_TRUE(tls::ech_retry_configs(io::error(make_error_code(tls::alert::handshake_failure), "handshake", c.error().path())).empty());
        EXPECT_TRUE(tls::ech_retry_configs(io::error(make_error_code(tls::alert::ech_required), "handshake", "tls ech=!!")).empty());
        EXPECT_TRUE(tls::ech_retry_configs(io::error(make_error_code(tls::alert::ech_required), "handshake", "tls")).empty());
    }
    // connect(): once more with the retry configs, accepted
    auto c = tls::connect(string(at(*l)), client_config(ca, tls::ech_config_list({old_key})));
    ASSERT_TRUE(c) << c.error().message().view();
    EXPECT_TRUE(tls::state_of(*c)->ech_accepted);
    EXPECT_EQ(line_of(*c), "ech=true sni=secret.test");
    (void)l->close();
    serving.wait();
}

TEST(TlsEch, RejectedByAServerWithoutEch) {
    Ca ca;
    auto key = tls::ech_key::generate("public.test");
    auto l = tls::listen("127.0.0.1:0", server_config(ca, {}));
    ASSERT_TRUE(l);
    auto serving = async::spawn(answer(*l));
    auto c = tls::connect(string(at(*l)), client_config(ca, tls::ech_config_list({key})));
    ASSERT_FALSE(c);
    EXPECT_TRUE(c.error().code() == tls::alert::ech_required) << c.error().message().view();
    EXPECT_TRUE(tls::ech_retry_configs(c.error()).empty());   // a server without keys sends none
    // a server whose certificate is not the public name's: the chain does not verify
    tls::config only_secret;
    only_secret.identities = {ca.issue("secret.test")};
    auto l2 = tls::listen("127.0.0.1:0", only_secret);
    ASSERT_TRUE(l2);
    auto serving2 = async::spawn(answer(*l2));
    auto c2 = tls::connect(string(at(*l2)), client_config(ca, tls::ech_config_list({key})));
    ASSERT_FALSE(c2);
    EXPECT_FALSE(c2.error().code() == tls::alert::ech_required) << c2.error().message().view();
    // insecure_skip_verify does not skip the public name's check
    auto cc = client_config(ca, tls::ech_config_list({key}));
    cc.insecure_skip_verify = true;
    auto c3 = tls::connect(string(at(*l2)), cc);
    ASSERT_FALSE(c3);
    (void)l->close();
    (void)l2->close();
    serving.wait();
    serving2.wait();
}

TEST(TlsEch, TheApiRefuses) {
    EXPECT_THROW((void)tls::ech_key::generate("127.0.0.1"), std::invalid_argument);
    EXPECT_THROW((void)tls::ech_key::generate("under_score.test"), std::invalid_argument);
    EXPECT_THROW((void)tls::ech_key::generate("public.test", {.suites = {crypto::hpke::suite{.aead = crypto::hpke::aead::export_only}}}), std::invalid_argument);
    auto key = tls::ech_key::generate("public.test", {.config_id = 7, .max_name_length = 32});
    EXPECT_EQ(key.config_id(), 7);
    EXPECT_EQ(text(key.public_name()), "public.test");
    EXPECT_TRUE(key.retry());
    // from_bytes: the key's own bytes read back, another key refused, a config that does not read refused
    auto config = key.config();
    auto priv = key.private_key();
    auto again = tls::ech_key::from_bytes(config, priv, false);
    ASSERT_TRUE(again) << again.error().message().view();
    EXPECT_FALSE(again->retry());
    EXPECT_TRUE(again->config() == config);
    auto other = tls::ech_key::generate("public.test");
    EXPECT_FALSE(tls::ech_key::from_bytes(config, other.private_key()));
    EXPECT_FALSE(tls::ech_key::from_bytes(slice<const byte>(config.data(), config.size() - 1), priv));
    EXPECT_FALSE(tls::ech_key::from_bytes(slice<const byte>(), priv));
    // a list of nothing the module takes: the client's config is refused
    Ca ca;
    auto plain = net::tcp::listen("127.0.0.1:0");   // config errors come before a record: a plain listener serves
    ASSERT_TRUE(plain);
    auto dial = [&](const tls::config& c) {
        auto t = net::tcp::connect(string(at(*plain)));
        return tls::client(*t, c);
    };
    auto bad = client_config(ca, vector<byte>{byte(0), byte(4), byte(0xfe), byte(0x0c), byte(0), byte(0)});
    auto t = dial(bad);
    ASSERT_FALSE(t);
    EXPECT_TRUE(t.error().code() == crypto::errc::unsupported) << t.error().message().view();
    // a list that does not read: malformed
    auto t2 = dial(client_config(ca, vector<byte>{byte(0), byte(2), byte(0xfe), byte(0x0c)}));
    ASSERT_FALSE(t2);
    EXPECT_TRUE(t2.error().code() == crypto::errc::malformed) << t2.error().message().view();
    // TLS 1.2 alone with a list: EINVAL
    auto twelve = client_config(ca, tls::ech_config_list({key}));
    twelve.max_version = tls::version::tls12;
    auto t3 = dial(twelve);
    ASSERT_FALSE(t3);
    EXPECT_TRUE(t3.error().code() == std::errc::invalid_argument) << t3.error().message().view();
    (void)plain->close();
    // the list of no keys is empty
    EXPECT_TRUE(tls::ech_config_list({}).empty());
    // the list of two keys holds both configs
    auto two = tls::ech_config_list({key, other});
    EXPECT_EQ(two.size(), 2 + config.size() + other.config().size());
}

// --- the wire ----------------------------------------------------------------

TEST(TlsEch, TheWire) {
    namespace d = sgcl::net::tls::detail;
    auto key = tls::ech_key::generate("public.test", {.config_id = 3});
    auto list = tls::ech_config_list({key});
    // a config of another version and one of a mandatory extension are passed over
    std::vector<byte> mixed = {byte(0), byte(0), byte(0xfe), byte(0x0a), byte(0), byte(1), byte(9)};
    auto config = key.config();
    mixed.insert(mixed.end(), config.begin(), config.end());
    std::vector<byte> mandatory = d::write_ech_config(9, 0x20, slice<const byte>(config.data() + 9, 32), {0x00010001}, 0, "x.test");
    mandatory[mandatory.size() - 2] = byte(0);
    mandatory[mandatory.size() - 1] = byte(4);
    mandatory.insert(mandatory.end(), {byte(0x80), byte(1), byte(0), byte(0)});
    mandatory[2] = byte((mandatory.size() - 4) >> 8);
    mandatory[3] = byte(mandatory.size() - 4);
    mixed.insert(mixed.end(), mandatory.begin(), mandatory.end());
    mixed[0] = byte((mixed.size() - 2) >> 8);
    mixed[1] = byte(mixed.size() - 2);
    auto configs = d::read_ech_configs(d::bytes_of(mixed.data(), mixed.size()));
    ASSERT_TRUE(configs);
    ASSERT_EQ(configs->size(), 1u);
    EXPECT_EQ(configs->front().id, 3);
    EXPECT_EQ(configs->front().public_name, "public.test");
    EXPECT_FALSE(d::read_ech_configs(d::bytes_of(mixed.data(), mixed.size() - 1)));
    // the public names a config may have
    EXPECT_TRUE(d::ech_public_name_ok("a-b.example.com"));
    for (const char* n : {"", "a..b", ".a", "a.", "192.168.0.1", "a.123", "a_b.test", "xn--ä.test"}) {
        EXPECT_FALSE(d::ech_public_name_ok(n)) << n;
    }
    // padding: to max_name_length, then to 32
    EXPECT_EQ((100 + d::ech_padding(100, 10, 32)) % 32, 0u);
    EXPECT_GE(d::ech_padding(100, 10, 32), 22u);
    EXPECT_EQ((77 + d::ech_padding(77, 0, 16)) % 32, 0u);
}

TEST(TlsEch, AnInnerHelloWithOuterExtensions) {
    namespace d = sgcl::net::tls::detail;
    // an outer hello with key_share and supported_groups; the inner names them in ech_outer_extensions
    std::vector<byte> outer_msg;
    {
        d::Builder w(outer_msg);
        uint8_t random[32] = {1};
        uint8_t sid[32] = {2};
        std::vector<uint16_t> suites = {0x1301};
        d::write_client_hello(w, d::bytes_of(random, 32), d::bytes_of(sid, 32), suites, [&](d::Builder& w) noexcept {
            {
                auto e = w.extension(uint16_t(10));
                w.u16(2);
                w.u16(0x1d);
            }
            {
                auto e = w.extension(uint16_t(51));
                w.u16(4);
                w.u16(0x1d);
                w.u16(0);
            }
            {
                auto e = w.extension(uint16_t(43));
                w.u8(2);
                w.u16(0x0304);
            }
        });
    }
    auto oh = d::read_handshake(d::bytes_of(outer_msg.data(), outer_msg.size()));
    auto outer = d::read_client_hello(oh->body);
    ASSERT_TRUE(outer);
    // built by hand in one buffer: the structure, then padding
    auto build = [&](std::vector<uint16_t> refs, bool with_inner, size_t pad, uint8_t pad_byte) {
        std::vector<byte> e;
        {
            d::Builder w(e);
            w.u16(0x0303);
            for (int i = 0; i < 32; ++i) {
                w.u8(9);
            }
            w.u8(0);
            w.u16(2);
            w.u16(0x1301);
            w.u8(1);
            w.u8(0);
            {
                auto list = w.block16();
                {
                    auto x = w.extension(uint16_t(0));
                    w.u16(14);
                    w.u8(0);
                    w.u16(11);
                    w.bytes("secret.test", 11);
                }
                if (!refs.empty()) {
                    auto x = w.extension(d::EchOuterExtensions);
                    w.u8(uint8_t(2 * refs.size()));
                    for (auto r : refs) {
                        w.u16(r);
                    }
                }
                if (with_inner) {
                    auto x = w.extension(d::EchExtension);
                    w.u8(1);
                }
            }
        }
        e.insert(e.end(), pad, byte(pad_byte));
        return e;
    };
    auto good = build({10, 51}, true, 7, 0);
    auto inner = d::decode_inner(d::bytes_of(good.data(), good.size()), *outer);
    ASSERT_TRUE(inner) << inner.error().what;
    auto ih = d::read_handshake(d::bytes_of(inner->data(), inner->size()));
    auto ich = d::read_client_hello(ih->body);
    ASSERT_TRUE(ich);
    EXPECT_EQ(ich->session_id.size(), 32u);                 // the outer's
    EXPECT_TRUE(ich->extensions.find(uint16_t(10)));       // copied from the outer
    EXPECT_TRUE(ich->extensions.find(uint16_t(51)));
    EXPECT_FALSE(ich->extensions.find(d::EchOuterExtensions));
    // refusals: out of the outer's order, a type it lacks, ECH named, no inner mark, padding not zero
    for (auto& bad : {build({51, 10}, true, 0, 0), build({41}, true, 0, 0), build({uint16_t(d::EchExtension)}, true, 0, 0), build({10}, false, 0, 0),
                      build({10}, true, 3, 1)}) {
        auto r = d::decode_inner(d::bytes_of(bad.data(), bad.size()), *outer);
        EXPECT_FALSE(r);
        if (!r) {
            EXPECT_EQ(r.error().description, d::AlertDescription::illegal_parameter);
        }
    }
}

// --- Go's crypto/tls ------------------------------------------------------------

namespace {
    const std::string& go_ech() {
        static std::string path = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            auto dir = std::filesystem::temp_directory_path() / "sgcl_ech_go";
            std::filesystem::create_directories(dir);
            std::filesystem::copy_file(source_root() / "tests/net/go_ech/main.go", dir / "main.go", std::filesystem::copy_options::overwrite_existing);
            std::ofstream(dir / "go.mod") << "module echoracle\n\ngo 1.26\n";
            auto out = dir / "go_ech";
            std::string cmd = "cd '" + dir.string() + "' && GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off go build -o '" + out.string() + "' . > build.log 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = ::popen((cmd + " 2>&1").c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        ::pclose(p);
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) {
            out.pop_back();
        }
        return out;
    }

    std::string write_file(const std::string& name, const std::string& content) {
        auto path = (std::filesystem::temp_directory_path() / ("sgcl_ech_" + std::to_string(::getpid()) + "_" + name)).string();
        std::ofstream(path) << content;
        return path;
    }
}

TEST(TlsEchGo, GosClientToOurServer) {
    if (go_ech().empty()) {
        GTEST_SKIP() << "no go";
    }
    Ca ca;
    const std::string ca_file = write_file("ca.pem", text(ca.root_pem()));
    auto key = tls::ech_key::generate("public.test");
    auto l = tls::listen("127.0.0.1:0", server_config(ca, {key}));
    ASSERT_TRUE(l);
    auto serving = async::spawn(answer(*l));
    const std::string list = hex(tls::ech_config_list({key}));
    EXPECT_EQ(run(go_ech() + " client " + at(*l) + " secret.test " + ca_file + " " + list), "ok ech=true line=ech=true sni=secret.test");
    // through a HelloRetryRequest: Go's first share P-384's, our server takes X25519 first... and asks for it
    auto sc = server_config(ca, {key});
    sc.groups = {tls::group::x25519};
    auto l2 = tls::listen("127.0.0.1:0", sc);
    ASSERT_TRUE(l2);
    auto serving2 = async::spawn(answer(*l2));
    EXPECT_EQ(run(go_ech() + " client " + at(*l2) + " secret.test " + ca_file + " " + list + " p384"), "ok ech=true line=ech=true sni=secret.test");
    // rejected: a config of another key; Go sees our retry configs
    auto other = tls::ech_key::generate("public.test", {.config_id = uint8_t(key.config_id() + 1)});
    const std::string r = run(go_ech() + " client " + at(*l) + " secret.test " + ca_file + " " + hex(tls::ech_config_list({other})));
    EXPECT_EQ(r, "error rejected retry=" + list);
    (void)l->close();
    (void)l2->close();
    serving.wait();
    serving2.wait();
}

TEST(TlsEchGo, OurClientToGosServer) {
    if (go_ech().empty()) {
        GTEST_SKIP() << "no go";
    }
    Ca ca;
    auto [pem, key_pem] = ca.issue_pem("secret.test");
    const std::string cert_file = write_file("leaf.pem", text(pem));
    const std::string key_file = write_file("leaf.key", std::string(reinterpret_cast<const char*>(key_pem.as_slice().data()), key_pem.size()));
    auto key = tls::ech_key::generate("public.test");
    for (bool retry : {false, true}) {
        const std::string mode = retry ? " x25519" : "";
        FILE* p = ::popen((go_ech() + " server " + cert_file + " " + key_file + " " + hex(key.config()) + " " + hex(key.private_key()) + " 1" + mode).c_str(), "r");
        ASSERT_TRUE(p);
        char line[64] = {};
        ASSERT_TRUE(std::fgets(line, sizeof line, p));
        const int port = std::atoi(line + 5);
        auto cc = client_config(ca, tls::ech_config_list({key}));
        if (retry) {
            cc.groups = {tls::group::secp384r1, tls::group::x25519};
        }
        auto c = tls::connect(string("127.0.0.1:" + std::to_string(port)), cc);
        ASSERT_TRUE(c) << c.error().message().view();
        EXPECT_TRUE(tls::state_of(*c)->ech_accepted);
        EXPECT_EQ(line_of(*c), "ech=true sni=secret.test");
        ::pclose(p);
    }
}
