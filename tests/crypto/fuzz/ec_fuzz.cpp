//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// P-256, P-384 and P-521 on any bytes. The input's first byte chooses the
// curve (its low two bits: 0 and 3 P-256, 1 P-384, 2 P-521) and what the
// rest is (the byte / 4 mod 5):
//
//   0  a SEC 1 point: public_key::from_bytes must agree with OpenSSL's
//      EC_POINT_oct2point; a point it takes must give back its own bytes,
//      and its compressed form must decode to it
//   1  a digest of the curve's size and a signature: verify_digest (DER)
//      and verify_digest_raw under a fixed key must be true only for the
//      one valid input made in the harness (a valid seed), never for
//      anything else, and never disagree with OpenSSL on DER
//   2  a SubjectPublicKeyInfo: from_pkix_der; what it takes, written back
//      by to_pkix_der, must read to the same key
//   3  a PKCS#8 key, 4 a SEC 1 key: from_pkcs8_der / from_sec1_der; what
//      they take must write back and read to the same scalar
//
// A disagreement aborts; ASan and UBSan catch the rest.
//
//   clang++ -std=c++20 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
//       -I<repo> -I/opt/homebrew/opt/openssl@3/include tests/fuzz/driver.cpp \
//       tests/crypto/fuzz/ec_fuzz.cpp /opt/homebrew/opt/openssl@3/lib/libcrypto.dylib -o ec_fuzz
//   ASAN_OPTIONS=abort_on_error=1 ./ec_fuzz <seconds> <seed files...>
//
// sgcl_ec_fuzz_seeds(dir), called from a main of one line linked with this
// file instead of the driver, writes a valid input of each kind for the
// three curves into dir: the seeds a run starts from.
#include "sgcl/crypto/crypto.h"

