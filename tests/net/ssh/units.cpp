//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh without a connection: the wire types of RFC 4251 (its mpint
// examples), the packet ciphers both ways and their refusals, KEXINIT and
// the negotiation rules of RFC 4253 §7.1, the key derivation, bcrypt_pbkdf
// against its published vector, the keys of the test data (made by
// ssh-keygen and OpenSSL: fingerprints as ssh-keygen -l prints them,
// encrypted keys, keys written and read back by ssh-keygen), certificates
// (ssh-keygen -s), known_hosts and authorized_keys.
#include "tests/types.h"
#include "tests/source_root.h"
#include "sgcl/net/ssh.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace sgcl;
namespace d = sgcl::net::ssh::detail;

namespace {
    std::string data_path(const std::string& name) {
        return (source_root() / "tests/net/ssh/testdata" / name).string();
    }

    std::string read(const std::string& name) {
        std::ifstream f(data_path(name));
        std::stringstream s;
        s << f.rdbuf();
        return s.str();
    }

    std::string hex(const d::Bytes& b) {
        static const char* x = "0123456789abcdef";
        std::string s;
        for (uint8_t c : b) {
            s += x[c >> 4];
            s += x[c & 15];
        }
        return s;
    }

    d::Bytes unhex(std::string_view h) {
        d::Bytes b;
        for (size_t i = 0; i + 1 < h.size(); i += 2) {
            b.push_back(uint8_t(std::stoi(std::string(h.substr(i, 2)), nullptr, 16)));
        }
        return b;
    }

    std::string run(const std::string& cmd) {
        std::string out;
        FILE* p = ::popen(cmd.c_str(), "r");
        if (!p) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
            out.append(buf, n);
        }
        ::pclose(p);
        return out;
    }

    std::filesystem::path temp_dir(const char* name) {
        auto p = std::filesystem::temp_directory_path() / ("sgcl_ssh_" + std::string(name) + "_" + std::to_string(::getpid()));
        std::filesystem::create_directories(p);
        return p;
    }
}

// --- the wire ---------------------------------------------------------------

TEST(SshWire, MpintExamplesOfRfc4251) {
    // RFC 4251 §5: 0, 9a378f9b2e332a7, 80
    struct {
        const char* value;
        const char* wire;
    } cases[] = {
        {"", "00000000"},
        {"09a378f9b2e332a7", "0000000809a378f9b2e332a7"},
        {"80", "000000020080"},
        {"0000ff", "0000000200ff"},
        {"7f", "000000017f"},
    };
    for (auto& c : cases) {
        d::Bytes b;
        d::Writer w(b);
        auto v = unhex(c.value);
        w.mpint(v.data(), v.size());
        EXPECT_EQ(hex(b), c.wire) << c.value;
        d::Reader r(b.data(), b.size());
        auto s = r.mpint();
        ASSERT_TRUE(r.done());
        d::Bytes back(s.p, s.p + s.n);
        auto stripped = v;
        while (!stripped.empty() && stripped[0] == 0) {
            stripped.erase(stripped.begin());
        }
        EXPECT_EQ(back, stripped);
    }
}

TEST(SshWire, MpintRefusals) {
    for (const char* bad : {"0000000180", "00000002007f", "0000000100", "00000002ffff", "000000020080ff"}) {
        auto b = unhex(bad);
        d::Reader r(b.data(), b.size());
        (void)r.mpint();
        EXPECT_FALSE(r.done()) << bad;
    }
}

TEST(SshWire, ReaderPastItsEndFailsSoftly) {
    d::Bytes b = {0, 0, 0, 5, 'a', 'b'};
    d::Reader r(b.data(), b.size());
    EXPECT_TRUE(r.string().empty());
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(r.u32(), 0u);
    EXPECT_EQ(r.u8(), 0u);
    EXPECT_FALSE(r.done());
    d::Reader empty(nullptr, 0);
    EXPECT_TRUE(empty.done());
    EXPECT_EQ(empty.u64(), 0u);
    EXPECT_FALSE(empty.ok());
}

TEST(SshWire, NameLists) {
    d::Bytes b;
    d::Writer w(b);
    std::vector<std::string> names = {"zlib", "none"};
    w.name_list(names);
    w.name_list(std::vector<std::string>());
    d::Reader r(b.data(), b.size());
    auto a = r.name_list();
    auto e = r.name_list();
    ASSERT_TRUE(r.done());
    ASSERT_EQ(a.size(), 2u);
    EXPECT_EQ(a[0], "zlib");
    EXPECT_EQ(a[1], "none");
    EXPECT_TRUE(e.empty());
    for (const char* bad : {"a,,b", ",a", "a,", "a b", "a\x01"}) {
        d::Bytes x;
        d::Writer xw(x);
        xw.string(bad);
        d::Reader xr(x.data(), x.size());
        EXPECT_TRUE(xr.name_list().empty()) << bad;
        EXPECT_FALSE(xr.ok()) << bad;
    }
    EXPECT_EQ(d::first_common(std::vector<std::string>{"a", "b", "c"}, std::vector<std::string>{"c", "b"}), "b");
    EXPECT_EQ(d::first_common(std::vector<std::string>{"a"}, std::vector<std::string>{"b"}), "");
}

