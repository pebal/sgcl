//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The crypto module against OpenSSL: what a digest of a buffer costs, one
// algorithm, one side and one length a run. Prints one line: ns per call
// and MB/s.
//
//   crypto <case> <sgcl|openssl> [length=1024]
//
//   sha1 sha256 sha512 sha3_256     one-shot digests: type::of(data) against
//                                   EVP_Digest with a fetched EVP_MD
//   sha256-portable sha512-portable
//   sha3_256-portable               the plain C++ compression over the same
//                                   bytes (sgcl only): the road a processor
//                                   without the instructions takes
//   blake2b_512 blake2s_256         BLAKE2 one-shot digests (RFC 7693):
//                                   type::of(data) against EVP_Digest
//   blake2b_mac blake2s_mac         keyed BLAKE2 with the 32-byte key:
//                                   blake2b_256 and blake2s_128 made with
//                                   it, against EVP_MAC BLAKE2BMAC/BLAKE2SMAC
//                                   of the same sizes (Go: New256(key),
//                                   New128(key), in a scratch module)
//   blake3                          BLAKE3's hash in one call: blake3::of
//                                   against the authors' C library (side
//                                   `reference`, when the build finds it:
//                                   SGCL_BENCH_BLAKE3); OpenSSL has none
//   blake3-xof                      `length` bytes of BLAKE3's output over
//                                   "abc" (value_to against finalize)
//   argon2id                        Argon2id of a 28-byte password, t = 3,
//                                   p = 4, `length` KiB of memory (65536:
//                                   RFC 9106's second option), against
//                                   OpenSSL's ARGON2ID with four threads
//                                   (Go: argon2.IDKey, in a scratch module)
//   scrypt                          scrypt of a 28-byte password, r = 8,
//                                   p = 1, N = `length` (16384: 16 MiB),
//                                   against EVP_PBE_scrypt (Go: scrypt.Key,
//                                   in a scratch module)
//   hmac_sha256                     a tag with a fixed key: hmac_sha256::of
//                                   against HMAC() (sgcl makes the keyed
//                                   states in each call, as HMAC() does)
//   aes128gcm-seal aes256gcm-seal   an AEAD over the buffer with 13 bytes
//   chachapoly-seal                 of aad, as TLS has: seal_to into a
//   aes128gcm-open aes256gcm-open   buffer made once against EVP with one
//   chachapoly-open                 context keyed once and the nonce set
//                                   per message; open checks the tag
//   aes256cbc-decrypt               CBC decryption of the buffer (whole
//   aes256cbc-encrypt               blocks) in place, the chain carried from
//                                   call to call: aes_cbc's decrypt_blocks
//                                   (eight blocks at a time) and
//                                   encrypt_blocks (one chain), against
//                                   EVP_aes_256_cbc without padding (Go:
//                                   benchmarks/go/crypto, CryptBlocks)
//   aes256kw                        a key of `length` bytes (a multiple of
//                                   8) wrapped by aes_kw against
//                                   EVP_aes_256_wrap
//   bcrypt                          bcrypt::verify of a hash of cost
//                                   `length` (no OpenSSL side; Go:
//                                   CompareHashAndPassword, scratch module)
//   totp                            a TOTP code, six digits, HMAC-SHA-1, one
//                                   time step after another (OpenSSL: HMAC()
//                                   and the truncation by hand; Go:
//                                   benchmarks/go/crypto)
//   aes256cbc-decrypt-single        the same one block at a time through
//                                   aes::decrypt_block (sgcl only): what
//                                   7zAES did before
//   random                          `length` random bytes a call into a
//                                   buffer made once: random::fill against
//                                   RAND_bytes (length 32: a key, a nonce)
//
// bench_crypto links libcrypto only when benchmarks/CMakeLists.txt finds
// OpenSSL (Homebrew's openssl@3); without it the openssl side is absent.
// The loop is the hash benchmark's: about two seconds after a quarter of a
// second thrown away, the pointer read through a volatile each call.
#include "benchmarks/common.h"
#include "sgcl/crypto/crypto.h"

#if defined(SGCL_BENCH_OPENSSL)
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>
#include <openssl/thread.h>
#endif

