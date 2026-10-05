//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The handshake messages of TLS 1.3 (sgcl/net/tls/detail/messages.h) and
// CertificateVerify's signature (signature.h):
//   - every handshake message of RFC 8448 §3–§7 read and written back byte
//     for byte, and every extension v1 reads, its body read and written
//     back; the fields checked by name;
//   - the CertificateVerify of §3 verified under the certificate of §3 over
//     the transcript of the messages before it;
//   - the refusals, one by one: a truncation at every byte, bytes past the
//     end, a vector past its bounds, a duplicate extension, pre_shared_key
//     not last, compression methods other than null, a key share of the
//     wrong size, the extensions a message may not carry.
#include "tests/types.h"

#include "sgcl/net/tls/detail/messages.h"
#include "sgcl/net/tls/detail/signature.h"
#include "tls_rfc8448.h"
#include "tls_roundtrip.h"

#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using namespace tls_roundtrip;
    using tls::AlertDescription;

    bytes_t unhex(const std::string& s) {
        bytes_t v;
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            v.push_back(uint8_t(std::stoi(s.substr(i, 2), nullptr, 16)));
        }
        return v;
    }

    const std::vector<rfc8448::Message>& messages() {
        static std::vector<rfc8448::Message> all = rfc8448::messages(rfc8448::read());
        return all;
    }

    const rfc8448::Message* find(int section, const std::string& who, const std::string& name, int nth = 0) {
        for (auto& m : messages()) {
            if (m.section == section && m.who == who && m.name == name && nth-- == 0) {
                return &m;
            }
        }
        return nullptr;
    }

    // The error of a result, or a success reported as a failure of the test
    template<class E>
    AlertDescription alert_of(const E& r) {
        return r ? AlertDescription::close_notify : r.error().description;
    }

    // A ClientHello of one's own: the fields and the extensions written by `ext`
    template<class F>
    bytes_t client_hello(F&& ext, std::vector<uint16_t> suites = {0x1301}) {
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        uint8_t random[32] = {1};
        tls::write_client_hello(w, tls::bytes_of(random, 32), tls::Bytes(), suites, std::forward<F>(ext));
        return of(out);
    }

    bytes_t body_of(const bytes_t& message) {
        return bytes_t(message.begin() + 4, message.end());
    }

    void x25519_share(tls::Builder& w) {
        auto e = w.extension(tls::ExtensionType::key_share);
        uint8_t key[32] = {9};
        std::vector<tls::KeyShare> shares = {{0x001D, tls::bytes_of(key, 32)}};   // lint-handles: ok slices over unmanaged bytes, no owner
        tls::write_key_shares(w, shares);
    }
}

// --- RFC 8448 -----------------------------------------------------------------