TEST(SshWire, PrintableCutsAndMasks) {
    EXPECT_EQ(d::printable("a\x01\x7f" "b"), "a??b");
    EXPECT_EQ(d::printable(std::string(1000, 'x')).size(), 256u);
}

// --- the packet ciphers -----------------------------------------------------

namespace {
    void keys_for(d::PacketKeys& k, const d::CipherInfo& c, const d::MacInfo* m) {
        uint8_t key[64], iv[16], mk[64];
        for (int i = 0; i < 64; ++i) {
            key[i] = uint8_t(i * 7 + 1);
            mk[i] = uint8_t(i * 3);
        }
        for (int i = 0; i < 16; ++i) {
            iv[i] = uint8_t(0xA0 + i);
        }
        k.set(c.kind, key, iv, m ? m->kind : d::MacKind::none, mk);
    }
}

TEST(SshCipher, EveryCipherAndMacRoundTrips) {
    for (const auto& c : d::cipher_table) {
        for (size_t mi = 0; mi < std::size(d::mac_table); ++mi) {
            const d::MacInfo* m = c.aead ? nullptr : &d::mac_table[mi];
            d::PacketKeys a, b;
            keys_for(a, c, m);
            keys_for(b, c, m);
            for (uint32_t seq : {0u, 1u, 2u, 0xFFFFFFFFu, 0u}) {
                for (size_t size : {size_t(0), size_t(1), size_t(15), size_t(16), size_t(17), size_t(1000), size_t(32768)}) {
                    d::Bytes payload(size);
                    for (size_t i = 0; i < size; ++i) {
                        payload[i] = uint8_t(i ^ seq);
                    }
                    d::Bytes out;
                    a.seal(seq, payload.data(), payload.size(), out);
                    ASSERT_EQ((out.size() - b.tag_size() - (b.length_aligned() ? 0 : 4)) % b.block(), 0u) << c.name;
                    uint32_t len;
                    size_t total;
                    ASSERT_EQ(b.length(seq, out.data(), len, total), d::OpenError::none) << c.name;
                    ASSERT_EQ(total, out.size());
                    size_t at, n;
                    ASSERT_EQ(b.open(seq, out.data(), len, at, n), d::OpenError::none) << c.name << " " << (m ? m->name : "");
                    ASSERT_EQ(n, size);
                    EXPECT_EQ(0, std::memcmp(out.data() + at, payload.data(), n));
                }
            }
            if (c.aead) {
                break;
            }
        }
    }
}

TEST(SshCipher, AFlippedBitIsRefused) {
    for (const auto& c : d::cipher_table) {
        const d::MacInfo* m = c.aead ? nullptr : &d::mac_table[0];
        for (size_t flip : {size_t(4), size_t(10), size_t(40)}) {
            d::PacketKeys a, b;
            keys_for(a, c, m);
            keys_for(b, c, m);
            d::Bytes payload(30, 0x55), out;
            a.seal(5, payload.data(), payload.size(), out);
            out[flip] ^= 1;
            uint32_t len;
            size_t total;
            auto e = b.length(5, out.data(), len, total);
            if (e == d::OpenError::none && total == out.size()) {
                size_t at, n;
                EXPECT_NE(b.open(5, out.data(), len, at, n), d::OpenError::none) << c.name << " flip " << flip;
            }
        }
        // the wrong sequence number (AES-GCM's nonce is its own counter)
        if (c.kind == d::CipherKind::aes128_gcm || c.kind == d::CipherKind::aes256_gcm) {
            continue;
        }
        d::PacketKeys a, b;
        keys_for(a, c, m);
        keys_for(b, c, m);
        d::Bytes payload(30, 0x55), out;
        a.seal(5, payload.data(), payload.size(), out);
        uint32_t len;
        size_t total;
        if (b.length(6, out.data(), len, total) == d::OpenError::none && total == out.size()) {
            size_t at, n;
            EXPECT_NE(b.open(6, out.data(), len, at, n), d::OpenError::none) << c.name;
        }
    }
}

