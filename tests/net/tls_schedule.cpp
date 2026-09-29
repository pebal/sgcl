//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The key schedule of TLS 1.3 (sgcl/net/tls/detail/schedule.h) against
// three oracles:
//
//   - RFC 8448 §3, the simple 1-RTT handshake, read from
//     ~/Programming/oracles/rfc8448/rfc8448.txt (or $SGCL_ORACLES): every
//     Derive-Secret, extract and key derivation of its trace recomputed
//     from its own inputs, the transcript hashes from its handshake
//     messages, both Finished, and the whole chain run from the shared
//     secret alone;
//   - OpenSSL's TLS13-KDF on random inputs, both hashes, a shared secret of
//     32 bytes (X25519) and of 64 (X25519MLKEM768) (tls_schedule_vectors.h,
//     tools/tls_schedule_oracle.cpp);
//   - real handshakes of OpenSSL with itself, the three cipher suites with
//     X25519 and X25519MLKEM768: the transcript of their messages and the
//     handshake traffic secrets of the key log give both Finished.
#include "tests/types.h"

#include "sgcl/net/tls/detail/schedule.h"
#include "tls_rfc8448.h"
#include "tls_schedule_vectors.h"

#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;

    bytes_t unhex(const std::string& s) {
        bytes_t v;
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            v.push_back(uint8_t(std::stoi(s.substr(i, 2), nullptr, 16)));
        }
        return v;
    }

    sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    bytes_t of(const tls::Secret& s) {
        return bytes_t(s.bytes, s.bytes + s.size);
    }

    tls::Hash hash_of(int bits) {
        return bits == 256 ? tls::Hash::sha256 : tls::Hash::sha384;
    }

    bytes_t transcript_of(tls::Hash h, const std::vector<bytes_t>& messages) {
        tls::Transcript t(h);
        for (auto& m : messages) {
            t.update(view(m));
        }
        bytes_t out(t.size());
        t.value_to(out.data());
        return out;
    }

    // --- RFC 8448 -------------------------------------------------------------

    using rfc8448::Step;
    using rfc8448::find;

    std::vector<Step> read_rfc8448() {
        return rfc8448::read();
    }

    // The label of a "derive secret "tls13 xxx"" step: xxx
    std::string label_of(const std::string& title) {
        size_t a = title.find("\"tls13 ");
        size_t b = title.find('"', a + 1);
        return title.substr(a + 7, b - a - 7);
    }
}