#if defined(SGCL_BENCH_BLAKE3)
#include <blake3.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
    volatile uint64_t sink;

    // calls between two looks at the clock: by the length, or 1 for the
    // password hashes, whose one call takes tens of milliseconds
    size_t batch_calls = 0;

    const unsigned char hmac_key[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

    std::vector<unsigned char> random_bytes(size_t n) {
        std::vector<unsigned char> b(n + 8);
        uint64_t s = 1;
        for (size_t i = 0; i < n; i += 8) {
            s += 0x9e3779b97f4a7c15ull;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
            z ^= z >> 31;
            for (int k = 0; k < 8; ++k) {
                b[i + k] = (unsigned char)(z >> (8 * k));
            }
        }
        return b;
    }

    template<class F>
    std::pair<uint64_t, double> run_for(F&& f, const unsigned char* data, size_t n, double seconds) {
        uint64_t calls = 0;
        uint64_t acc = 0;
        size_t batch = batch_calls != 0 ? batch_calls : n >= 16384 ? 64 : 1024;
        const unsigned char* volatile source = data;
        auto t0 = bench::Clock::now();
        double wall = 0;
        do {
            for (size_t i = 0; i < batch; ++i) {
                acc += f(source, n);
            }
            calls += batch;
            wall = bench::seconds_since(t0);
        } while (wall < seconds);
        sink = acc;
        return {calls, wall};
    }

    sgcl::slice<const sgcl::byte> as_slice(const unsigned char* p, size_t n) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(p), n);
    }

    template<class H>
    auto sgcl_digest() {
        return [](const unsigned char* p, size_t n) {
            auto d = H::of(as_slice(p, n));
            return uint64_t(d[0]) ^ uint64_t(d[H::digest_size - 1]) << 8;
        };
    }

    // The compression function alone over whole blocks, plus a final
    // block, from the initial values: the portable road of each digest
    template<size_t Block, class Word, size_t Words, class F>
    auto portable(const Word (&iv)[Words], F compress) {
        return [&iv, compress](const unsigned char* p, size_t n) {
            Word h[Words];
            std::memcpy(h, iv, sizeof h);
            compress(h, p, n / Block);
            unsigned char last[Block] = {};
            std::memcpy(last, p + n / Block * Block, n % Block);
            compress(h, last, 1);
            return uint64_t(h[0]);
        };
    }

    auto sha3_portable() {
        return [](const unsigned char* p, size_t n) {
            uint64_t a[25] = {};
            sgcl::crypto::detail::keccak_absorb_portable(a, p, n / 136, 136);
            sgcl::crypto::detail::keccak_permute_portable(a);
            return a[0];
        };
    }

#if defined(SGCL_BENCH_OPENSSL)
    auto openssl_digest(const char* name) {
        EVP_MD* md = EVP_MD_fetch(nullptr, name, nullptr);
        return [md](const unsigned char* p, size_t n) {
            unsigned char out[EVP_MAX_MD_SIZE];
            unsigned int len = 0;
            EVP_Digest(p, n, out, &len, md, nullptr);
            return uint64_t(out[0]) ^ uint64_t(out[len - 1]) << 8;
        };
    }