TEST(SshCipher, LengthsOutOfRangeAreRefused) {
    d::PacketKeys none;
    uint8_t head[16] = {};
    uint32_t len;
    size_t total;
    for (uint32_t l : {0u, 4u, 11u, d::MaxPacketLength + 4, 0xFFFFFFF0u}) {
        d::store32(head, l);
        EXPECT_EQ(none.length(0, head, len, total), d::OpenError::length) << l;
    }
    d::store32(head, 12);   // 16 with the length: aligned
    EXPECT_EQ(none.length(0, head, len, total), d::OpenError::none);
    // a padding of 3 is refused
    d::Bytes p = {0, 0, 0, 12, 3, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    size_t at, n;
    EXPECT_EQ(none.open(0, p.data(), 12, at, n), d::OpenError::padding);
    p[4] = 12;   // more padding than the packet
    EXPECT_EQ(none.open(0, p.data(), 12, at, n), d::OpenError::padding);
}

TEST(SshCipher, ChaChaLengthIsEncryptedAndTheMacCoversIt) {
    // PROTOCOL.chacha20poly1305: the length is not sent in the clear
    d::PacketKeys a;
    keys_for(a, d::cipher_table[0], nullptr);
    d::Bytes payload(100, 1), out;
    a.seal(0, payload.data(), payload.size(), out);
    EXPECT_NE(d::load32(out.data()), out.size() - 4 - 16);
    // the same packet sealed under another sequence number differs whole
    d::Bytes out2;
    a.seal(1, payload.data(), payload.size(), out2);
    EXPECT_NE(std::memcmp(out.data(), out2.data(), 4), 0);
}

// --- KEXINIT and negotiation ------------------------------------------------

namespace {
    d::Kexinit kexinit_of(const d::Preferences& p, bool client, bool first) {
        auto b = d::make_kexinit(p, client, first);
        d::Kexinit k;
        EXPECT_TRUE(d::read_kexinit(b.data(), b.size(), k));
        return k;
    }
}

TEST(SshKex, KexinitCarriesTheMarkersInTheFirstExchange) {
    auto p = d::default_preferences();
    auto c = kexinit_of(p, true, true);
    EXPECT_TRUE(d::contains_name(c.lists[d::ListKex], d::ExtInfoClient));
    EXPECT_TRUE(d::contains_name(c.lists[d::ListKex], d::StrictClient));
    auto s = kexinit_of(p, false, true);
    EXPECT_TRUE(d::contains_name(s.lists[d::ListKex], d::StrictServer));
    auto later = kexinit_of(p, true, false);
    EXPECT_FALSE(d::contains_name(later.lists[d::ListKex], d::StrictClient));
    d::Negotiated n;
    ASSERT_EQ(d::negotiate(c, s, true, n), nullptr);
    EXPECT_TRUE(n.strict);
    EXPECT_EQ(n.kex->name, "mlkem768x25519-sha256");
    EXPECT_EQ(n.cipher[0]->name, "chacha20-poly1305@openssh.com");
    EXPECT_EQ(n.mac[0], nullptr);
    EXPECT_FALSE(n.zlib[0]);
}

TEST(SshKex, TheClientsOrderDecides) {
    auto cp = d::default_preferences();
    auto sp = d::default_preferences();
    cp.kex = {"ecdh-sha2-nistp384", "curve25519-sha256"};
    sp.kex = {"curve25519-sha256", "ecdh-sha2-nistp384"};
    cp.ciphers = {"aes256-ctr", "aes128-gcm@openssh.com"};
    cp.macs = {"hmac-sha2-512"};
    sp.compression = {"zlib@openssh.com", "none"};
    d::Negotiated n;
    ASSERT_EQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), nullptr);
    EXPECT_EQ(n.kex->name, "ecdh-sha2-nistp384");
    EXPECT_EQ(n.cipher[0]->name, "aes256-ctr");
    EXPECT_EQ(n.mac[0]->name, "hmac-sha2-512");
    EXPECT_FALSE(n.zlib[0]);   // the client offers none alone
    cp.compression = {"zlib@openssh.com", "none"};
    ASSERT_EQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), nullptr);
    EXPECT_TRUE(n.zlib[0] && n.zlib[1]);
}

TEST(SshKex, NothingInCommonNamesTheList) {
    auto cp = d::default_preferences();
    auto sp = d::default_preferences();
    cp.kex = {"curve25519-sha256"};
    sp.kex = {"diffie-hellman-group14-sha256"};
    d::Negotiated n;
    EXPECT_STREQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), "no common key exchange algorithm");
    sp = d::default_preferences();
    sp.ciphers = {"aes128-ctr"};
    cp = d::default_preferences();
    cp.ciphers = {"aes256-ctr"};
    EXPECT_STREQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), "no common cipher");
    cp = d::default_preferences();
    cp.host_key = {"ssh-ed25519"};
    sp = d::default_preferences();
    sp.host_key = {"rsa-sha2-512"};
    EXPECT_STREQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), "no common host key algorithm");
    cp = d::default_preferences();
    cp.ciphers = {"aes128-ctr"};
    cp.macs = {"hmac-sha2-256"};
    sp = d::default_preferences();
    sp.macs = {"hmac-sha2-512"};
    EXPECT_STREQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), "no common MAC");
    // the pseudo-algorithms never negotiate as a key exchange
    cp = d::default_preferences();
    cp.kex = {};
    sp = d::default_preferences();
    sp.kex = {};
    EXPECT_STREQ(d::negotiate(kexinit_of(cp, true, true), kexinit_of(sp, false, true), true, n), "no common key exchange algorithm");
}

