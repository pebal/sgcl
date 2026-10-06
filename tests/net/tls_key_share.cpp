//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The key shares of TLS 1.3 (sgcl/net/tls/detail/key_share.h) against:
//
//   - RFC 8448: the X25519 key pairs of §3 and the P-256 ones of §5 (the
//     HelloRetryRequest), each side's share from its private key and the
//     shared secret the trace extracts the handshake secret from;
//   - OpenSSL (tls_key_share_vectors.h, tools/tls_key_share_oracle.cpp):
//     X25519, P-256 and P-384 key exchanges, and X25519MLKEM768 — the
//     client's share from its ML-KEM seed and X25519 key, and OpenSSL's
//     hybrid group encapsulating to it, which fixes the order of the parts
//     of both shares and of the secret;
//   - each other, both sides of every group, and the shares that are not
//     valid ones.
#include "tests/types.h"

#include "sgcl/net/tls/detail/key_share.h"
#include "tls_key_share_vectors.h"
#include "tls_rfc8448.h"

#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;
    using tls::Group;

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

    bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

    bytes_t of(const tls::SharedSecret& s) {
        return bytes_t(s.bytes, s.bytes + s.size);
    }

    // An entropy that gives the bytes it holds, in order
    struct Queue {
        bytes_t data;
        size_t at = 0;
        bool short_of_bytes = false;

        static void fill(void* self, uint8_t* out, size_t n) {
            auto& q = *static_cast<Queue*>(self);
            for (size_t i = 0; i < n; ++i) {
                if (q.at < q.data.size()) {
                    out[i] = q.data[q.at++];
                } else {
                    out[i] = 0;
                    q.short_of_bytes = true;
                }
            }
        }

        tls::Entropy entropy() {
            return tls::Entropy{&Queue::fill, this};
        }

        bool all_used() const {
            return at == data.size() && !short_of_bytes;
        }
    };

    Group group_of(const std::string& name) {
        if (name == "secp256r1") {
            return Group::secp256r1;
        }
        if (name == "secp384r1") {
            return Group::secp384r1;
        }
        if (name == "secp521r1") {
            return Group::secp521r1;
        }
        return Group::x25519;
    }

    tls::AlertDescription alert_of(const sgcl::expected<tls::SharedSecret, tls::Alert>& r) {
        EXPECT_FALSE(r.has_value());
        return r ? tls::AlertDescription::close_notify : r.error().description;
    }

    tls::AlertDescription alert_of(const sgcl::expected<tls::ServerShare, tls::Alert>& r) {
        EXPECT_FALSE(r.has_value());
        return r ? tls::AlertDescription::close_notify : r.error().description;
    }
}

