//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// crypto::hpke (RFC 9180): RFC 9180's base-mode vectors as Go's crypto/hpke
// keeps them (src/crypto/hpke/testdata/rfc9180.json of the Go found on the
// path: pkRm, enc, and a thousand seals and a thousand exports accumulated
// through SHAKE128, as Go's own test reads them); Go's crypto/hpke both ways
// (tests/crypto/go_hpke); the PSK, auth and auth-PSK modes, which Go does
// not have, against RFC 9180 written by hand in Python
// (tests/crypto/hpke_oracle.py, its DHKEM checked against Go's vectors);
// and every method at its boundaries.
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "tests/source_root.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace sgcl;
namespace hpke = sgcl::crypto::hpke;

namespace {
    std::string hex(const slice<const byte>& b) {
        auto s = encoding::hex::encode(b);
        return std::string(s.data(), s.size());
    }

    std::vector<uint8_t> unhex(const std::string& s) {
        std::vector<uint8_t> out(s.size() / 2);
        for (size_t i = 0; i < out.size(); ++i) {
            out[i] = uint8_t(std::stoi(s.substr(2 * i, 2), nullptr, 16));
        }
        return out;
    }

    slice<const byte> bytes(const std::vector<uint8_t>& v) {
        return slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size());
    }

    slice<const byte> bytes(const std::string& s) {
        return slice<const byte>(reinterpret_cast<const byte*>(s.data()), s.size());
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

    std::string goroot() {
        static std::string root = [] {
            if (std::system("command -v go > /dev/null 2>&1") != 0) {
                return std::string();
            }
            return run("go env GOROOT");
        }();
        return root;
    }

    // Go's reader of random inputs: a length byte, then that many bytes
    std::vector<uint8_t> draw(crypto::shake128& source) {
        auto l = source.read(1);
        const size_t n = size_t(l.as_slice()[0]);
        auto b = source.read(n);
        std::vector<uint8_t> out(n);
        for (size_t i = 0; i < n; ++i) {
            out[i] = uint8_t(b.as_slice()[i]);
        }
        return out;
    }

    bool supported(int kem, int kdf, int aead) {
        return (kem == 0x10 || kem == 0x11 || kem == 0x12 || kem == 0x20) && kdf >= 1 && kdf <= 3 && ((aead >= 1 && aead <= 3) || aead == 0xffff);
    }
}