TEST(SshKex, AWrongGuessIsDropped) {
    auto cp = d::default_preferences();
    auto sp = d::default_preferences();
    sp.kex = {"curve25519-sha256", "mlkem768x25519-sha256"};
    auto s = kexinit_of(sp, false, true);
    s.follows = true;
    d::Negotiated n;
    ASSERT_EQ(d::negotiate(kexinit_of(cp, true, true), s, true, n), nullptr);
    EXPECT_TRUE(n.guess_wrong);
    sp = cp;
    s = kexinit_of(sp, false, true);
    s.follows = true;
    ASSERT_EQ(d::negotiate(kexinit_of(cp, true, true), s, true, n), nullptr);
    EXPECT_FALSE(n.guess_wrong);
}

TEST(SshKex, EveryMethodAgreesBothWays) {
    for (const auto& info : d::kex_table) {
        d::KexExchange client(info), server(info);
        d::Bytes ic = {20, 1}, is = {20, 2};
        d::HashInputs in{"SSH-2.0-a", "SSH-2.0-b", &ic, &is};
        auto host = net::ssh::private_key::generate();
        const auto& kp = d::PrivateKeyAccess::key(host);
        auto init = client.client_init();
        d::Bytes reply;
        ASSERT_EQ(server.server_init(init.data(), init.size(), in, kp.public_blob, reply), nullptr) << info.name;
        auto sig = kp.sign("ssh-ed25519", server.h().data(), server.h().size());
        d::Writer w(reply);
        w.string(sig);
        d::Span hk, s;
        ASSERT_EQ(client.client_reply(reply.data(), reply.size(), in, hk, s), nullptr) << info.name;
        EXPECT_EQ(client.k(), server.k()) << info.name;
        EXPECT_EQ(client.h(), server.h()) << info.name;
        EXPECT_EQ(client.h().size(), d::hash_size(info.hash));
        d::ParsedKey pk;
        ASSERT_TRUE(d::parse_key(hk.p, hk.n, pk));
        EXPECT_TRUE(d::verify(pk.key, "ssh-ed25519", client.h().data(), client.h().size(), s.p, s.n));
    }
}

TEST(SshKex, SharesOutOfRangeAreRefused) {
    for (const auto& info : d::kex_table) {
        d::KexExchange server(info);
        d::Bytes ic = {20}, is = {20};
        d::HashInputs in{"a", "b", &ic, &is};
        d::Bytes init, reply, blob = {0};
        d::Writer w(init);
        w.u8(d::MsgKexInit);
        if (info.kind == d::KexKind::dh14 || info.kind == d::KexKind::dh16) {
            uint8_t one = 1;
            w.mpint(&one, 1);   // f = 1
        } else {
            w.string(std::string(5, 'x'));
        }
        EXPECT_NE(server.server_init(init.data(), init.size(), in, blob, reply), nullptr) << info.name;
    }
    // p - 1 refused, p - 2 taken
    d::DhGroup g(d::KexKind::dh14);
    d::Bytes p(d::modp2048, d::modp2048 + 256);
    p[255] -= 1;
    EXPECT_FALSE(g.in_range(p.data(), p.size()));
    p[255] -= 1;
    EXPECT_TRUE(g.in_range(p.data(), p.size()));
    uint8_t two = 2;
    EXPECT_TRUE(g.in_range(&two, 1));
}

TEST(SshKex, DiffieHellmanMatchesSmallPowers) {
    // 2^x mod p for x = 1, 2, 10 by the constant-time path
    d::DhGroup g(d::KexKind::dh14);
    uint8_t two = 2;
    for (uint8_t x : {uint8_t(1), uint8_t(2), uint8_t(10)}) {
        uint8_t xb[4] = {0, 0, 0, x};
        d::Bytes r(256);
        g.power(&two, 1, xb, 4, r.data());
        d::Bytes want(256, 0);
        uint32_t v = 1u << x;
        d::store32(want.data() + 252, v);
        EXPECT_EQ(r, want) << int(x);
    }
}

TEST(SshKex, KeyDerivationExtendsByHashing) {
    d::Bytes k = {0, 0, 0, 1, 5}, h(32, 7), sid(32, 9);
    d::SecretBuffer a, b;
    d::derive_key(d::HashKind::sha256, k, h, sid, 'C', 64, a);
    d::derive_key(d::HashKind::sha256, k, h, sid, 'C', 32, b);
    ASSERT_EQ(a.b.size(), 64u);
    EXPECT_EQ(0, std::memcmp(a.b.data(), b.b.data(), 32));   // K1 is the first part
    // K2 = HASH(K || H || K1)
    d::Bytes in = k;
    in.insert(in.end(), h.begin(), h.end());
    in.insert(in.end(), b.b.begin(), b.b.end());
    uint8_t k2[32];
    d::hash_of(d::HashKind::sha256, in.data(), in.size(), k2);
    EXPECT_EQ(0, std::memcmp(a.b.data() + 32, k2, 32));
}

// --- bcrypt_pbkdf and the key files ---------------------------------------------