// Every derivation of §3's trace from its own inputs
TEST(TlsSchedule, Rfc8448EveryStep) {
    auto steps = read_rfc8448();
    if (steps.empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    size_t derived = 0, extracted = 0, keyed = 0;
    for (auto& s : steps) {
        if (s.section != 3) {
            continue;
        }
        auto& f = s.fields;
        if (s.title.rfind("derive secret", 0) == 0 && f.count("PRK") && f.count("expanded")) {
            tls::Secret prk;
            prk.size = uint8_t(f["PRK"].size());
            std::memcpy(prk.bytes, f["PRK"].data(), prk.size);
            tls::Secret out;
            tls::derive_secret(tls::Hash::sha256, out, prk, label_of(s.title).c_str(), view(f["hash"]));
            EXPECT_EQ(of(out), f["expanded"]) << s.who << " " << s.title;
            ++derived;
        } else if (s.title.rfind("extract secret", 0) == 0 && f.count("IKM") && f.count("secret")) {
            tls::Secret out;
            tls::extract(tls::Hash::sha256, out, view(f["salt"]), view(f["IKM"]));
            EXPECT_EQ(of(out), f["secret"]) << s.who << " " << s.title;
            ++extracted;
        } else if (s.title.find("traffic keys") != std::string::npos && f.count("PRK") && f.count("key expanded")) {
            tls::Secret prk;
            prk.size = uint8_t(f["PRK"].size());
            std::memcpy(prk.bytes, f["PRK"].data(), prk.size);
            tls::TrafficKeys keys;
            tls::traffic_keys(tls::Hash::sha256, keys, prk, 16);
            EXPECT_EQ(bytes_t(keys.key, keys.key + 16), f["key expanded"]) << s.who << " " << s.title;
            EXPECT_EQ(bytes_t(keys.iv, keys.iv + 12), f["iv expanded"]) << s.who << " " << s.title;
            ++keyed;
        }
    }
    EXPECT_GE(derived, 8u);    // derived (twice), c/s hs traffic, c/s ap traffic, exp master, res master
    EXPECT_GE(extracted, 3u);  // early, handshake, master
    EXPECT_GE(keyed, 3u);      // handshake write, application write (both sides)
}

// The transcript of the trace's messages, the chain from the shared secret
// alone, and both Finished
TEST(TlsSchedule, Rfc8448TheWholeChain) {
    auto steps = read_rfc8448();
    if (steps.empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    auto message = [&](const std::string& who, const char* title, const char* field) {
        auto s = find(steps, 3, who, title);
        EXPECT_TRUE(s) << title;
        return s ? s->fields.at(field) : bytes_t();
    };
    bytes_t ch = message("client", "construct a ClientHello", "ClientHello");
    bytes_t sh = message("server", "construct a ServerHello", "ServerHello");
    bytes_t ee = message("server", "construct an EncryptedExtensions", "EncryptedExtensions");
    bytes_t cert = message("server", "construct a Certificate", "Certificate");
    bytes_t cv = message("server", "construct a CertificateVerify", "CertificateVerify");
    bytes_t sfin = message("server", "construct a Finished", "Finished");
    bytes_t shared = message("server", "extract secret \"handshake\"", "IKM");
    auto chs = find(steps, 3, "server", "derive secret \"tls13 c hs traffic\"");
    auto shs = find(steps, 3, "server", "derive secret \"tls13 s hs traffic\"");
    auto cap = find(steps, 3, "server", "derive secret \"tls13 c ap traffic\"");
    auto sap = find(steps, 3, "server", "derive secret \"tls13 s ap traffic\"");
    auto exp = find(steps, 3, "server", "derive secret \"tls13 exp master\"");
    auto master = find(steps, 3, "server", "extract secret \"master\"");
    auto server_finished = find(steps, 3, "server", "calculate finished");
    auto client_finished = find(steps, 3, "client", "calculate finished \"tls13 finished\":");
    ASSERT_TRUE(chs && shs && cap && sap && exp && master && server_finished && client_finished);

    // the transcript hashes the trace derives with
    bytes_t hello = transcript_of(tls::Hash::sha256, {ch, sh});
    EXPECT_EQ(hello, chs->fields.at("hash"));
    bytes_t through_server_finished = transcript_of(tls::Hash::sha256, {ch, sh, ee, cert, cv, sfin});
    EXPECT_EQ(through_server_finished, cap->fields.at("hash"));

    // the chain
    tls::KeySchedule k(tls::Hash::sha256);
    k.handshake(view(shared), view(hello));
    EXPECT_EQ(of(k.client_handshake_traffic), chs->fields.at("expanded"));
    EXPECT_EQ(of(k.server_handshake_traffic), shs->fields.at("expanded"));
    k.application(view(through_server_finished));
    EXPECT_EQ(of(k.master_secret()), master->fields.at("secret"));
    EXPECT_EQ(of(k.client_application_traffic), cap->fields.at("expanded"));
    EXPECT_EQ(of(k.server_application_traffic), sap->fields.at("expanded"));
    EXPECT_EQ(of(k.exporter_master), exp->fields.at("expanded"));
    EXPECT_TRUE(k.early_secret().empty());       // gone once used
    EXPECT_TRUE(k.handshake_secret().empty());

    // the server's Finished: over ClientHello..CertificateVerify; the
    // client's: over ClientHello..the server's Finished
    uint8_t verify[48];
    tls::verify_data(tls::Hash::sha256, verify, k.server_handshake_traffic, view(transcript_of(tls::Hash::sha256, {ch, sh, ee, cert, cv})));
    EXPECT_EQ(bytes_t(verify, verify + 32), server_finished->fields.at("finished"));
    EXPECT_EQ(bytes_t(sfin.begin() + 4, sfin.end()), server_finished->fields.at("finished"));
    tls::verify_data(tls::Hash::sha256, verify, k.client_handshake_traffic, view(through_server_finished));
    EXPECT_EQ(bytes_t(verify, verify + 32), client_finished->fields.at("finished"));

    k.finish_handshake();
    EXPECT_TRUE(k.client_handshake_traffic.empty());
    EXPECT_TRUE(k.master_secret().empty());
}

// OpenSSL's TLS13-KDF on random inputs, both hashes, both sizes of shared secret
TEST(TlsSchedule, AgainstOpenSslOnRandomInputs) {
    size_t n = 0;
    for (auto& c : tls_vectors::chains) {
        tls::Hash h = hash_of(c.hash);
        tls::KeySchedule k(h);
        EXPECT_EQ(of(k.early_secret()), unhex(c.early));
        k.handshake(view(unhex(c.shared)), view(unhex(c.hello_hash)));
        EXPECT_EQ(of(k.handshake_secret()), unhex(c.handshake));
        EXPECT_EQ(of(k.client_handshake_traffic), unhex(c.client_hs));
        EXPECT_EQ(of(k.server_handshake_traffic), unhex(c.server_hs));
        k.application(view(unhex(c.server_finished_hash)));
        EXPECT_EQ(of(k.master_secret()), unhex(c.master));
        EXPECT_EQ(of(k.client_application_traffic), unhex(c.client_ap));
        EXPECT_EQ(of(k.server_application_traffic), unhex(c.server_ap));
        EXPECT_EQ(of(k.exporter_master), unhex(c.exporter));
        size_t key_size = c.hash == 256 ? 16 : 32;
        tls::TrafficKeys server_hs, client_ap;
        tls::traffic_keys(h, server_hs, k.server_handshake_traffic, key_size);
        EXPECT_EQ(bytes_t(server_hs.key, server_hs.key + key_size), unhex(c.server_hs_key));
        EXPECT_EQ(bytes_t(server_hs.iv, server_hs.iv + 12), unhex(c.server_hs_iv));
        tls::traffic_keys(h, client_ap, k.client_application_traffic, 32);   // ChaCha20's 32 bytes under SHA-256 too
        EXPECT_EQ(bytes_t(client_ap.key, client_ap.key + 32), unhex(c.client_ap_key32));
        EXPECT_EQ(bytes_t(client_ap.iv, client_ap.iv + 12), unhex(c.client_ap_iv));
        uint8_t verify[48];
        tls::verify_data(h, verify, k.server_handshake_traffic, view(unhex(c.server_finished_hash)));
        EXPECT_EQ(bytes_t(verify, verify + k.hash_length()), unhex(c.server_verify));
        tls::verify_data(h, verify, k.client_handshake_traffic, view(unhex(c.client_finished_hash)));
        EXPECT_EQ(bytes_t(verify, verify + k.hash_length()), unhex(c.client_verify));
        tls::update_traffic_secret(h, k.server_application_traffic);
        EXPECT_EQ(of(k.server_application_traffic), unhex(c.server_ap_updated));
        ++n;
    }
    EXPECT_EQ(n, 32u);
}

// Real handshakes of OpenSSL with itself: the transcript of their messages
// and the key log's handshake traffic secrets give both Finished
TEST(TlsSchedule, TheFinishedOfOpenSslHandshakes) {
    size_t checked = 0;
    for (auto& hs : tls_vectors::handshakes) {
        tls::Hash h = hash_of(hs.hash);
        tls::Secret client_hs, server_hs;
        auto set = [](tls::Secret& s, const char* text) {
            auto b = unhex(text);
            s.size = uint8_t(b.size());
            std::memcpy(s.bytes, b.data(), b.size());
        };
        set(client_hs, hs.client_hs);
        set(server_hs, hs.server_hs);
        tls::Transcript t(h);
        int finished_seen = 0;
        for (auto& m : hs.messages) {
            bytes_t msg = unhex(m.bytes);
            ASSERT_GE(msg.size(), 4u);
            if (msg[0] == 20) {   // Finished: its verify_data over the transcript before it
                bytes_t before(t.size());
                t.value_to(before.data());
                uint8_t verify[48];
                tls::verify_data(h, verify, m.client ? client_hs : server_hs, view(before));
                EXPECT_EQ(bytes_t(verify, verify + t.size()), bytes_t(msg.begin() + 4, msg.end())) << hs.suite << " " << hs.group << (m.client ? " client" : " server");
                ++finished_seen;
            }
            t.update(view(msg));
        }
        EXPECT_EQ(finished_seen, 2) << hs.suite << " " << hs.group;
        ++checked;
    }
    EXPECT_EQ(checked, 6u);
}

// The labels and lengths HKDF-Expand-Label writes (§7.1): the info of
// "tls13 key" for 16 bytes, as RFC 8448 prints it
TEST(TlsSchedule, TheLabelsInfo) {
    tls::Secret s;
    s.size = 32;
    for (int i = 0; i < 32; ++i) {
        s.bytes[i] = uint8_t(i);
    }
    uint8_t a[16], b[16];
    tls::expand_label(tls::Hash::sha256, tls::room_of(a, 16), s.view(), "key", sgcl::slice<const sgcl::byte>());
    // HKDF-Expand with the info spelled out by hand: 00 10 09 "tls13 key" 00
    const uint8_t info[] = {0x00, 0x10, 0x09, 't', 'l', 's', '1', '3', ' ', 'k', 'e', 'y', 0x00};
    sgcl::crypto::hkdf<sgcl::crypto::sha256>::expand_to(tls::room_of(b, 16), s.view(), tls::bytes_of(info, sizeof info));
    EXPECT_EQ(std::memcmp(a, b, 16), 0);
}