TEST(TlsMessages_Tests, EveryMessageOfRfc8448ComesBack) {
    if (messages().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    size_t count = 0;
    for (auto& m : messages()) {
        // §3–§7 negotiate TLS_AES_128_GCM_SHA256: a Finished of 32 bytes
        EXPECT_EQ(message_back(m.bytes, 32), "") << "§" << m.section << " " << m.who << " " << m.name << " " << m.bytes.size() << " bytes, header " << (m.bytes.size() >= 4 ? (size_t(m.bytes[1]) << 16 | size_t(m.bytes[2]) << 8 | m.bytes[3]) : 0);
        ++count;
    }
    EXPECT_EQ(count, 40u);                  // every handshake message of the five traces
    EXPECT_GE(extensions_compared, 60u);    // every extension of theirs v1 reads, compared byte for byte
    std::printf("%zu messages, %zu extension bodies compared\n", count, extensions_compared);
}

TEST(TlsMessages_Tests, TheFieldsOfTheSimpleHandshake) {
    auto ch = find(3, "client", "ClientHello");
    auto sh = find(3, "server", "ServerHello");
    if (!ch || !sh) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    auto h = tls::read_handshake(view(ch->bytes));
    ASSERT_TRUE(h);
    EXPECT_EQ(h->type, uint8_t(tls::HandshakeType::client_hello));
    auto m = tls::read_client_hello(h->body);
    ASSERT_TRUE(m);
    EXPECT_EQ(m->legacy_version, tls::Tls12);
    EXPECT_TRUE(m->session_id.empty());
    EXPECT_EQ(std::vector<uint16_t>(m->cipher_suites.begin(), m->cipher_suites.end()), (std::vector<uint16_t>{0x1301, 0x1303, 0x1302}));
    auto sni = m->extensions.find(tls::ExtensionType::server_name);
    ASSERT_TRUE(sni);
    auto host = tls::read_server_name(*sni);
    ASSERT_TRUE(host);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(host->data()), host->size()), "server");
    auto groups = tls::read_groups(*m->extensions.find(tls::ExtensionType::supported_groups));
    ASSERT_TRUE(groups);
    EXPECT_EQ(std::vector<uint16_t>(groups->begin(), groups->end()), (std::vector<uint16_t>{0x001D, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101, 0x0102, 0x0103, 0x0104}));
    auto versions = tls::read_versions_offered(*m->extensions.find(tls::ExtensionType::supported_versions));
    ASSERT_TRUE(versions);
    EXPECT_EQ(std::vector<uint16_t>(versions->begin(), versions->end()), (std::vector<uint16_t>{tls::Tls13}));
    auto shares = tls::read_key_shares(*m->extensions.find(tls::ExtensionType::key_share));
    ASSERT_TRUE(shares);
    auto x = shares->find(0x001D);
    ASSERT_TRUE(x);
    EXPECT_EQ(of(*x), unhex("99381de560e4bd43d23d8e435a7dbafeb3c06e51c13cae4d5413691e529aaf2c"));
    EXPECT_TRUE(m->extensions.has(tls::ExtensionType::psk_key_exchange_modes));
    EXPECT_TRUE(tls::validate_extensions(tls::HandshakeType::client_hello, false, m->extensions, 0));

    auto s = tls::read_server_hello(tls::read_handshake(view(sh->bytes))->body);
    ASSERT_TRUE(s);
    EXPECT_FALSE(s->is_retry());
    EXPECT_EQ(s->cipher_suite, 0x1301);
    auto share = tls::read_key_share_selected(*s->extensions.find(tls::ExtensionType::key_share));
    ASSERT_TRUE(share);
    EXPECT_EQ(share->group, 0x001D);
    EXPECT_EQ(of(share->key), unhex("c9828876112095fe66762bdbf7c672e156d6cc253b833df1dd69b1b04e751f0f"));
    EXPECT_EQ(tls::read_version_selected(*s->extensions.find(tls::ExtensionType::supported_versions)).value(), tls::Tls13);
    EXPECT_TRUE(tls::validate_extensions(tls::HandshakeType::server_hello, false, s->extensions, m->extensions.mask));
}

TEST(TlsMessages_Tests, TheHelloRetryRequestOfRfc8448) {
    auto hrr = find(5, "server", "ServerHello", 0);
    auto ch2 = find(5, "client", "ClientHello", 1);
    if (!hrr || !ch2) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    auto s = tls::read_server_hello(tls::read_handshake(view(hrr->bytes))->body);
    ASSERT_TRUE(s);
    EXPECT_TRUE(s->is_retry());
    EXPECT_EQ(tls::read_key_share_retry(*s->extensions.find(tls::ExtensionType::key_share)).value(), uint16_t(tls::Group::secp256r1));
    auto cookie = tls::read_cookie(*s->extensions.find(tls::ExtensionType::cookie));
    ASSERT_TRUE(cookie);
    EXPECT_EQ(cookie->size(), 0x72u);
    // the cookie needs no offer; the rest of a retry was offered by the first ClientHello
    uint64_t offered = tls::bit_of(tls::ExtensionType::key_share) | tls::bit_of(tls::ExtensionType::supported_versions);
    EXPECT_TRUE(tls::validate_extensions(tls::HandshakeType::server_hello, true, s->extensions, offered));

    auto m = tls::read_client_hello(tls::read_handshake(view(ch2->bytes))->body);
    ASSERT_TRUE(m);
    auto shares = tls::read_key_shares(*m->extensions.find(tls::ExtensionType::key_share));
    ASSERT_TRUE(shares);
    auto p256 = shares->find(uint16_t(tls::Group::secp256r1));
    ASSERT_TRUE(p256);
    EXPECT_EQ(p256->size(), 65u);
    EXPECT_EQ(uint8_t((*p256)[0]), 4);
    auto echoed = tls::read_cookie(*m->extensions.find(tls::ExtensionType::cookie));
    ASSERT_TRUE(echoed);
    EXPECT_EQ(of(*echoed), of(*cookie));
}