TEST(SshKeys, BcryptPbkdfVector) {
    // the published vector of bcrypt_pbkdf (OpenBSD's regress, python-bcrypt)
    uint8_t k[32];
    ASSERT_TRUE(crypto::detail::bcrypt_pbkdf(reinterpret_cast<const uint8_t*>("password"), 8, reinterpret_cast<const uint8_t*>("salt"), 4, 4, k, 32));
    EXPECT_EQ(hex(d::Bytes(k, k + 32)), "5bbf0cc293587f1c3635555c27796598d47e579071bf427e9d8fbe842aba34d9");
    EXPECT_FALSE(crypto::detail::bcrypt_pbkdf(reinterpret_cast<const uint8_t*>("p"), 1, reinterpret_cast<const uint8_t*>("s"), 1, 0, k, 32));
    EXPECT_FALSE(crypto::detail::bcrypt_pbkdf(reinterpret_cast<const uint8_t*>("p"), 1, reinterpret_cast<const uint8_t*>("s"), 0, 1, k, 32));
    EXPECT_FALSE(crypto::detail::bcrypt_pbkdf(reinterpret_cast<const uint8_t*>("p"), 1, reinterpret_cast<const uint8_t*>("s"), 1, 1, k, 0));
}

TEST(SshKeys, FilesOfSshKeygenAndOpenSslLoad) {
    struct {
        const char* file;
        const char* pass;
        net::ssh::key_type type;
    } cases[] = {
        {"ed25519", "", net::ssh::key_type::ed25519},
        {"ed25519_enc", "correct horse", net::ssh::key_type::ed25519},
        {"ed25519_gcm", "correct horse", net::ssh::key_type::ed25519},
        {"p256", "", net::ssh::key_type::ecdsa_p256},
        {"p384_enc", "correct horse", net::ssh::key_type::ecdsa_p384},
        {"p521", "", net::ssh::key_type::ecdsa_p521},
        {"rsa", "", net::ssh::key_type::rsa},
        {"rsa_enc", "correct horse", net::ssh::key_type::rsa},
        {"rsa_pkcs1", "", net::ssh::key_type::rsa},
        {"p256_sec1.pem", "", net::ssh::key_type::ecdsa_p256},
        {"p384_pkcs8.pem", "", net::ssh::key_type::ecdsa_p384},
        {"p521_pkcs8.pem", "", net::ssh::key_type::ecdsa_p521},
        {"rsa_pkcs8.pem", "", net::ssh::key_type::rsa},
        {"ed25519_pkcs8.pem", "", net::ssh::key_type::ed25519},
    };
    const std::string fps = read("fingerprints.txt");
    for (auto& c : cases) {
        auto k = net::ssh::private_key::load(string(data_path(c.file)), string(c.pass));
        ASSERT_TRUE(k) << c.file << ": " << k.error().message().view();
        EXPECT_EQ(k->type(), c.type) << c.file;
        auto pub = k->public_key();
        if (std::filesystem::exists(data_path(std::string(c.file) + ".pub"))) {
            auto line = net::ssh::public_key::parse(string(read(std::string(c.file) + ".pub")));
            ASSERT_TRUE(line) << c.file;
            EXPECT_EQ(*line, pub) << c.file;
            EXPECT_NE(fps.find(std::string(pub.fingerprint().view())), std::string::npos) << c.file << " " << pub.fingerprint().view();
        }
        auto sig = k->sign(slice<const byte>("data"));
        EXPECT_TRUE(pub.verify(slice<const byte>("data"), sig.as_slice())) << c.file;
        EXPECT_FALSE(pub.verify(slice<const byte>("datA"), sig.as_slice())) << c.file;
    }
    auto e = net::ssh::private_key::load(string(data_path("ed25519")));
    EXPECT_EQ(e->comment(), "test-ed25519");
}

TEST(SshKeys, PassphrasesAndBrokenFiles) {
    auto none = net::ssh::private_key::load(string(data_path("ed25519_enc")));
    ASSERT_FALSE(none);
    EXPECT_EQ(none.error().code(), crypto::errc::authentication);
    auto wrong = net::ssh::private_key::load(string(data_path("ed25519_enc")), string("wrong"));
    ASSERT_FALSE(wrong);
    EXPECT_EQ(wrong.error().code(), crypto::errc::authentication);
    auto gcm = net::ssh::private_key::load(string(data_path("ed25519_gcm")), string("wrong"));
    EXPECT_EQ(gcm.error().code(), crypto::errc::authentication);
    auto missing = net::ssh::private_key::load(string(data_path("nothing-here")));
    EXPECT_TRUE(missing.error().is_not_found());
    for (std::string bad : {std::string(""), std::string("not a key"), std::string("-----BEGIN OPENSSH PRIVATE KEY-----\nAAAA\n-----END OPENSSH PRIVATE KEY-----\n"),
                            std::string("-----BEGIN OPENSSH PRIVATE KEY-----\n")}) {
        auto k = net::ssh::private_key::parse(slice<const byte>(std::string_view(bad)));
        EXPECT_FALSE(k) << bad;
        EXPECT_EQ(k.error().code(), crypto::errc::malformed) << bad;
    }
    // a byte of the body changed: the check words or the key no longer agree
    std::string text = read("ed25519");
    size_t at = text.find('\n') + 40;
    text[at] = text[at] == 'A' ? 'B' : 'A';
    EXPECT_FALSE(net::ssh::private_key::parse(slice<const byte>(std::string_view(text))));
}