#endif

    // CBC in place, the chain carried on (the data turns to noise and
    // stays noise: the cost is the same); single: decryption a block at a
    // time through aes::decrypt_block, what 7zAES did before
    auto sgcl_cbc(bool encrypt, bool single) {
        auto cbc = std::make_shared<sgcl::crypto::aes_cbc>(as_slice(hmac_key, 32), as_slice(hmac_key, 16));
        auto a = std::make_shared<sgcl::crypto::aes>(as_slice(hmac_key, 32));
        auto iv = std::make_shared<std::vector<unsigned char>>(16, 7);
        return [=](const unsigned char* p, size_t m) {
            unsigned char* q = const_cast<unsigned char*>(p);
            sgcl::slice<sgcl::byte> b(reinterpret_cast<sgcl::byte*>(q), m / 16 * 16);
            if (encrypt) {
                cbc->encrypt_blocks(b, b);
                return uint64_t(q[0]);
            }
            if (!single) {
                cbc->decrypt_blocks(b, b);
                return uint64_t(q[0]);
            }
            sgcl::array<sgcl::byte, 16> in;
            for (size_t i = 0; i + 16 <= m; i += 16) {
                std::memcpy(in.data(), q + i, 16);
                auto out = a->decrypt_block(in);
                for (int j = 0; j < 16; ++j) {
                    q[i + j] = (unsigned char)(uint8_t(out[j]) ^ (*iv)[j]);
                }
                std::memcpy(iv->data(), in.data(), 16);
            }
            return uint64_t(q[0]);
        };
    }

    const unsigned char aead_nonce[12] = {0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33};
    const unsigned char aead_aad[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

    bool is_aead(const std::string& what) {
        return what.starts_with("aes128gcm-") || what.starts_with("aes256gcm-") || what.starts_with("chachapoly-");
    }

    // seal_to, or open_to of what was sealed once, over the n bytes of
    // data; the key is hmac_key's first key_size bytes
    template<class Aead>
    auto sgcl_aead(bool seal, const unsigned char* data, size_t n, size_t key_size) {
        auto aead = std::make_shared<Aead>(as_slice(hmac_key, key_size));
        auto sealed = std::make_shared<std::vector<unsigned char>>(n + 16);
        auto out = std::make_shared<std::vector<unsigned char>>(n + 16);
        auto w = [](std::vector<unsigned char>& v) {
            return sgcl::slice<sgcl::byte>(reinterpret_cast<sgcl::byte*>(v.data()), v.size());
        };
        aead->seal_to(w(*sealed), as_slice(aead_nonce, 12), as_slice(data, n), as_slice(aead_aad, 13));
        return [=](const unsigned char* p, size_t m) {
            if (seal) {
                return uint64_t(aead->seal_to(w(*out), as_slice(aead_nonce, 12), as_slice(p, m), as_slice(aead_aad, 13)));
            }
            auto r = aead->open_to(w(*out), as_slice(aead_nonce, 12), as_slice(sealed->data(), m + 16), as_slice(aead_aad, 13));
            if (!r) {
                std::fprintf(stderr, "sgcl: the tag does not verify\n");
                std::exit(1);
            }
            return uint64_t(*r);
        };
    }

#if defined(SGCL_BENCH_OPENSSL)
    // EVP as TLS stacks use it: one context keyed once, the nonce set per
    // message
    auto openssl_aead(const std::string& what, bool seal, const unsigned char* data, size_t n) {
        const EVP_CIPHER* cipher = what.starts_with("aes128gcm") ? EVP_aes_128_gcm()
                                 : what.starts_with("aes256gcm") ? EVP_aes_256_gcm()
                                 : EVP_chacha20_poly1305();
        auto ctx = std::shared_ptr<EVP_CIPHER_CTX>(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        auto sealed = std::make_shared<std::vector<unsigned char>>(n + 16);
        auto out = std::make_shared<std::vector<unsigned char>>(n + 16);
        auto evp_seal = [cipher](EVP_CIPHER_CTX* c, unsigned char* dst, const unsigned char* src, size_t m) {
            int k = 0, f = 0;
            EVP_CipherInit_ex(c, nullptr, nullptr, nullptr, aead_nonce, 1);
            EVP_CipherUpdate(c, nullptr, &k, aead_aad, 13);
            EVP_CipherUpdate(c, dst, &k, src, int(m));
            EVP_CipherFinal_ex(c, dst + k, &f);
            EVP_CIPHER_CTX_ctrl(c, EVP_CTRL_AEAD_GET_TAG, 16, dst + m);
            return uint64_t(k + f);
        };
        EVP_CipherInit_ex(ctx.get(), cipher, nullptr, hmac_key, nullptr, seal ? 1 : 0);
        if (!seal) {
            EVP_CIPHER_CTX* enc = EVP_CIPHER_CTX_new();
            EVP_CipherInit_ex(enc, cipher, nullptr, hmac_key, nullptr, 1);
            evp_seal(enc, sealed->data(), data, n);
            EVP_CIPHER_CTX_free(enc);
        }
        return [=](const unsigned char* p, size_t m) {
            if (seal) {
                return evp_seal(ctx.get(), out->data(), p, m);
            }
            int k = 0, f = 0;
            EVP_CipherInit_ex(ctx.get(), nullptr, nullptr, nullptr, aead_nonce, 0);
            EVP_CipherUpdate(ctx.get(), nullptr, &k, aead_aad, 13);
            EVP_CipherUpdate(ctx.get(), out->data(), &k, sealed->data(), int(m));
            EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, 16, sealed->data() + m);
            if (EVP_CipherFinal_ex(ctx.get(), out->data() + k, &f) <= 0) {
                std::fprintf(stderr, "openssl: the tag does not verify\n");
                std::exit(1);
            }
            return uint64_t(k + f);
        };
    }
#endif

}