// CertificateVerify of §3: rsa_pss_rsae_sha256 under the certificate of §3,
// over the hash of ClientHello … Certificate
TEST(TlsMessages_Tests, TheCertificateVerifyOfRfc8448Verifies) {
    auto ch = find(3, "client", "ClientHello");
    auto sh = find(3, "server", "ServerHello");
    auto ee = find(3, "server", "EncryptedExtensions");
    auto ct = find(3, "server", "Certificate");
    auto cv = find(3, "server", "CertificateVerify");
    if (!ch || !sh || !ee || !ct || !cv) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    tls::Transcript t(tls::Hash::sha256);
    for (auto* m : {ch, sh, ee, ct}) {
        t.update(view(m->bytes));
    }
    uint8_t hash[32];
    t.value_to(hash);
    auto certificate = tls::read_certificate(tls::read_handshake(view(ct->bytes))->body);
    ASSERT_TRUE(certificate);
    EXPECT_EQ(certificate->count, 1u);
    auto leaf = *certificate->begin();
    auto cert = sgcl::crypto::x509::certificate::parse(leaf.der);
    ASSERT_TRUE(cert);
    auto verify = tls::read_certificate_verify(tls::read_handshake(view(cv->bytes))->body);
    ASSERT_TRUE(verify);
    EXPECT_EQ(verify->scheme, uint16_t(tls::SignatureScheme::rsa_pss_rsae_sha256));
    auto content = tls::certificate_verify_content(true, tls::bytes_of(hash, 32));
    EXPECT_TRUE(tls::verify(verify->scheme, cert->public_key(), content.view(), verify->signature));

    // the client's context string, a byte of the signature changed, a
    // PKCS #1 scheme, an ECDSA scheme on an RSA key
    auto client = tls::certificate_verify_content(false, tls::bytes_of(hash, 32));
    EXPECT_EQ(alert_of(tls::verify(verify->scheme, cert->public_key(), client.view(), verify->signature)), AlertDescription::decrypt_error);
    bytes_t bad = of(verify->signature);
    bad[10] ^= 1;
    EXPECT_EQ(alert_of(tls::verify(verify->scheme, cert->public_key(), content.view(), view(bad))), AlertDescription::decrypt_error);
    EXPECT_EQ(alert_of(tls::verify(uint16_t(tls::SignatureScheme::rsa_pkcs1_sha256), cert->public_key(), content.view(), verify->signature)), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(tls::verify(uint16_t(tls::SignatureScheme::ecdsa_secp256r1_sha256), cert->public_key(), content.view(), verify->signature)), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(tls::verify(0x0808, cert->public_key(), content.view(), verify->signature)), AlertDescription::illegal_parameter);
}