TEST(SshKeys, WrittenKeysReadBackHereAndBySshKeygen) {
    auto dir = temp_dir("keys");
    for (auto t : {net::ssh::key_type::ed25519, net::ssh::key_type::ecdsa_p256, net::ssh::key_type::ecdsa_p384, net::ssh::key_type::ecdsa_p521, net::ssh::key_type::rsa}) {
        auto k = net::ssh::private_key::generate(t, 2048).with_comment(string("made-here"));
        EXPECT_EQ(k.comment(), "made-here");
        for (const char* pass : {"", "s3cret"}) {
            auto text = k.to_openssh(string(pass));
            auto back = net::ssh::private_key::parse(text.as_slice(), string(pass));
            ASSERT_TRUE(back) << int(t);
            EXPECT_EQ(back->public_key(), k.public_key());
            EXPECT_EQ(back->comment(), "made-here");
            auto path = (dir / ("k" + std::to_string(int(t)) + (pass[0] ? "e" : ""))).string();
            ASSERT_TRUE(k.save(string(path), string(pass)));
            EXPECT_EQ(std::filesystem::status(path).permissions() & std::filesystem::perms::all, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
            std::string pub = run("ssh-keygen -y -P '" + std::string(pass) + "' -f " + path + " 2>/dev/null");
            if (!pub.empty()) {   // where there is an ssh-keygen
                auto line = net::ssh::public_key::parse(string(pub));
                ASSERT_TRUE(line) << pub;
                EXPECT_EQ(*line, k.public_key()) << int(t);
            }
        }
    }
    std::filesystem::remove_all(dir);
}

TEST(SshKeys, PublicKeysAsText) {
    auto line = net::ssh::public_key::parse(string(read("ed25519.pub")));
    ASSERT_TRUE(line);
    EXPECT_EQ(line->type_name(), "ssh-ed25519");
    EXPECT_EQ(line->type(), net::ssh::key_type::ed25519);
    EXPECT_EQ(line->comment(), "test-ed25519");
    EXPECT_FALSE(line->is_certificate());
    EXPECT_FALSE(line->certificate());
    std::string text = read("ed25519.pub");
    while (!text.empty() && text.back() == '\n') {
        text.pop_back();
    }
    EXPECT_EQ(line->to_string(), text);
    auto from = net::ssh::public_key::from_bytes(line->bytes().as_slice());
    ASSERT_TRUE(from);
    EXPECT_EQ(*from, *line);
    EXPECT_EQ(from->comment(), "");
    EXPECT_THROW(net::ssh::public_key(string("ssh-ed25519 !!!")), std::invalid_argument);
    EXPECT_FALSE(net::ssh::public_key());
    for (const char* bad : {"", "ssh-ed25519", "ssh-dss AAAAB3NzaC1kc3MAAACBAP==", "ssh-rsa AAAAC3NzaC1lZDI1NTE5AAAAIP5k", "ecdsa-sha2-nistp256 AAAA"}) {
        EXPECT_FALSE(net::ssh::public_key::parse(string(bad))) << bad;
    }
    // the type of the line must be the blob's
    std::string swapped = "ssh-rsa" + text.substr(text.find(' '));
    EXPECT_FALSE(net::ssh::public_key::parse(string(swapped)));
}

TEST(SshKeys, ShaOneSignaturesAreRefused) {
    auto k = net::ssh::private_key::load(string(data_path("rsa")));
    ASSERT_TRUE(k);
    const auto& kp = d::PrivateKeyAccess::key(*k);
    EXPECT_FALSE(kp.can_sign("ssh-rsa"));
    auto sig = kp.sign("rsa-sha2-256", reinterpret_cast<const uint8_t*>("m"), 1);
    d::ParsedKey pk;
    ASSERT_TRUE(d::parse_key(kp.public_blob.data(), kp.public_blob.size(), pk));
    EXPECT_TRUE(d::verify(pk.key, "rsa-sha2-256", reinterpret_cast<const uint8_t*>("m"), 1, sig.data(), sig.size()));
    EXPECT_FALSE(d::verify(pk.key, "rsa-sha2-512", reinterpret_cast<const uint8_t*>("m"), 1, sig.data(), sig.size()));
    // the same bytes renamed ssh-rsa: refused
    d::Bytes renamed;
    d::Writer w(renamed);
    d::Reader r(sig.data(), sig.size());
    r.string();
    w.string("ssh-rsa").string(r.string());
    EXPECT_FALSE(d::verify(pk.key, "ssh-rsa", reinterpret_cast<const uint8_t*>("m"), 1, renamed.data(), renamed.size()));
}

TEST(SshKeys, CertificatesOfSshKeygen) {
    auto host = net::ssh::public_key::parse(string(read("host-cert.pub")));
    ASSERT_TRUE(host);
    EXPECT_TRUE(host->is_certificate());
    EXPECT_EQ(host->type_name(), "ecdsa-sha2-nistp256-cert-v01@openssh.com");
    EXPECT_EQ(host->type(), net::ssh::key_type::ecdsa_p256);
    auto c = host->certificate();
    ASSERT_TRUE(c);
    EXPECT_EQ(c->type, net::ssh::certificate_type::host);
    EXPECT_EQ(c->serial, 7u);
    EXPECT_EQ(c->key_id, "host-id");
    ASSERT_EQ(c->principals.size(), 2u);
    EXPECT_EQ(c->principals[0], "localhost");
    EXPECT_EQ(c->principals[1], "127.0.0.1");
    EXPECT_EQ(c->valid_after, 0u);
    EXPECT_EQ(c->valid_before, UINT64_MAX);
    EXPECT_EQ(c->key, *net::ssh::public_key::parse(string(read("host.pub"))));
    EXPECT_EQ(c->signature_key, *net::ssh::public_key::parse(string(read("ca.pub"))));
    auto user = net::ssh::public_key::parse(string(read("ed25519-cert.pub")));
    auto uc = user->certificate();
    ASSERT_TRUE(uc);
    EXPECT_EQ(uc->type, net::ssh::certificate_type::user);
    EXPECT_EQ(uc->serial, 9u);
    EXPECT_EQ(uc->principals.size(), 2u);
    bool pty = false;
    for (auto& e : uc->extensions) {
        pty |= e.first == "permit-pty";
    }
    EXPECT_TRUE(pty);
    // a certificate whose signature is broken has no fields
    auto blob = user->bytes();
    blob[blob.size() - 5] = byte(uint8_t(blob[blob.size() - 5]) ^ 1);
    auto broken = net::ssh::public_key::from_bytes(blob.as_slice());
    ASSERT_TRUE(broken);
    EXPECT_FALSE(broken->certificate());
}

// --- known_hosts ----------------------------------------------------------------

TEST(SshKnownHosts, PlainHashedPortsWildcardsAndNegation) {
    auto key = *net::ssh::public_key::parse(string(read("host.pub")));
    auto other = *net::ssh::public_key::parse(string(read("p256.pub")));
    auto rsa = *net::ssh::public_key::parse(string(read("rsa.pub")));
    std::string k = std::string(key.with_comment(string()).to_string().view());
    std::string text = "# a comment\n\n"
                       "example.com,192.0.2.1 " + k + "\n"
                       "[alt.example.com]:2222 " + k + "\n"
                       "*.wild.example,!bad.wild.example " + k + "\n"
                       "garbage line\n"
                       "@revoked revoked.example " + k + "\n";
    auto kh = net::ssh::known_hosts::parse(string(text));
    EXPECT_EQ(kh.size(), 4u);
    EXPECT_TRUE(kh.check(string("example.com:22"), key));
    EXPECT_TRUE(kh.check(string("example.com"), key));
    EXPECT_TRUE(kh.check(string("EXAMPLE.com"), key));
    EXPECT_TRUE(kh.check(string("192.0.2.1:22"), key));
    EXPECT_TRUE(kh.check(string("alt.example.com:2222"), key));
    EXPECT_EQ(kh.check(string("alt.example.com:22"), key).error().code(), net::errc::ssh_host_key_unknown);
    EXPECT_EQ(kh.check(string("example.com:2222"), key).error().code(), net::errc::ssh_host_key_unknown);
    EXPECT_TRUE(kh.check(string("a.wild.example"), key));
    EXPECT_EQ(kh.check(string("bad.wild.example"), key).error().code(), net::errc::ssh_host_key_unknown);
    EXPECT_EQ(kh.check(string("example.com"), other).error().code(), net::errc::ssh_host_key_mismatch);
    EXPECT_EQ(kh.check(string("example.com"), rsa).error().code(), net::errc::ssh_host_key_unknown);   // another type: not known
    EXPECT_EQ(kh.check(string("revoked.example"), key).error().code(), net::errc::ssh_host_key_revoked);
    EXPECT_EQ(kh.check(string("nowhere"), key).error().code(), net::errc::ssh_host_key_unknown);
    EXPECT_EQ(kh.check(string("bad:port:x"), key).error().code(), net::errc::invalid_address);
    EXPECT_EQ(kh.check(string(""), key).error().code(), net::errc::invalid_address);
    // ssh-keygen -H's hashed form, and ssh-keygen -F finding ours
    auto dir = temp_dir("kh");
    auto file = (dir / "known_hosts").string();
    auto loaded = net::ssh::known_hosts::load(string(file));
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded->size(), 0u);
    ASSERT_TRUE(loaded->add(string("hashed.example:22"), key, true));
    ASSERT_TRUE(loaded->add(string("plain.example:2022"), key));
    EXPECT_TRUE(loaded->check(string("hashed.example"), key));
    auto again = net::ssh::known_hosts::load(string(file));
    EXPECT_EQ(again->size(), 2u);
    EXPECT_TRUE(again->check(string("hashed.example"), key));
    EXPECT_TRUE(again->check(string("plain.example:2022"), key));
    std::string found = run("ssh-keygen -F hashed.example -f " + file + " 2>/dev/null");
    if (!found.empty()) {
        EXPECT_NE(found.find("|1|"), std::string::npos);
    }
    std::string found2 = run("ssh-keygen -F '[plain.example]:2022' -f " + file + " 2>/dev/null");
    if (!found2.empty()) {
        EXPECT_NE(found2.find("plain.example"), std::string::npos);
    }
    // ssh-keygen -H hashes a file of ours, which still matches here
    std::filesystem::copy_file(file, dir / "h", std::filesystem::copy_options::overwrite_existing);
    run("ssh-keygen -H -f " + (dir / "h").string() + " >/dev/null 2>&1");
    auto hashed = net::ssh::known_hosts::load(string((dir / "h").string()));
    EXPECT_TRUE(hashed->check(string("plain.example:2022"), key));
    std::filesystem::remove_all(dir);
}