int main(int argc, char** argv) {
    std::string side = argc > 2 ? argv[2] : "";
    if (argc < 3 || (side != "sgcl" && side != "openssl" && side != "reference")) {
        std::fprintf(stderr, "usage: crypto <sha1|sha256|sha512|sha3_256|sha256-portable|sha512-portable|sha3_256-portable|hmac_sha256|random|<aes128gcm|aes256gcm|chachapoly>-<seal|open>> <sgcl|openssl> [length]\n");
        return 2;
    }
    namespace crypto = sgcl::crypto;
    std::string what = argv[1];
    size_t n = argc > 3 ? size_t(std::atoll(argv[3])) : 1024;
    auto data = random_bytes(n);
    if (what.starts_with("argon2") || what.starts_with("scrypt") || what.starts_with("bcrypt")) {
        batch_calls = 1;
    }
    auto measure = [&](auto f) {
        run_for(f, data.data(), n, 0.25);   // thrown away
        auto [calls, wall] = run_for(f, data.data(), n, 2.0);
        double ns = wall * 1e9 / double(calls);
        double mbs = double(n) * double(calls) / wall / 1e6;
        std::printf("crypto %s %s length=%zu ns/op=%.1f MB/s=%.0f wall=%.2fs\n", what.c_str(), side.c_str(), n, ns, mbs, wall);
    };
    bool seal = what.ends_with("-seal");
    if (is_aead(what) && side == "sgcl") {
        if (what.starts_with("aes128gcm")) {
            measure(sgcl_aead<crypto::aes_gcm>(seal, data.data(), n, 16));
        } else if (what.starts_with("aes256gcm")) {
            measure(sgcl_aead<crypto::aes_gcm>(seal, data.data(), n, 32));
        } else {
            measure(sgcl_aead<crypto::chacha20_poly1305>(seal, data.data(), n, 32));
        }
        return 0;
    }
    if (side == "sgcl") {
        if (what == "sha1") {
            measure(sgcl_digest<crypto::sha1>());
        } else if (what == "sha256") {
            measure(sgcl_digest<crypto::sha256>());
        } else if (what == "sha512") {
            measure(sgcl_digest<crypto::sha512>());
        } else if (what == "sha3_256") {
            measure(sgcl_digest<crypto::sha3_256>());
        } else if (what == "sha256-portable") {
            measure(portable<64>(crypto::detail::sha256_iv, crypto::detail::sha256_compress_portable));
        } else if (what == "sha512-portable") {
            measure(portable<128>(crypto::detail::sha512_iv, crypto::detail::sha512_compress_portable));
        } else if (what == "sha3_256-portable") {
            measure(sha3_portable());
        } else if (what == "aes256cbc-decrypt" || what == "aes256cbc-decrypt-single" || what == "aes256cbc-encrypt") {
            measure(sgcl_cbc(what == "aes256cbc-encrypt", what.ends_with("-single")));
        } else if (what == "aes256kw") {
            auto kek = std::make_shared<crypto::aes_kw>(as_slice(hmac_key, 32));
            measure([kek](const unsigned char* p, size_t n) {
                auto w = kek->wrap(as_slice(p, n / 8 * 8));
                return uint64_t(w[0]);
            });
        } else if (what == "bcrypt") {
            sgcl::string h = crypto::bcrypt::generate("correct horse battery staple", int(n)).value();
            measure([h](const unsigned char*, size_t) {
                return uint64_t(crypto::bcrypt::verify("correct horse battery staple", h).has_value());
            });
        } else if (what == "totp") {
            auto step = std::make_shared<uint64_t>(1);
            measure([step](const unsigned char*, size_t) {
                sgcl::string code = crypto::hotp::generate("12345678901234567890", (*step)++);
                return uint64_t(code[0]);
            });
        } else if (what == "blake2b_512") {
            measure(sgcl_digest<crypto::blake2b_512>());
        } else if (what == "blake2s_256") {
            measure(sgcl_digest<crypto::blake2s_256>());
        } else if (what == "blake2b_mac") {
            measure([](const unsigned char* p, size_t n) {
                auto t = crypto::blake2b_256::of(as_slice(p, n), {.key = as_slice(hmac_key, 32)});
                return uint64_t(t[0]);
            });
        } else if (what == "blake2s_mac") {
            measure([](const unsigned char* p, size_t n) {
                auto t = crypto::blake2s_128::of(as_slice(p, n), {.key = as_slice(hmac_key, 32)});
                return uint64_t(t[0]);
            });
        } else if (what == "blake3") {
            measure(sgcl_digest<crypto::blake3>());
        } else if (what == "blake3-xof") {
            measure([](const unsigned char* p, size_t n) {
                crypto::blake3 h;
                h.update("abc");
                unsigned char* q = const_cast<unsigned char*>(p);
                h.value_to(sgcl::slice<sgcl::byte>(reinterpret_cast<sgcl::byte*>(q), n));
                return uint64_t(q[0]);
            });
        } else if (what == "argon2id") {
            measure([](const unsigned char*, size_t n) {
                auto k = crypto::argon2::derive("correct horse battery staple", "0123456789abcdef", 32,
                                                {.memory = uint32_t(n), .iterations = 3, .parallelism = 4});
                return uint64_t(k.size());
            });
        } else if (what == "scrypt") {
            measure([](const unsigned char*, size_t n) {
                auto k = crypto::scrypt::derive("correct horse battery staple", "0123456789abcdef", 32,
                                                {.cost = uint32_t(n), .block_size = 8, .parallelism = 1});
                return uint64_t(k.size());
            });
        } else if (what == "hmac_sha256") {
            measure([](const unsigned char* p, size_t n) {
                auto t = crypto::hmac_sha256::of(as_slice(p, n), as_slice(hmac_key, 32));
                return uint64_t(t[0]);
            });
        } else if (what == "random") {
            measure([](const unsigned char* p, size_t n) {
                unsigned char* q = const_cast<unsigned char*>(p);
                crypto::random::fill(sgcl::slice<sgcl::byte>(reinterpret_cast<sgcl::byte*>(q), n));
                return uint64_t(q[0]);
            });
        } else {
            std::fprintf(stderr, "unknown case %s\n", what.c_str());
            return 2;
        }
        return 0;
    }
    if (side == "reference") {
#if defined(SGCL_BENCH_BLAKE3)
        if (what == "blake3") {
            measure([](const unsigned char* p, size_t n) {
                blake3_hasher h;
                blake3_hasher_init(&h);
                blake3_hasher_update(&h, p, n);
                unsigned char out[32];
                blake3_hasher_finalize(&h, out, 32);
                return uint64_t(out[0]);
            });
            return 0;
        }
        if (what == "blake3-xof") {
            measure([](const unsigned char* p, size_t n) {
                blake3_hasher h;
                blake3_hasher_init(&h);
                blake3_hasher_update(&h, "abc", 3);
                unsigned char* q = const_cast<unsigned char*>(p);
                blake3_hasher_finalize(&h, q, n);
                return uint64_t(q[0]);
            });
            return 0;
        }
#endif
        std::fprintf(stderr, "no reference side for %s\n", what.c_str());
        return 2;
    }
#if defined(SGCL_BENCH_OPENSSL)
    if (is_aead(what)) {
        measure(openssl_aead(what, seal, data.data(), n));
    } else if (what == "sha1") {
        measure(openssl_digest("SHA1"));
    } else if (what == "sha256") {
        measure(openssl_digest("SHA256"));
    } else if (what == "sha512") {
        measure(openssl_digest("SHA512"));
    } else if (what == "sha3_256") {
        measure(openssl_digest("SHA3-256"));
    } else if (what == "blake2b_512") {
        measure(openssl_digest("BLAKE2B-512"));
    } else if (what == "blake2s_256") {
        measure(openssl_digest("BLAKE2S-256"));
    } else if (what == "scrypt") {
        measure([](const unsigned char*, size_t n) {
            unsigned char out[32];
            EVP_PBE_scrypt("correct horse battery staple", 28, reinterpret_cast<const unsigned char*>("0123456789abcdef"), 16,
                           uint64_t(n), 8, 1, 0, out, 32);
            return uint64_t(out[0]);
        });
    } else if (what == "argon2id") {
        OSSL_set_max_threads(nullptr, 4);
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
        measure([kdf](const unsigned char*, size_t n) {
            EVP_KDF_CTX* c = EVP_KDF_CTX_new(kdf);
            uint32_t t = 3, p = 4, threads = 4, m = uint32_t(n);
            OSSL_PARAM params[] = {
                OSSL_PARAM_construct_octet_string("pass", const_cast<char*>("correct horse battery staple"), 28),
                OSSL_PARAM_construct_octet_string("salt", const_cast<char*>("0123456789abcdef"), 16),
                OSSL_PARAM_construct_uint32("iter", &t),
                OSSL_PARAM_construct_uint32("threads", &threads),
                OSSL_PARAM_construct_uint32("lanes", &p),
                OSSL_PARAM_construct_uint32("memcost", &m),
                OSSL_PARAM_construct_end(),
            };
            unsigned char out[32];
            EVP_KDF_derive(c, out, 32, params);
            EVP_KDF_CTX_free(c);
            return uint64_t(out[0]);
        });
    } else if (what == "blake2b_mac" || what == "blake2s_mac") {
        bool b = what == "blake2b_mac";
        EVP_MAC* mac = EVP_MAC_fetch(nullptr, b ? "BLAKE2BMAC" : "BLAKE2SMAC", nullptr);
        auto ctx = std::shared_ptr<EVP_MAC_CTX>(EVP_MAC_CTX_new(mac), EVP_MAC_CTX_free);
        size_t size = b ? 32 : 16;
        OSSL_PARAM params[] = {OSSL_PARAM_construct_size_t("size", &size), OSSL_PARAM_construct_end()};
        EVP_MAC_init(ctx.get(), hmac_key, 32, params);
        measure([ctx, size](const unsigned char* p, size_t n) {
            unsigned char out[32];
            size_t len = 0;
            EVP_MAC_init(ctx.get(), nullptr, 0, nullptr);
            EVP_MAC_update(ctx.get(), p, n);
            EVP_MAC_final(ctx.get(), out, &len, size);
            return uint64_t(out[0]);
        });
    } else if (what == "aes256cbc-decrypt" || what == "aes256cbc-encrypt") {
        bool enc = what == "aes256cbc-encrypt";
        auto ctx = std::shared_ptr<EVP_CIPHER_CTX>(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        unsigned char iv[16] = {7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
        EVP_CipherInit_ex(ctx.get(), EVP_aes_256_cbc(), nullptr, hmac_key, iv, enc ? 1 : 0);
        EVP_CIPHER_CTX_set_padding(ctx.get(), 0);
        measure([ctx](const unsigned char* p, size_t m) {
            unsigned char* q = const_cast<unsigned char*>(p);
            int k = 0;
            EVP_CipherUpdate(ctx.get(), q, &k, q, int(m / 16 * 16));
            return uint64_t(q[0]) + uint64_t(k);
        });
    } else if (what == "aes256kw") {
        auto ctx = std::shared_ptr<EVP_CIPHER_CTX>(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
        auto out = std::make_shared<std::vector<unsigned char>>(n + 16);
        measure([ctx, out](const unsigned char* p, size_t m) {
            int k = 0, f = 0;
            EVP_CipherInit_ex(ctx.get(), EVP_aes_256_wrap(), nullptr, hmac_key, nullptr, 1);
            EVP_CipherUpdate(ctx.get(), out->data(), &k, p, int(m / 8 * 8));
            EVP_CipherFinal_ex(ctx.get(), out->data() + k, &f);
            return uint64_t((*out)[0]) + uint64_t(k);
        });
    } else if (what == "totp") {
        auto step = std::make_shared<uint64_t>(1);
        const EVP_MD* md = EVP_sha1();
        measure([step, md](const unsigned char*, size_t) {
            unsigned char msg[8], out[20];
            uint64_t c = (*step)++;
            for (int i = 0; i < 8; ++i) {
                msg[i] = (unsigned char)(c >> (56 - 8 * i));
            }
            unsigned int len = 0;
            HMAC(md, "12345678901234567890", 20, msg, 8, out, &len);
            unsigned off = out[19] & 15;
            uint32_t bin = (uint32_t(out[off] & 0x7f) << 24 | uint32_t(out[off + 1]) << 16 | uint32_t(out[off + 2]) << 8 | out[off + 3]) % 1000000;
            char code[7];
            std::snprintf(code, sizeof code, "%06u", bin);
            return uint64_t(code[0]);
        });
    } else if (what == "hmac_sha256") {
        const EVP_MD* md = EVP_sha256();
        measure([md](const unsigned char* p, size_t n) {
            unsigned char out[32];
            unsigned int len = 0;
            HMAC(md, hmac_key, 32, p, n, out, &len);
            return uint64_t(out[0]);
        });
    } else if (what == "random") {
        measure([](const unsigned char* p, size_t n) {
            unsigned char* q = const_cast<unsigned char*>(p);
            RAND_bytes(q, int(n));
            return uint64_t(q[0]);
        });
    } else {
        std::fprintf(stderr, "no openssl side for %s\n", what.c_str());
        return 2;
    }
    return 0;
#else
    std::fprintf(stderr, "built without OpenSSL\n");
    return 2;
#endif
}