// Signed by the module's own keys and verified back, every scheme of v1
TEST(TlsMessages_Tests, EverySchemeSignsAndVerifies) {
    uint8_t hash[48] = {7};
    auto content = tls::certificate_verify_content(true, tls::bytes_of(hash, 48));
    auto check = [&](uint16_t scheme, const auto& key, const auto& pub) {
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        tls::sign(w, scheme, key, content.view());
        EXPECT_TRUE(tls::verify(scheme, pub, content.view(), tls::bytes_of(out.data(), out.size()))) << scheme;
    };
    auto ed = sgcl::crypto::ed25519::private_key::generate();
    check(0x0807, ed, ed.public_key());
    auto p256 = sgcl::crypto::p256::private_key::generate();
    check(0x0403, p256, p256.public_key());
    auto p384 = sgcl::crypto::p384::private_key::generate();
    check(0x0503, p384, p384.public_key());
    auto rsa = sgcl::crypto::rsa::private_key::generate(2048);
    for (uint16_t s : {0x0804, 0x0805, 0x0806}) {
        check(s, rsa, rsa.public_key());
    }
}

// --- refusals -------------------------------------------------------------------

// Every message of the RFC cut at every byte: decode_error, never a read
// past the end
TEST(TlsMessages_Tests, EveryTruncationIsADecodeError) {
    if (messages().empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    for (auto& m : messages()) {
        auto body = body_of(m.bytes);
        for (size_t n = 0; n < body.size(); ++n) {
            bytes_t cut(body.begin(), body.begin() + n);
            AlertDescription got = AlertDescription::close_notify;
            switch (tls::HandshakeType(m.bytes[0])) {
                case tls::HandshakeType::client_hello: got = alert_of(tls::read_client_hello(view(cut))); break;
                case tls::HandshakeType::server_hello: got = alert_of(tls::read_server_hello(view(cut))); break;
                case tls::HandshakeType::encrypted_extensions: got = alert_of(tls::read_encrypted_extensions(view(cut))); break;
                case tls::HandshakeType::certificate_request: got = alert_of(tls::read_certificate_request(view(cut))); break;
                case tls::HandshakeType::certificate: got = alert_of(tls::read_certificate(view(cut))); break;
                case tls::HandshakeType::certificate_verify: got = alert_of(tls::read_certificate_verify(view(cut))); break;
                case tls::HandshakeType::finished: got = alert_of(tls::read_finished(view(cut), 32)); break;
                case tls::HandshakeType::new_session_ticket: got = alert_of(tls::read_new_session_ticket(view(cut))); break;
                default: got = AlertDescription::decode_error; break;
            }
            // a ServerHello cut right after its compression method is a
            // TLS 1.2 ServerHello without extensions (RFC 5246 §7.4.1.3)
            if (tls::HandshakeType(m.bytes[0]) == tls::HandshakeType::server_hello && n == size_t(2 + 32 + 1 + body[34] + 2 + 1)) {
                EXPECT_EQ(got, AlertDescription::close_notify) << m.name;
                continue;
            }
            // an EncryptedExtensions cut to nothing is an EncryptedExtensions
            // without extensions only when its length is gone too: it is not
            EXPECT_EQ(got, AlertDescription::decode_error) << m.name << " cut to " << n << " of " << body.size();
            // and the header claims the whole: bytes missing
            bytes_t message(m.bytes.begin(), m.bytes.begin() + 4 + n);
            EXPECT_EQ(alert_of(tls::read_handshake(view(message))), AlertDescription::decode_error);
        }
    }
}

TEST(TlsMessages_Tests, TheHeaderAndItsLength) {
    // a length of 16 MiB with four bytes: refused at once, nothing read
    auto huge = unhex("01ffffff");
    auto h = tls::read_handshake(view(huge));
    ASSERT_FALSE(h);
    EXPECT_EQ(h.error().description, AlertDescription::decode_error);
    // a byte past the length
    auto past = unhex("140000010000");
    EXPECT_EQ(alert_of(tls::read_handshake(view(past))), AlertDescription::decode_error);
    // bytes after the last field of a body
    auto kv = unhex("0000");
    EXPECT_EQ(alert_of(tls::read_key_update(view(kv))), AlertDescription::decode_error);
}

TEST(TlsMessages_Tests, TheClientHelloRefusals) {
    // ours, as written, reads
    auto good = client_hello([](tls::Builder& w) { x25519_share(w); });
    EXPECT_TRUE(tls::read_client_hello(view(body_of(good))));

    // an extension twice
    auto twice = client_hello([](tls::Builder& w) {
        x25519_share(w);
        auto e = w.extension(tls::ExtensionType::cookie);
        uint8_t c[1] = {1};
        tls::write_cookie(w, tls::bytes_of(c, 1));
        e.close();
        auto f = w.extension(tls::ExtensionType::cookie);
        tls::write_cookie(w, tls::bytes_of(c, 1));
    });
    auto r = tls::read_client_hello(view(body_of(twice)));
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().description, AlertDescription::illegal_parameter);

    // pre_shared_key before another extension
    auto psk = client_hello([](tls::Builder& w) {
        {
            auto e = w.extension(tls::ExtensionType::pre_shared_key);
            bytes_t body = unhex("0007" "000161" "00000000" "0021" "20");   // one identity, one binder of 32 bytes
            body.insert(body.end(), 32, 0x20);
            w.bytes(body.data(), body.size());
        }
        x25519_share(w);
    });
    EXPECT_EQ(alert_of(tls::read_client_hello(view(body_of(psk)))), AlertDescription::illegal_parameter);

    // compression [1] and [0, 1]; legacy_version 0x0301
    auto body = body_of(good);
    size_t compression = 2 + 32 + 1 + 2 + 2;   // after legacy_version, random, session_id, suites
    ASSERT_EQ(body[compression], 1);
    auto one = body;
    one[compression + 1] = 1;
    EXPECT_EQ(alert_of(tls::read_client_hello(view(one))), AlertDescription::illegal_parameter);
    auto two = body;
    two[compression] = 2;
    two.insert(two.begin() + long(compression) + 2, 1);
    EXPECT_EQ(alert_of(tls::read_client_hello(view(two))), AlertDescription::illegal_parameter);
    auto old = body;
    old[1] = 0x01;
    EXPECT_EQ(alert_of(tls::read_client_hello(view(old))), AlertDescription::illegal_parameter);

    // cipher suites of an odd length; a session id of 33 bytes
    auto odd = body;
    odd[2 + 32 + 1 + 1] = 3;
    EXPECT_EQ(alert_of(tls::read_client_hello(view(odd))), AlertDescription::decode_error);
    auto long_id = body;
    long_id[2 + 32] = 33;
    EXPECT_EQ(alert_of(tls::read_client_hello(view(long_id))), AlertDescription::decode_error);
}

