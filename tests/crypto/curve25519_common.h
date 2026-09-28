//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// What the tests of X25519 and Ed25519 share: hex, bytes of a fixed seed,
// OpenSSL 3 as the oracle (EVP_PKEY_X25519 and EVP_PKEY_ED25519, raw keys,
// derive, sign, verify, the DER encoders), a slow reference of Ed25519's
// verification written on math::big_integer in affine coordinates — no
// code shared with the module's, so that it can judge the cases OpenSSL
// and Go judge otherwise — and the files of other oracles, read when they
// are on disk and skipped when not.
#pragma once

#include "tests/types.h"
#include "sgcl/crypto/crypto.h"
#include "sgcl/math/big_integer.h"

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace curve_test {
    namespace crypto = sgcl::crypto;
    using bytes_t = std::vector<unsigned char>;

    inline sgcl::slice<const byte> view(const unsigned char* p, size_t n) {
        return sgcl::slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    inline sgcl::slice<const byte> view(const bytes_t& v) {
        return view(v.data(), v.size());
    }

    template<class R>
    requires requires(const R& r) { r.data(); r.size(); }
    bytes_t to_bytes(const R& r) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    inline std::string hex(const unsigned char* p, size_t n) {
        static const char digits[] = "0123456789abcdef";
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            s += digits[p[i] >> 4];
            s += digits[p[i] & 15];
        }
        return s;
    }

    template<class R>
    requires requires(const R& r) { r.data(); r.size(); }
    std::string hex(const R& r) {
        return hex(reinterpret_cast<const unsigned char*>(r.data()), r.size());
    }

    // a secret<N> read through its view
    template<size_t N>
    bytes_t to_bytes(const crypto::secret<N>& s) {
        return to_bytes(s.bytes());
    }

    template<size_t N>
    std::string hex(const crypto::secret<N>& s) {
        return hex(s.bytes());
    }

    inline bytes_t unhex(const std::string& s) {
        bytes_t v;
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            throw std::runtime_error("not hex");
        };
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            v.push_back((unsigned char)(nibble(s[i]) << 4 | nibble(s[i + 1])));
        }
        return v;
    }

    // Bytes of a generator with a fixed seed, so that a failure repeats
    struct random_source {
        std::mt19937_64 g;

        explicit random_source(uint64_t seed) : g(seed) {}

        bytes_t bytes(size_t n) {
            bytes_t v(n);
            for (auto& b : v) {
                b = (unsigned char)g();
            }
            return v;
        }

        size_t below(size_t n) {
            return size_t(g() % n);
        }
    };

    // --- OpenSSL --------------------------------------------------------------

    struct pkey {
        EVP_PKEY* p = nullptr;
        explicit pkey(EVP_PKEY* k) : p(k) {}
        pkey(const pkey&) = delete;
        ~pkey() { EVP_PKEY_free(p); }
    };

    inline bytes_t ossl_raw_public(EVP_PKEY* k) {
        bytes_t out(32);
        size_t n = 32;
        if (EVP_PKEY_get_raw_public_key(k, out.data(), &n) != 1 || n != 32) {
            throw std::runtime_error("EVP_PKEY_get_raw_public_key");
        }
        return out;
    }

    // X25519(priv, pub) by OpenSSL, or empty when it refuses (the all-zero
    // result, which OpenSSL checks as RFC 7748 §6.1 asks)
    inline bytes_t ossl_x25519(const bytes_t& priv, const bytes_t& pub) {
        pkey a(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, priv.data(), 32));
        pkey b(EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr, pub.data(), 32));
        EVP_PKEY_CTX* c = EVP_PKEY_CTX_new(a.p, nullptr);
        bytes_t out(32);
        size_t n = 32;
        bool ok = EVP_PKEY_derive_init(c) == 1 && EVP_PKEY_derive_set_peer(c, b.p) == 1
               && EVP_PKEY_derive(c, out.data(), &n) == 1 && n == 32;
        EVP_PKEY_CTX_free(c);
        return ok ? out : bytes_t();
    }

    inline bytes_t ossl_x25519_public(const bytes_t& priv) {
        pkey a(EVP_PKEY_new_raw_private_key(EVP_PKEY_X25519, nullptr, priv.data(), 32));
        return ossl_raw_public(a.p);
    }

    inline bytes_t ossl_ed25519_public(const bytes_t& seed) {
        pkey a(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), 32));
        return ossl_raw_public(a.p);
    }

    inline bytes_t ossl_ed25519_sign(const bytes_t& seed, const bytes_t& msg) {
        pkey a(EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, nullptr, seed.data(), 32));
        EVP_MD_CTX* c = EVP_MD_CTX_new();
        bytes_t sig(64);
        size_t n = 64;
        static const unsigned char none = 0;
        bool ok = EVP_DigestSignInit(c, nullptr, nullptr, nullptr, a.p) == 1
               && EVP_DigestSign(c, sig.data(), &n, msg.empty() ? &none : msg.data(), msg.size()) == 1;
        EVP_MD_CTX_free(c);
        if (!ok) {
            throw std::runtime_error("EVP_DigestSign");
        }
        return sig;
    }

    inline bool ossl_ed25519_verify(const bytes_t& pub, const bytes_t& msg, const bytes_t& sig) {
        pkey a(EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub.data(), pub.size()));
        if (!a.p) {
            return false;
        }
        EVP_MD_CTX* c = EVP_MD_CTX_new();
        static const unsigned char none = 0;
        bool ok = EVP_DigestVerifyInit(c, nullptr, nullptr, nullptr, a.p) == 1
               && EVP_DigestVerify(c, sig.data(), sig.size(), msg.empty() ? &none : msg.data(), msg.size()) == 1;
        EVP_MD_CTX_free(c);
        return ok;
    }

    // The DER forms as OpenSSL writes them: SubjectPublicKeyInfo and PKCS #8
    inline bytes_t ossl_pkix(int type, const bytes_t& pub) {
        pkey a(EVP_PKEY_new_raw_public_key(type, nullptr, pub.data(), 32));
        unsigned char* p = nullptr;
        int n = i2d_PUBKEY(a.p, &p);
        bytes_t out(p, p + n);
        OPENSSL_free(p);
        return out;
    }

    inline bytes_t ossl_pkcs8(int type, const bytes_t& priv) {
        pkey a(EVP_PKEY_new_raw_private_key(type, nullptr, priv.data(), 32));
        PKCS8_PRIV_KEY_INFO* info = EVP_PKEY2PKCS8(a.p);
        unsigned char* p = nullptr;
        int n = i2d_PKCS8_PRIV_KEY_INFO(info, &p);
        bytes_t out(p, p + n);
        OPENSSL_free(p);
        PKCS8_PRIV_KEY_INFO_free(info);
        return out;
    }

    // The raw private key OpenSSL reads from a PKCS #8, or empty
    inline bytes_t ossl_read_pkcs8(const bytes_t& der) {
        const unsigned char* p = der.data();
        PKCS8_PRIV_KEY_INFO* info = d2i_PKCS8_PRIV_KEY_INFO(nullptr, &p, long(der.size()));
        if (!info) {
            return {};
        }
        EVP_PKEY* k = EVP_PKCS82PKEY(info);
        PKCS8_PRIV_KEY_INFO_free(info);
        if (!k) {
            return {};
        }
        bytes_t out(32);
        size_t n = 32;
        bool ok = EVP_PKEY_get_raw_private_key(k, out.data(), &n) == 1;
        EVP_PKEY_free(k);
        return ok ? out : bytes_t();
    }

    // --- a slow reference of Ed25519's verification ---------------------------

    // RFC 8032 §5.1 in affine coordinates on big integers, the strict
    // decoding (§5.1.3) and the equation without the cofactor, [S]B = R +
    // [k]A, compared as points: what the module promises, computed by other
    // means. Milliseconds a verification; for a few hundred cases.
    struct reference {
        using big = sgcl::math::big_integer;
        big p, L, d, sqrtm1;
        big bx, by;

        static big from_le(const unsigned char* s, size_t n) {
            std::vector<unsigned char> be(s, s + n);
            std::reverse(be.begin(), be.end());
            return big::from_bytes(view(be));
        }

        static big hex_big(const char* h) {
            return *big::parse(sgcl::string(h), 16);
        }

        reference() {
            p = (big(1) << 255) - 19;
            L = (big(1) << 252) + hex_big("14def9dea2f79cd65812631a5cf5d3ed");
            d = (big(-121665) * *big(121666).mod_inverse(p)).mod(p);
            sqrtm1 = big(2).mod_pow((p - 1) / 4, p);
            by = (big(4) * *big(5).mod_inverse(p)).mod(p);
            bx = *recover_x(by, 0);
        }

        big inv(const big& x) const {
            return x.mod_pow(p - 2, p);
        }

        optional<big> recover_x(const big& y, int sign) const {
            big x2 = ((y * y - 1) * inv(d * y * y + 1)).mod(p);
            if (x2 == 0) {
                if (sign) {
                    return nullopt;
                }
                return big(0);
            }
            big x = x2.mod_pow((p + 3) / 8, p);
            if ((x * x - x2).mod(p) != 0) {
                x = (x * sqrtm1).mod(p);
            }
            if ((x * x - x2).mod(p) != 0) {
                return nullopt;
            }
            if (int(x.bit(0)) != sign) {
                x = p - x;
            }
            return x;
        }

        struct point {
            big x, y;
        };

        optional<point> decode(const unsigned char* s) const {
            unsigned char c[32];
            std::memcpy(c, s, 32);
            int sign = c[31] >> 7;
            c[31] &= 0x7f;
            big y = from_le(c, 32);
            if (y >= p) {
                return nullopt;
            }
            auto x = recover_x(y, sign);
            if (!x) {
                return nullopt;
            }
            return point{*x, y};
        }

        point add(const point& a, const point& b) const {
            big t = (d * a.x * b.x * a.y * b.y).mod(p);
            big x = ((a.x * b.y + b.x * a.y) * inv((t + 1).mod(p))).mod(p);
            big y = ((a.y * b.y + a.x * b.x) * inv((big(1) - t).mod(p))).mod(p);
            return point{x, y};
        }

        point mul(big k, point a) const {
            point r{big(0), big(1)};
            while (k != 0) {
                if (k.bit(0)) {
                    r = add(r, a);
                }
                a = add(a, a);
                k >>= 1;
            }
            return r;
        }

        bool verify(const bytes_t& pub, const bytes_t& msg, const bytes_t& sig) const {
            if (pub.size() != 32 || sig.size() != 64) {
                return false;
            }
            auto a = decode(pub.data());
            auto r = decode(sig.data());
            big s = from_le(sig.data() + 32, 32);
            if (!a || !r || s >= L) {
                return false;
            }
            crypto::sha512 h;
            h.update(view(sig.data(), 32));
            h.update(view(pub));
            h.update(view(msg));
            auto digest = h.value();
            big k = from_le(reinterpret_cast<const unsigned char*>(digest.data()), 64).mod(L);
            point left = mul(s, point{bx, by});
            point right = add(*r, mul(k, *a));
            return left.x == right.x && left.y == right.y;
        }
    };

    // --- files of other oracles -------------------------------------------------

    // A file under ~/Programming/oracles (or under $SGCL_ORACLES when it is
    // set), or nothing when it is not there
    inline optional<std::string> oracle_file(const std::string& path) {
        std::string root;
        if (const char* o = std::getenv("SGCL_ORACLES")) {
            root = o;
        } else if (const char* home = std::getenv("HOME")) {
            root = std::string(home) + "/Programming/oracles";
        } else {
            return nullopt;
        }
        std::ifstream in(root + "/" + path, std::ios::binary);
        if (!in) {
            return nullopt;
        }
        std::stringstream s;
        s << in.rdbuf();
        return s.str();
    }
}