TEST(SshKnownHosts, CertificateAuthorities) {
    auto cert = *net::ssh::public_key::parse(string(read("host-cert.pub")));
    auto ca = *net::ssh::public_key::parse(string(read("ca.pub")));
    std::string line = "@cert-authority *.example,localhost " + std::string(ca.to_string().view()) + "\n";
    auto kh = net::ssh::known_hosts::parse(string(line));
    EXPECT_TRUE(kh.check(string("localhost:22"), cert));
    // another port is another name ("[localhost]:2200"), which the line does not have
    EXPECT_EQ(kh.check(string("localhost:2200"), cert).error().code(), net::errc::ssh_host_key_unknown);
    auto ported = net::ssh::known_hosts::parse(string("@cert-authority [localhost]:2200 " + std::string(ca.to_string().view()) + "\n"));
    EXPECT_TRUE(ported.check(string("localhost:2200"), cert));   // the principal is the name without the port
    EXPECT_EQ(kh.check(string("host.example"), cert).error().code(), net::errc::ssh_host_key_unknown);   // not a principal
    auto plain = *net::ssh::public_key::parse(string(read("host.pub")));
    EXPECT_EQ(kh.check(string("localhost"), plain).error().code(), net::errc::ssh_host_key_unknown);
    auto revoked = net::ssh::known_hosts::parse(string(line + "@revoked * " + std::string(ca.to_string().view()) + "\n"));
    EXPECT_EQ(revoked.check(string("localhost"), cert).error().code(), net::errc::ssh_host_key_revoked);
    auto user = *net::ssh::public_key::parse(string(read("ed25519-cert.pub")));
    auto users = net::ssh::known_hosts::parse(string("@cert-authority * " + std::string(ca.to_string().view()) + "\n"));
    EXPECT_FALSE(users.check(string("alice"), user));   // a user certificate is no host's
}