TEST(TlsMessages_Tests, KeyShareRefusals) {
    auto shares = [](std::vector<std::pair<uint16_t, bytes_t>> list) {
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        std::vector<tls::KeyShare> s;   // lint-handles: ok slices over unmanaged bytes, no owner
        for (auto& [g, k] : list) {
            s.push_back({g, view(k)});
        }
        tls::write_key_shares(w, s);
        return of(out);
    };
    bytes_t x(32, 9), point(65, 1), mlkem(1216, 3);
    point[0] = 4;
    EXPECT_TRUE(tls::read_key_shares(view(shares({{0x001D, x}, {0x0017, point}, {0x11EC, mlkem}, {0x4a4a, bytes_t(1, 0)}}))));
    EXPECT_EQ(alert_of(tls::read_key_shares(view(shares({{0x001D, bytes_t(31, 9)}})))), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(tls::read_key_shares(view(shares({{0x001D, x}, {0x001D, x}})))), AlertDescription::illegal_parameter);
    auto compressed = point;
    compressed[0] = 2;
    EXPECT_EQ(alert_of(tls::read_key_shares(view(shares({{0x0017, compressed}})))), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(tls::read_key_shares(view(shares({{0x11EC, bytes_t(1120, 3)}})))), AlertDescription::illegal_parameter);   // the server's size from a client
    // the server's: one share, of the server's size
    std::vector<sgcl::byte> out;
    tls::Builder w(out);
    tls::write_key_share_selected(w, {0x11EC, view(bytes_t(1120, 3))});
    EXPECT_TRUE(tls::read_key_share_selected(tls::bytes_of(out.data(), out.size())));
}