// RFC 8448 §3 (X25519) and §5 (P-256 after the HelloRetryRequest)
TEST(TlsKeyShare, Rfc8448) {
    auto steps = rfc8448::read();
    if (steps.empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    struct Case {
        int section;
        Group group;
        const char* pair;
    };
    for (auto c : {Case{3, Group::x25519, "create an ephemeral x25519 key pair"}, Case{5, Group::secp256r1, "create an ephemeral P-256 key pair"}}) {
        SCOPED_TRACE(c.section);
        auto client = rfc8448::find(steps, c.section, "client", c.pair);
        auto server = rfc8448::find(steps, c.section, "server", c.pair);
        auto extract = rfc8448::find(steps, c.section, "server", "extract secret \"handshake\"");
        ASSERT_TRUE(client && server && extract);
        const bytes_t& shared = extract->fields.at("IKM");
        Queue cq{client->fields.at("private key")};
        tls::ClientShares shares;
        shares.add(c.group, cq.entropy());
        EXPECT_TRUE(cq.all_used());
        EXPECT_EQ(of(shares.public_share(0)), client->fields.at("public key"));
        auto s = shares.shared(c.group, view(server->fields.at("public key")));
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(of(*s), shared);
        Queue sq{server->fields.at("private key")};
        auto ss = tls::server_share(c.group, view(client->fields.at("public key")), sq.entropy());
        ASSERT_TRUE(ss.has_value());
        EXPECT_TRUE(sq.all_used());
        EXPECT_EQ(ss->public_share, server->fields.at("public key"));
        EXPECT_EQ(of(ss->secret), shared);
    }
}

// OpenSSL's X25519, P-256, P-384 and P-521 exchanges, from either side
TEST(TlsKeyShare, OpenSslClassic) {
    size_t n = 0;
    for (auto& v : tls_key_share_vectors::classic) {
        SCOPED_TRACE(v.group);
        Group g = group_of(v.group);
        Queue q{unhex(v.priv)};
        tls::ClientShares shares;
        shares.add(g, q.entropy());
        EXPECT_TRUE(q.all_used());
        EXPECT_EQ(of(shares.public_share(0)), unhex(v.share));
        EXPECT_EQ(shares.public_share(0).size(), tls::client_share_size(g));
        auto s = shares.shared(g, view(unhex(v.peer)));
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(of(*s), unhex(v.secret));
        Queue sq{unhex(v.priv)};
        auto ss = tls::server_share(g, view(unhex(v.peer)), sq.entropy());
        ASSERT_TRUE(ss.has_value());
        EXPECT_EQ(ss->public_share, unhex(v.share));
        EXPECT_EQ(of(ss->secret), unhex(v.secret));
        ++n;
    }
    EXPECT_EQ(n, 16u);
}

// X25519MLKEM768: the client's share and secret against OpenSSL's hybrid
TEST(TlsKeyShare, OpenSslHybrid) {
    for (auto& v : tls_key_share_vectors::hybrid) {
        bytes_t draws = unhex(v.seed);
        bytes_t x = unhex(v.x25519);
        draws.insert(draws.end(), x.begin(), x.end());
        Queue q{draws};
        tls::ClientShares shares;
        shares.add(Group::x25519_mlkem768, q.entropy());
        EXPECT_TRUE(q.all_used());
        EXPECT_EQ(of(shares.public_share(0)), unhex(v.share));
        EXPECT_EQ(shares.public_share(0).size(), 1216u);
        bytes_t server = unhex(v.server_share);
        ASSERT_EQ(server.size(), 1120u);
        auto s = shares.shared(Group::x25519_mlkem768, view(server));
        ASSERT_TRUE(s.has_value());
        EXPECT_EQ(of(*s), unhex(v.secret));
        EXPECT_EQ(s->size, 64);
    }
}

// Both sides of every group, several shares at once, with crypto::random
TEST(TlsKeyShare, RoundTrip) {
    tls::Entropy system;
    // four shares at most (MaxShares): P-521 in a ClientShares of its own
    for (int round = 0; round < 4; ++round) {
        tls::ClientShares shares;
        if (round == 3) {
            shares.add(Group::secp521r1, system);
        } else {
            for (Group g : {Group::x25519_mlkem768, Group::x25519, Group::secp256r1, Group::secp384r1}) {
                shares.add(g, system);
            }
        }
        ASSERT_EQ(shares.size(), round == 3 ? 1u : 4u);
        for (size_t i = 0; i < shares.size(); ++i) {
            Group g = shares.group(i);
            auto ss = tls::server_share(g, shares.public_share(i), system);
            ASSERT_TRUE(ss.has_value());
            EXPECT_EQ(ss->public_share.size(), tls::server_share_size(g));
            auto s = shares.shared(g, view(ss->public_share));
            ASSERT_TRUE(s.has_value());
            EXPECT_EQ(of(*s), of(ss->secret));
        }
        shares.clear();
        EXPECT_EQ(shares.size(), 0u);
        EXPECT_FALSE(shares.has(Group::x25519));
        shares.add(Group::secp256r1, system);   // the second ClientHello's
        EXPECT_TRUE(shares.has(Group::secp256r1));
    }
}

// The server's X25519MLKEM768 draws its message m, then its X25519 key
TEST(TlsKeyShare, HybridServerDraws) {
    tls::Entropy system;
    tls::ClientShares shares;
    shares.add(Group::x25519_mlkem768, system);
    bytes_t draws(64, 0x5A);
    Queue a{draws}, b{draws};
    auto one = tls::server_share(Group::x25519_mlkem768, shares.public_share(0), a.entropy());
    auto two = tls::server_share(Group::x25519_mlkem768, shares.public_share(0), b.entropy());
    ASSERT_TRUE(one && two);
    EXPECT_TRUE(a.all_used());
    EXPECT_EQ(one->public_share, two->public_share);   // the same draws, the same share
    EXPECT_EQ(of(one->secret), of(two->secret));
}

// A scalar out of range is drawn again
TEST(TlsKeyShare, ScalarDrawnAgain) {
    for (auto [g, n] : {std::pair{Group::secp256r1, size_t(32)}, std::pair{Group::secp384r1, size_t(48)}, std::pair{Group::secp521r1, size_t(66)}}) {
        bytes_t draws(n, 0xFF);                // at least the order (P-521's top byte masked to 01)
        draws.insert(draws.end(), n, 0x00);    // zero
        bytes_t good(n, 0x11);
        draws.insert(draws.end(), good.begin(), good.end());
        Queue q{draws};
        tls::ClientShares shares;
        shares.add(g, q.entropy());
        EXPECT_TRUE(q.all_used());
        Queue direct{good};
        tls::ClientShares expected;
        expected.add(g, direct.entropy());
        EXPECT_EQ(of(shares.public_share(0)), of(expected.public_share(0)));
    }
}

// Shares that are not valid ones: illegal_parameter (§4.2.8, §7.4.2)
TEST(TlsKeyShare, InvalidShares) {
    using tls::AlertDescription;
    tls::Entropy system;
    tls::ClientShares shares;
    for (Group g : {Group::x25519, Group::secp256r1, Group::secp384r1, Group::x25519_mlkem768}) {
        shares.add(g, system);
    }
    tls::ClientShares p521_shares;   // four shares at most in one
    p521_shares.add(Group::secp521r1, system);
    auto of_group = [&](Group g) -> tls::ClientShares& {
        return g == Group::secp521r1 ? p521_shares : shares;
    };
    // a group not offered
    tls::ClientShares only;
    only.add(Group::x25519, system);
    EXPECT_EQ(alert_of(only.shared(Group::secp256r1, view(bytes_t(65, 4)))), AlertDescription::illegal_parameter);
    // lengths
    for (Group g : {Group::x25519, Group::secp256r1, Group::secp384r1, Group::secp521r1, Group::x25519_mlkem768}) {
        size_t n = tls::server_share_size(g);
        EXPECT_EQ(alert_of(of_group(g).shared(g, view(bytes_t(n - 1, 4)))), AlertDescription::illegal_parameter);
        EXPECT_EQ(alert_of(of_group(g).shared(g, view(bytes_t(n + 1, 4)))), AlertDescription::illegal_parameter);
        EXPECT_EQ(alert_of(tls::server_share(g, view(bytes_t(tls::client_share_size(g) + 1, 4)), system)), AlertDescription::illegal_parameter);
    }
    // X25519 of small order: the secret of all zeros
    bytes_t zero(32, 0), one(32, 0);
    one[0] = 1;
    EXPECT_EQ(alert_of(shares.shared(Group::x25519, view(zero))), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(shares.shared(Group::x25519, view(one))), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(tls::server_share(Group::x25519, view(zero), system)), AlertDescription::illegal_parameter);
    // P-256: not uncompressed, off the curve
    tls::ClientShares p;
    p.add(Group::secp256r1, system);
    bytes_t point = of(p.public_share(0));
    bytes_t compressed = point;
    compressed[0] = 0x02;
    EXPECT_EQ(alert_of(shares.shared(Group::secp256r1, view(compressed))), AlertDescription::illegal_parameter);
    bytes_t off = point;
    off[64] ^= 1;
    EXPECT_EQ(alert_of(shares.shared(Group::secp256r1, view(off))), AlertDescription::illegal_parameter);
    EXPECT_EQ(alert_of(tls::server_share(Group::secp256r1, view(off), system)), AlertDescription::illegal_parameter);
    EXPECT_TRUE(shares.shared(Group::secp256r1, view(point)).has_value());
    // P-256, P-384 and P-521 (the auditor's condition): off the curve, a
    // coordinate not below p, the point at infinity (the single byte 00 and
    // 04 with zero coordinates), from either side
    for (auto [g, n] : {std::pair{Group::secp256r1, size_t(65)}, std::pair{Group::secp384r1, size_t(97)}, std::pair{Group::secp521r1, size_t(133)}}) {
        tls::ClientShares mine;
        mine.add(g, system);
        bytes_t good = of(mine.public_share(0));
        bytes_t off_curve = good;
        off_curve[n - 1] ^= 1;
        bytes_t over_p = good;
        std::fill(over_p.begin() + 1, over_p.begin() + 1 + (n - 1) / 2, 0xFF);
        bytes_t zeros(n, 0);
        zeros[0] = 0x04;
        for (const bytes_t& bad : {off_curve, over_p, zeros, bytes_t{0x00}}) {
            EXPECT_EQ(alert_of(of_group(g).shared(g, view(bad))), AlertDescription::illegal_parameter) << bad.size();
            EXPECT_EQ(alert_of(tls::server_share(g, view(bad), system)), AlertDescription::illegal_parameter) << bad.size();
        }
        EXPECT_TRUE(tls::server_share(g, view(good), system).has_value());
    }
    // X25519 (§7.4.2): every point of small order gives the secret of all
    // zeros, refused from either side
    for (const char* point : {"0000000000000000000000000000000000000000000000000000000000000000",
                              "0100000000000000000000000000000000000000000000000000000000000000",
                              "e0eb7a7c3b41b8ae1656e3faf19fc46ada098deb9c32b1fd866205165f49b800",
                              "5f9c95bca3508c24b1d0b1559c83ef5b04445cc4581c8e86d8224eddd09f1157",
                              "ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
                              "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
                              "eeffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f"}) {
        EXPECT_EQ(alert_of(shares.shared(Group::x25519, view(unhex(point)))), AlertDescription::illegal_parameter) << point;
        EXPECT_EQ(alert_of(tls::server_share(Group::x25519, view(unhex(point)), system)), AlertDescription::illegal_parameter) << point;
    }
    // the hybrid: its X25519 part of small order; an encapsulation key
    // with a coefficient not below q
    bytes_t hybrid(1120, 7);
    std::fill(hybrid.end() - 32, hybrid.end(), 0);
    EXPECT_EQ(alert_of(shares.shared(Group::x25519_mlkem768, view(hybrid))), AlertDescription::illegal_parameter);
    bytes_t ek = of(shares.public_share(3));
    bytes_t bad_ek = ek;
    bad_ek[0] = 0xFF;
    bad_ek[1] |= 0x0F;   // the first coefficient 0xFFF >= q
    EXPECT_EQ(alert_of(tls::server_share(Group::x25519_mlkem768, view(bad_ek), system)), AlertDescription::illegal_parameter);
    bytes_t bad_x = ek;
    std::fill(bad_x.end() - 32, bad_x.end(), 0);
    EXPECT_EQ(alert_of(tls::server_share(Group::x25519_mlkem768, view(bad_x), system)), AlertDescription::illegal_parameter);
    EXPECT_TRUE(tls::server_share(Group::x25519_mlkem768, view(ek), system).has_value());
    // contracts
    EXPECT_THROW(only.add(Group::x25519, system), std::logic_error);
    EXPECT_THROW(only.add(Group(0x001E), system), std::logic_error);   // X448: not a group here
}