// --- authorized_keys ------------------------------------------------------------

TEST(SshAuthorizedKeys, OptionsAndCertificateAuthorities) {
    auto key = *net::ssh::public_key::parse(string(read("p256.pub")));
    auto ca = *net::ssh::public_key::parse(string(read("ca.pub")));
    auto cert = *net::ssh::public_key::parse(string(read("ed25519-cert.pub")));
    std::string text = "# comment\n"
                       "command=\"echo \\\"hi\\\"\",no-pty,from=\"10.0.0.0/8\" " + std::string(key.to_string().view()) + "\n"
                       "cert-authority,principals=\"admin,root\" " + std::string(ca.to_string().view()) + "\n"
                       "broken=\"unterminated " + std::string(key.to_string().view()) + "\n";
    auto ak = net::ssh::authorized_keys::parse(string(text));
    EXPECT_EQ(ak.size(), 2u);
    auto e = ak.find(key);
    ASSERT_TRUE(e);
    EXPECT_EQ(e->option("command"), "echo \"hi\"");
    EXPECT_TRUE(e->has("no-pty"));
    EXPECT_EQ(e->option("from"), "10.0.0.0/8");
    EXPECT_EQ(e->option("absent"), "");
    EXPECT_TRUE(ak.allows(string("anyone"), key));
    EXPECT_TRUE(ak.allows(string("alice"), cert));   // admin is one of the certificate's principals
    EXPECT_FALSE(ak.find(ca));                        // a cert-authority line is no key's own
    auto ak2 = net::ssh::authorized_keys::parse(string("cert-authority " + std::string(ca.to_string().view()) + "\n"));
    EXPECT_TRUE(ak2.allows(string("alice"), cert));
    EXPECT_FALSE(ak2.allows(string("mallory"), cert));
    auto expired = net::ssh::authorized_keys::parse(string("expiry-time=\"20000101\" " + std::string(key.to_string().view()) + "\n"));
    EXPECT_FALSE(expired.allows(string("x"), key));
    auto later = net::ssh::authorized_keys::parse(string("expiry-time=\"29991231\" " + std::string(key.to_string().view()) + "\n"));
    EXPECT_TRUE(later.allows(string("x"), key));
    EXPECT_FALSE(net::ssh::authorized_keys().allows(string("x"), key));
    EXPECT_FALSE(net::ssh::authorized_keys::load(string(data_path("nothing"))));
}