TEST(TlsMessages_Tests, TheOtherRefusals) {
    EXPECT_EQ(alert_of(tls::read_key_update(view(unhex("02")))), AlertDescription::illegal_parameter);
    EXPECT_TRUE(tls::read_key_update(view(unhex("01"))).value());
    // a ticket of seven days and a second
    auto nst = unhex("00093a81" "00000000" "00" "0001" "01" "0000");   // lifetime, age_add, nonce, ticket, extensions
    EXPECT_EQ(alert_of(tls::read_new_session_ticket(view(nst))), AlertDescription::illegal_parameter);
    nst[3] = 0x80;
    EXPECT_TRUE(tls::read_new_session_ticket(view(nst)));
    // a Finished of another length
    EXPECT_EQ(alert_of(tls::read_finished(view(bytes_t(31)), 32)), AlertDescription::decode_error);
    // CertificateRequest without signature_algorithms
    EXPECT_EQ(alert_of(tls::read_certificate_request(view(unhex("00" "0004" "002c0000")))), AlertDescription::missing_extension);
    // two host names in server_name
    EXPECT_EQ(alert_of(tls::read_server_name(view(unhex("000a" "000001" "61" "0000" "01" "62")))), AlertDescription::decode_error);
    EXPECT_EQ(alert_of(tls::read_server_name(view(unhex("0008" "00000161" "00000162")))), AlertDescription::illegal_parameter);
    // a reply with two protocols
    EXPECT_EQ(alert_of(tls::read_protocol_selected(view(unhex("0006" "026832" "026833")))), AlertDescription::illegal_parameter);
    // an alert record: its two bytes, and the level not read
    auto a = tls::read_alert(view(unhex("0228")));
    ASSERT_TRUE(a);
    EXPECT_EQ(a->description, AlertDescription::handshake_failure);
    EXPECT_TRUE(a->fatal());
    EXPECT_FALSE(tls::read_alert(view(unhex("0100"))).value().fatal());
    EXPECT_EQ(alert_of(tls::read_alert(view(unhex("022800")))), AlertDescription::decode_error);
}

TEST(TlsMessages_Tests, WhichExtensionsAMessageMayCarry) {
    auto x = [](std::vector<uint16_t> types) {
        static std::vector<std::vector<sgcl::byte>> keep;   // the views of the result point into it
        keep.emplace_back();
        tls::Builder w(keep.back());
        for (auto t : types) {
            auto e = w.extension(t);
        }
        std::vector<sgcl::byte> whole;
        tls::Builder list(whole);
        {
            auto b = list.block16();
            list.bytes(keep.back().data(), keep.back().size());
        }
        keep.push_back(whole);
        tls::Reader r(tls::bytes_of(keep.back().data(), keep.back().size()));
        return tls::read_extensions(r, 0, 0xFFFF).value();
    };
    using HT = tls::HandshakeType;
    uint64_t offered = tls::bit_of(tls::ExtensionType::server_name) | tls::bit_of(tls::ExtensionType::key_share) | tls::bit_of(tls::ExtensionType::supported_versions);
    // allowed and offered
    EXPECT_TRUE(tls::validate_extensions(HT::encrypted_extensions, false, x({0}), offered));
    // ALPN not offered
    EXPECT_EQ(alert_of(tls::validate_extensions(HT::encrypted_extensions, false, x({16}), offered)), AlertDescription::unsupported_extension);
    // key_share in EncryptedExtensions: known, and not of that message
    EXPECT_EQ(alert_of(tls::validate_extensions(HT::encrypted_extensions, false, x({51}), offered)), AlertDescription::illegal_parameter);
    // an unknown type: passed over in a ClientHello, unsupported in a reply
    EXPECT_TRUE(tls::validate_extensions(HT::client_hello, false, x({0x1234, 0xff01}), 0));
    EXPECT_EQ(alert_of(tls::validate_extensions(HT::server_hello, false, x({0x1234}), offered)), AlertDescription::unsupported_extension);
    // cookie: the server's own in a retry, not of a ServerHello
    EXPECT_TRUE(tls::validate_extensions(HT::server_hello, true, x({43, 44, 51}), offered));
    EXPECT_EQ(alert_of(tls::validate_extensions(HT::server_hello, false, x({44}), offered)), AlertDescription::illegal_parameter);
    // status_request in a certificate entry, not asked for
    EXPECT_EQ(alert_of(tls::validate_extensions(HT::certificate, false, x({5}), offered)), AlertDescription::unsupported_extension);
    // an unknown type in a NewSessionTicket and a CertificateRequest: passed over
    EXPECT_TRUE(tls::validate_extensions(HT::new_session_ticket, false, x({0x1234}), 0));
    EXPECT_TRUE(tls::validate_extensions(HT::certificate_request, false, x({13, 0x1234}), 0));
}