TEST(Hpke, Rfc9180VectorsAsGoKeepsThem) {
    if (goroot().empty()) {
        GTEST_SKIP() << "no go";
    }
    std::ifstream in(std::filesystem::path(goroot()) / "src/crypto/hpke/testdata/rfc9180.json");
    if (!in) {
        GTEST_SKIP() << "no rfc9180.json in Go's tree";
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto doc = encoding::json::parse(string(ss.str()));
    ASSERT_TRUE(doc);
    size_t checked = 0, skipped = 0;
    for (const auto& v : doc->elements()) {
        auto num = [&](const char* name) {
            auto s = v[string(name)];
            return int(s.as_int().value_or(std::stoi(std::string(s.as_string(string("0")).view()))));
        };
        auto field = [&](const char* name) {
            return std::string(v[string(name)].as_string(string()).view());
        };
        const int mode = num("mode"), kem = num("kem_id"), kdf = num("kdf_id"), aead = num("aead_id");
        if (mode != 0 || !supported(kem, kdf, aead)) {
            ++skipped;
            continue;
        }
        const std::string name = "kem " + std::to_string(kem) + " kdf " + std::to_string(kdf) + " aead " + std::to_string(aead);
        const hpke::kem k = hpke::kem(kem);
        const hpke::suite s{hpke::kdf(kdf), hpke::aead(aead)};
        auto info = unhex(field("info"));
        // the recipient's key: derived, deserialized, its public half
        auto pkRm = unhex(field("pkRm"));
        auto skR = hpke::private_key::derive(k, bytes(unhex(field("ikmR"))));
        EXPECT_EQ(hex(skR.public_key().bytes()), field("pkRm")) << name;
        auto skR2 = hpke::private_key::from_bytes(k, bytes(unhex(field("skRm"))));
        ASSERT_TRUE(skR2) << name;
        EXPECT_EQ(hex(skR2->public_key().bytes()), field("pkRm")) << name;
        if (k != hpke::kem::dhkem_x25519) {
            EXPECT_EQ(hex(skR.bytes()), field("skRm")) << name;
        } else {
            // X25519's serialization is clamped (RFC 9180 7.1.2): read back, the same key
            auto again = hpke::private_key::from_bytes(k, skR.bytes());
            EXPECT_EQ(hex(again->public_key().bytes()), field("pkRm")) << name;
        }
        auto pk = hpke::public_key::from_bytes(k, bytes(pkRm));
        ASSERT_TRUE(pk) << name;
        // the sender with the ephemeral key of ikmE
        auto eph = hpke::private_key::derive(k, bytes(unhex(field("ikmE"))));
        auto sender = crypto::detail::HpkeDerandomized::setup(*pk, s, bytes(info), {}, nullptr, eph);
        ASSERT_TRUE(sender) << name;
        EXPECT_EQ(hex(sender->enc()), field("enc")) << name;
        auto recipient = hpke::recipient::setup(sender->enc(), skR, s, bytes(info));
        ASSERT_TRUE(recipient) << name;
        // a thousand seals, each opened, accumulated
        if (s.aead != hpke::aead::export_only) {
            crypto::shake128 source, sink;
            for (int i = 0; i < 1000; ++i) {
                auto aad = draw(source);
                auto pt = draw(source);
                auto ct = sender->seal(bytes(pt), bytes(aad));
                sink.update(ct);
                auto back = recipient->open(ct, bytes(aad));
                ASSERT_TRUE(back) << name << " " << i;
                ASSERT_EQ(back->size(), pt.size());
            }
            EXPECT_EQ(hex(sink.read(16)), field("encryptions_accumulated")) << name;
        } else {
            EXPECT_THROW((void)sender->seal(bytes(std::string("x"))), std::logic_error);
            EXPECT_THROW((void)recipient->open(bytes(std::string("x"))), std::logic_error);
        }
        // a thousand exports of lengths 0..999
        crypto::shake128 source, sink;
        for (size_t l = 0; l < 1000; ++l) {
            auto context = draw(source);
            auto value = sender->export_secret(bytes(context), l);
            sink.update(value);
            EXPECT_TRUE(recipient->export_secret(bytes(context), l) == value) << name << " " << l;
        }
        EXPECT_EQ(hex(sink.read(16)), field("exports_accumulated")) << name;
        ++checked;
    }
    EXPECT_GE(checked, 16u);   // and 8 of P-521, not in the module yet
    std::printf("[ rfc9180 ] %zu suites checked, %zu skipped (modes Go does not keep, KEMs not here)\n", checked, skipped);
}

// --- Go's crypto/hpke both ways --------------------------------------------------

namespace {
    const std::string& go_hpke() {
        static std::string path = [] {
            if (goroot().empty()) {
                return std::string();
            }
            auto dir = std::filesystem::temp_directory_path() / "sgcl_hpke_go";
            std::filesystem::create_directories(dir);
            std::filesystem::copy_file(source_root() / "tests/crypto/go_hpke/main.go", dir / "main.go", std::filesystem::copy_options::overwrite_existing);
            std::ofstream(dir / "go.mod") << "module hpkeoracle\n\ngo 1.26\n";
            auto out = dir / "go_hpke";
            std::string cmd = "cd '" + dir.string() + "' && GOFLAGS=-mod=mod GOPROXY=off GOSUMDB=off go build -o '" + out.string() + "' . > build.log 2>&1";
            if (std::system(cmd.c_str()) != 0) {
                return std::string();
            }
            return out.string();
        }();
        return path;
    }
}

TEST(HpkeGo, SingleShotBothWays) {
    if (go_hpke().empty()) {
        GTEST_SKIP() << "no go with crypto/hpke";
    }
    for (auto k : {hpke::kem::dhkem_x25519, hpke::kem::dhkem_p256, hpke::kem::dhkem_p384, hpke::kem::dhkem_p521}) {
        for (auto f : {hpke::kdf::hkdf_sha256, hpke::kdf::hkdf_sha384, hpke::kdf::hkdf_sha512}) {
            for (auto a : {hpke::aead::aes128_gcm, hpke::aead::aes256_gcm, hpke::aead::chacha20_poly1305}) {
                const hpke::suite s{f, a};
                const std::string ids = std::to_string(int(k)) + " " + std::to_string(int(f)) + " " + std::to_string(int(a));
                auto key = hpke::private_key::generate(k);
                const std::string pt = "message of suite " + ids;
                const std::string info = "info " + ids;
                // ours, opened by Go
                auto sealed = hpke::seal(key.public_key(), bytes(pt), s, bytes(info));
                ASSERT_TRUE(sealed);
                EXPECT_EQ(run(go_hpke() + " open " + ids + " " + hex(key.bytes()) + " " + hex(bytes(info)) + " " + hex(*sealed)), "ok " + hex(bytes(pt))) << ids;
                // Go's, opened by us
                auto theirs = run(go_hpke() + " seal " + ids + " " + hex(key.public_key().bytes()) + " " + hex(bytes(info)) + " " + hex(bytes(pt)));
                auto opened = hpke::open(key, bytes(unhex(theirs)), s, bytes(info));
                ASSERT_TRUE(opened) << ids << ": " << opened.error().message().view();
                EXPECT_EQ(std::string(reinterpret_cast<const char*>(opened->data()), opened->size()), pt) << ids;
                // Go's key, read here
                auto pair = run(go_hpke() + " keygen " + std::to_string(int(k)));
                auto sp = pair.find(' ');
                auto sk = hpke::private_key::from_bytes(k, bytes(unhex(pair.substr(0, sp))));
                ASSERT_TRUE(sk) << ids;
                EXPECT_EQ(hex(sk->public_key().bytes()), pair.substr(sp + 1)) << ids;
            }
        }
    }
}

// --- the modes Go does not have -----------------------------------------------

TEST(Hpke, ModesAgainstThePythonOracle) {
    if (std::system("command -v python3 > /dev/null 2>&1") != 0) {
        GTEST_SKIP() << "no python3";
    }
    const std::string script = (source_root() / "tests/crypto/hpke_oracle.py").string();
    const std::string ikmR = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    const std::string ikmE = "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";
    const std::string ikmS = "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f";
    const std::string psk = "0247fd33b913760fa1fa51e1892d9f307fbe65eb171e8132c2af18555a738b82";
    const std::string psk_id = "456e6e796e20447572696e206172616e204d6f726961";
    const std::string info = "4f6465206f6e2061204772656369616e2055726e";
    auto skR = hpke::private_key::derive(hpke::kem::dhkem_x25519, bytes(unhex(ikmR)));
    auto skS = hpke::private_key::derive(hpke::kem::dhkem_x25519, bytes(unhex(ikmS)));
    auto pkS = skS.public_key();
    auto psk_b = unhex(psk);
    auto psk_id_b = unhex(psk_id);
    for (int mode = 0; mode < 4; ++mode) {
        for (auto f : {hpke::kdf::hkdf_sha256, hpke::kdf::hkdf_sha384, hpke::kdf::hkdf_sha512}) {
            for (auto a : {hpke::aead::aes128_gcm, hpke::aead::aes256_gcm, hpke::aead::chacha20_poly1305, hpke::aead::export_only}) {
                const hpke::suite s{f, a};
                const bool with_psk = mode & 1, auth = mode & 2;
                const std::string args = std::to_string(mode) + " " + std::to_string(int(f)) + " " + std::to_string(int(a)) + " " + ikmR + " " + ikmE + " " +
                                         (auth ? ikmS : "-") + " " + (with_psk ? psk : "-") + " " + (with_psk ? psk_id : "-") + " " + info;
                std::istringstream lines(run("python3 '" + script + "' " + args));
                std::string pkRm, enc, key, nonce, exporter, exported;
                lines >> pkRm >> enc >> key >> nonce >> exporter >> exported;
                ASSERT_FALSE(exported.empty()) << args;
                hpke::options so;
                hpke::options ro;
                if (with_psk) {
                    so.psk = ro.psk = bytes(psk_b);
                    so.psk_id = ro.psk_id = bytes(psk_id_b);
                }
                auto eph = hpke::private_key::derive(hpke::kem::dhkem_x25519, bytes(unhex(ikmE)));
                auto sender = crypto::detail::HpkeDerandomized::setup(skR.public_key(), s, bytes(unhex(info)), so, auth ? &skS : nullptr, eph);
                ASSERT_TRUE(sender) << args;
                EXPECT_EQ(hex(sender->enc()), enc) << args;
                EXPECT_EQ(hex(sender->export_secret(bytes(std::string("context")), 32)), exported) << args;
                auto recipient = auth ? hpke::recipient::setup(sender->enc(), skR, s, bytes(unhex(info)), ro, pkS)
                                      : hpke::recipient::setup(sender->enc(), skR, s, bytes(unhex(info)), ro);
                ASSERT_TRUE(recipient) << args;
                EXPECT_EQ(hex(recipient->export_secret(bytes(std::string("context")), 32)), exported) << args;
                if (a == hpke::aead::export_only) {
                    continue;
                }
                // the key and the nonce: the first message opens under the oracle's
                auto ct = sender->seal(bytes(std::string("first")), bytes(std::string("aad")));
                auto k = unhex(key);
                auto n = unhex(nonce);
                expected<vector<byte>, crypto::error> by_oracle = a == hpke::aead::chacha20_poly1305
                    ? crypto::chacha20_poly1305(bytes(k)).open(bytes(n), ct, bytes(std::string("aad")))
                    : crypto::aes_gcm(bytes(k)).open(bytes(n), ct, bytes(std::string("aad")));
                ASSERT_TRUE(by_oracle) << args;
                auto mine = recipient->open(ct, bytes(std::string("aad")));
                ASSERT_TRUE(mine) << args;
                // the wrong mode on the other side: nothing opens
                auto wrong = hpke::recipient::setup(sender->enc(), skR, s, bytes(unhex(info)), mode == 0 ? ro : hpke::options());
                if (mode != 0) {
                    ASSERT_TRUE(wrong);
                    auto ct2 = sender->seal(bytes(std::string("second")));
                    EXPECT_FALSE(wrong->open(ct2)) << args;
                }
            }
        }
    }
}

TEST(Hpke, Boundaries) {
    const hpke::suite s;
    auto key = hpke::private_key::generate(hpke::kem::dhkem_x25519);
    // single shot: empty plaintext, aad and info; a megabyte
    auto e = hpke::seal(key.public_key(), slice<const byte>());
    ASSERT_TRUE(e);
    auto eo = hpke::open(key, *e);
    ASSERT_TRUE(eo);
    EXPECT_TRUE(eo->empty());
    std::string big(1 << 20, 'b');
    auto b = hpke::seal(key.public_key(), bytes(big), s, bytes(std::string("i")), bytes(std::string("a")));
    EXPECT_EQ(hpke::open(key, *b, s, bytes(std::string("i")), bytes(std::string("a")))->size(), big.size());
    EXPECT_FALSE(hpke::open(key, *b, s, bytes(std::string("j")), bytes(std::string("a"))));
    EXPECT_FALSE(hpke::open(key, *b, s, bytes(std::string("i")), bytes(std::string("b"))));
    EXPECT_FALSE(hpke::open(key, b->as_slice().subslice(0, 31)));
    EXPECT_FALSE(hpke::open(key, b->as_slice().subslice(0, 32)));
    // order: the recipient opens in the order sealed, a failure keeps its place
    auto snd = hpke::sender::setup(key.public_key(), s);
    ASSERT_TRUE(snd);
    auto rcv = hpke::recipient::setup(snd->enc(), key, s);
    ASSERT_TRUE(rcv);
    auto c1 = snd->seal(bytes(std::string("1")));
    auto c2 = snd->seal(bytes(std::string("2")));
    EXPECT_FALSE(rcv->open(c2));
    EXPECT_TRUE(rcv->open(c1));
    EXPECT_TRUE(rcv->open(c2));
    EXPECT_FALSE(rcv->open(c2));
    // moved from: a broken contract
    auto moved = std::move(*snd);
    EXPECT_THROW((void)snd->seal(bytes(std::string("x"))), std::logic_error);
    EXPECT_THROW((void)snd->export_secret(slice<const byte>(), 1), std::logic_error);
    EXPECT_NO_THROW((void)moved.seal(bytes(std::string("x"))));
    // exports: up to 255 Nh, and zero
    EXPECT_EQ(moved.export_secret(slice<const byte>(), 0).size(), 0u);
    EXPECT_EQ(moved.export_secret(slice<const byte>(), 255 * 32).size(), 255u * 32);
    EXPECT_THROW((void)moved.export_secret(slice<const byte>(), 255 * 32 + 1), std::invalid_argument);
    // keys: wrong sizes, points off the curve, a suite of another KEM
    EXPECT_FALSE(hpke::public_key::from_bytes(hpke::kem::dhkem_x25519, bytes(std::string(31, 'x'))));
    EXPECT_FALSE(hpke::public_key::from_bytes(hpke::kem::dhkem_p256, bytes(std::string(65, '\x04'))));
    auto p256 = hpke::private_key::generate(hpke::kem::dhkem_p256);
    auto compressed = p256.public_key().bytes();
    EXPECT_FALSE(hpke::public_key::from_bytes(hpke::kem::dhkem_p256, compressed.as_slice().subslice(0, 33)));
    EXPECT_FALSE(hpke::private_key::from_bytes(hpke::kem::dhkem_p256, bytes(std::string(32, '\0'))));
    EXPECT_FALSE(hpke::private_key::from_bytes(hpke::kem::dhkem_p256, bytes(std::string(32, '\xff'))));
    EXPECT_FALSE(hpke::recipient::setup(moved.enc(), p256, s));   // an X25519 enc to a P-256 key
    auto p256_pub = p256.public_key();
    EXPECT_FALSE(hpke::recipient::setup(moved.enc(), key, s, slice<const byte>(), p256_pub));
    // X25519 of small order: the zero shared secret is refused on both sides
    auto zero = hpke::public_key::from_bytes(hpke::kem::dhkem_x25519, bytes(std::string(32, '\0')));
    ASSERT_TRUE(zero);
    auto z = hpke::sender::setup(*zero, s);
    ASSERT_FALSE(z);
    EXPECT_EQ(z.error().code(), crypto::errc::invalid_key);
    EXPECT_FALSE(hpke::recipient::setup(bytes(std::string(32, '\0')), key, s));
    // PSK given half
    const std::string psk(32, 'p');
    EXPECT_THROW((void)hpke::sender::setup(key.public_key(), s, slice<const byte>(), {.psk = bytes(psk)}), std::invalid_argument);
    auto half = hpke::recipient::setup(moved.enc(), key, s, slice<const byte>(), {.psk_id = bytes(psk)});
    ASSERT_FALSE(half);
    EXPECT_EQ(half.error().code(), crypto::errc::malformed);
    // an auth sender's key of another KEM
    EXPECT_THROW((void)hpke::sender::setup(key.public_key(), s, slice<const byte>(), p256), std::invalid_argument);
    // ids of no value
    EXPECT_THROW((void)hpke::private_key::generate(hpke::kem(0x99)), std::invalid_argument);
    EXPECT_FALSE(hpke::recipient::setup(moved.enc(), key, {.kdf = hpke::kdf(9)}));
    // a key and its bytes both ways; clone
    auto again = hpke::private_key::from_bytes(key.kem(), key.bytes());
    ASSERT_TRUE(again);
    EXPECT_TRUE(again->public_key() == key.public_key());
    EXPECT_TRUE(key.clone().public_key() == key.public_key());
    // derive: the same key every time
    EXPECT_TRUE(hpke::private_key::derive(hpke::kem::dhkem_p384, bytes(psk)).public_key() == hpke::private_key::derive(hpke::kem::dhkem_p384, bytes(psk)).public_key());
    EXPECT_TRUE(hpke::private_key::derive(hpke::kem::dhkem_p521, bytes(psk)).public_key() == hpke::private_key::derive(hpke::kem::dhkem_p521, bytes(psk)).public_key());
    EXPECT_FALSE(hpke::public_key::from_bytes(hpke::kem::dhkem_p521, bytes(std::string(132, '\x04'))));
}