#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    using bytes_t = std::vector<unsigned char>;

    slice<const byte> view(const uint8_t* p, size_t n) {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    template<class R>
    bytes_t to_bytes(const R& r) {
        const unsigned char* p = reinterpret_cast<const unsigned char*>(r.data());
        return bytes_t(p, p + r.size());
    }

    // a private key's DER comes in a secret_bytes
    bytes_t to_bytes(const crypto::secret_bytes& s) {
        return to_bytes(s.as_slice());
    }

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "ec_fuzz: %s\n", what);
            std::abort();
        }
    }

    bool ossl_point_valid(int nid, const uint8_t* p, size_t n) {
        EC_GROUP* g = EC_GROUP_new_by_curve_name(nid);
        EC_POINT* pt = EC_POINT_new(g);
        bool ok = n != 0 && EC_POINT_oct2point(g, pt, p, n, nullptr) == 1 && EC_POINT_is_at_infinity(g, pt) == 0;
        EC_POINT_free(pt);
        EC_GROUP_free(g);
        return ok;
    }

    template<class Curve>
    struct Target {
        using SK = typename Curve::private_key;
        using PK = typename Curve::public_key;
        static constexpr size_t size = SK::size;

        // The fixed key of the verification target and its one valid input
        static const SK& key() {
            static const SK k = [] {
                unsigned char d[size];
                for (size_t i = 0; i < size; ++i) {
                    d[i] = static_cast<unsigned char>(0x11 * (i + 1));
                }
                if (size == 66) {
                    d[0] = 0x01;   // P-521: 521 bits, one in the top byte
                }
                return std::move(*SK::from_bytes(view(d, size)));
            }();
            return k;
        }

        static const bytes_t& valid_input() {
            static const bytes_t v = [] {
                bytes_t in(size);
                for (size_t i = 0; i < size; ++i) {
                    in[i] = static_cast<unsigned char>(i * 7 + 3);
                }
                // RFC 6979's deterministic signature, so that the seeds
                // written by another process hold this very signature
                using Ecdsa = crypto::detail::Ecdsa<typename Curve::curve>;
                unsigned char d[size];
                unsigned char raw[2 * size];
                unsigned char der[Ecdsa::max_der_size];
                auto secret = key().bytes();
                std::memcpy(d, secret.bytes().data(), size);
                Ecdsa::template sign<crypto::sha256>(raw, crypto::detail::ec_from_be<typename Curve::curve>(d), in.data(), size, nullptr, 0);
                size_t n = Ecdsa::encode_signature(der, raw);
                in.insert(in.end(), der, der + n);
                return in;
            }();
            return v;
        }

        static EVP_PKEY* ossl_key() {
            static EVP_PKEY* k = [] {
                bytes_t spki = to_bytes(key().public_key().to_pkix_der());
                const unsigned char* p = spki.data();
                return d2i_PUBKEY(nullptr, &p, long(spki.size()));
            }();
            return k;
        }

        static void point(const uint8_t* p, size_t n) {
            auto k = PK::from_bytes(view(p, n));
            check(k.has_value() == ossl_point_valid(Curve::nid, p, n), "from_bytes disagrees with OpenSSL");
            if (k) {
                auto u = k->bytes();
                auto c = k->bytes_compressed();
                if (n == u.size()) {
                    check(std::memcmp(u.data(), p, n) == 0, "a point does not give back its bytes");
                } else {
                    check(std::memcmp(c.data(), p, n) == 0, "a compressed point does not give back its bytes");
                }
                auto again = PK::from_bytes(c.as_slice());
                check(again.has_value() && *again == *k, "the compressed form does not decode to the point");
            }
        }

        static void verify(const uint8_t* p, size_t n) {
            if (n < size) {
                return;
            }
            auto pub = key().public_key();
            bool der = pub.verify_digest(view(p, size), view(p + size, n - size));
            bool raw = pub.verify_digest_raw(view(p, size), view(p + size, n - size));
            const bytes_t& valid = valid_input();
            bool is_valid = n == valid.size() && std::memcmp(p, valid.data(), n) == 0;
            check(der == is_valid, der ? "verify_digest took a forgery" : "verify_digest refused the valid signature");
            check(!raw, "verify_digest_raw took a forgery");
            EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(ossl_key(), nullptr);
            EVP_PKEY_verify_init(ctx);
            bool ossl = EVP_PKEY_verify(ctx, p + size, n - size, p, size) == 1;
            EVP_PKEY_CTX_free(ctx);
            // OpenSSL checks DER strictly too (it encodes what it read
            // again and compares)
            check(ossl == der, "verify_digest disagrees with OpenSSL");
        }

        static void spki(const uint8_t* p, size_t n) {
            auto k = PK::from_pkix_der(view(p, n));
            if (k) {
                auto der = k->to_pkix_der();
                auto again = PK::from_pkix_der(der.as_slice());
                check(again.has_value() && *again == *k, "an SPKI does not read back");
            }
        }

        static void pkcs8(const uint8_t* p, size_t n, bool sec1) {
            auto k = sec1 ? SK::from_sec1_der(view(p, n)) : SK::from_pkcs8_der(view(p, n));
            if (k) {
                auto der = sec1 ? k->to_sec1_der() : k->to_pkcs8_der();
                auto again = sec1 ? SK::from_sec1_der(der.as_slice()) : SK::from_pkcs8_der(der.as_slice());
                check(again.has_value(), "a private key does not read back");
                auto a = k->bytes();
                auto b = again->bytes();
                check(std::memcmp(a.bytes().data(), b.bytes().data(), size) == 0, "a private key reads back to another scalar");
                check(k->public_key() == again->public_key(), "a private key reads back to another point");
            }
        }

        static void run(unsigned what, const uint8_t* p, size_t n) {
            switch (what) {
                case 0: point(p, n); break;
                case 1: verify(p, n); break;
                case 2: spki(p, n); break;
                case 3: pkcs8(p, n, false); break;
                case 4: pkcs8(p, n, true); break;
            }
        }

        static void seeds(const std::string& dir, unsigned curve_bit) {
            auto write = [&](unsigned what, const bytes_t& body) {
                bytes_t in = {static_cast<unsigned char>(what * 4 + curve_bit)};
                in.insert(in.end(), body.begin(), body.end());
                std::ofstream(dir + "/seed-" + std::to_string(curve_bit) + "-" + std::to_string(what) + ".bin", std::ios::binary)
                    .write(reinterpret_cast<const char*>(in.data()), std::streamsize(in.size()));
            };
            write(0, to_bytes(key().public_key().bytes()));
            write(1, valid_input());
            write(2, to_bytes(key().public_key().to_pkix_der()));
            write(3, to_bytes(key().to_pkcs8_der()));
            write(4, to_bytes(key().to_sec1_der()));
        }
    };

    struct P256 {
        using private_key = crypto::p256::private_key;
        using public_key = crypto::p256::public_key;
        using curve = crypto::detail::P256;
        static constexpr int nid = NID_X9_62_prime256v1;
    };

    struct P384 {
        using private_key = crypto::p384::private_key;
        using public_key = crypto::p384::public_key;
        using curve = crypto::detail::P384;
        static constexpr int nid = NID_secp384r1;
    };

    struct P521 {
        using private_key = crypto::p521::private_key;
        using public_key = crypto::p521::public_key;
        using curve = crypto::detail::P521;
        static constexpr int nid = NID_secp521r1;
    };
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    unsigned what = (data[0] >> 2) % 5;
    switch (data[0] & 3) {
        case 1: Target<P384>::run(what, data + 1, size - 1); break;
        case 2: Target<P521>::run(what, data + 1, size - 1); break;
        default: Target<P256>::run(what, data + 1, size - 1); break;
    }
    return 0;
}

// The seeds (see the top of the file)
extern "C" int sgcl_ec_fuzz_seeds(const char* dir) {
    Target<P256>::seeds(dir, 0);
    Target<P384>::seeds(dir, 1);
    Target<P521>::seeds(dir, 2);
    return 0;
}