TEST(TlsMessages_Tests, AMessageWrittenIsAMessageRead) {
    // what the machines write, read back
    std::vector<sgcl::byte> out;
    tls::Builder w(out);
    uint8_t random[32] = {5};
    uint8_t id[32] = {6};
    tls::write_server_hello(w, tls::bytes_of(random, 32), tls::bytes_of(id, 32), 0x1302, [](tls::Builder& b) {
        {
            auto e = b.extension(tls::ExtensionType::supported_versions);
            tls::write_version_selected(b, tls::Tls13);
        }
        auto e = b.extension(tls::ExtensionType::key_share);
        uint8_t k[32] = {8};
        tls::write_key_share_selected(b, {0x001D, tls::bytes_of(k, 32)});
    });
    auto h = tls::read_handshake(tls::bytes_of(out.data(), out.size()));
    ASSERT_TRUE(h);
    auto s = tls::read_server_hello(h->body);
    ASSERT_TRUE(s);
    EXPECT_EQ(s->cipher_suite, 0x1302);
    EXPECT_EQ(s->session_id.size(), 32u);
    EXPECT_EQ(s->extensions.count, 2u);

    // a retry
    std::vector<sgcl::byte> retry;
    tls::Builder r(retry);
    tls::write_server_hello(r, tls::bytes_of(tls::HelloRetryRandom, 32), tls::Bytes(), 0x1301, [](tls::Builder& b) {
        auto e = b.extension(tls::ExtensionType::key_share);
        tls::write_key_share_retry(b, uint16_t(tls::Group::x25519_mlkem768));
    });
    auto rs = tls::read_server_hello(tls::read_handshake(tls::bytes_of(retry.data(), retry.size()))->body);
    ASSERT_TRUE(rs);
    EXPECT_TRUE(rs->is_retry());

    // message_hash: 254, the length, the hash
    std::vector<sgcl::byte> mh;
    tls::Builder m(mh);
    uint8_t hash[32] = {1};
    tls::write_message_hash(m, tls::bytes_of(hash, 32));
    EXPECT_EQ(mh.size(), 36u);
    EXPECT_EQ(uint8_t(mh[0]), 254);
    EXPECT_EQ(uint8_t(mh[3]), 32);

    // an alert: fatal but close_notify and user_canceled
    std::vector<sgcl::byte> al;
    tls::Builder a(al);
    tls::write_alert(a, AlertDescription::close_notify);
    tls::write_alert(a, AlertDescription::decode_error);
    EXPECT_EQ(of(al), unhex("01000232"));
}
