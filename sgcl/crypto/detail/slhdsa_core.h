//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../hmac.h"
#include "../secure_zero.h"
#include "../sha256.h"
#include "../sha512.h"
#include "bytes.h"
#include "keccak.h"
#include "md.h"
#include "sha256.h"
#include "sha512.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// SLH-DSA's algorithms (FIPS 205 §4–§9) for the twelve parameter sets: the
// addresses (§4.2), WOTS+ (§5), XMSS (§6), the hypertree (§7), FORS (§8)
// and the internal key generation, signing and verification (§9), over
// the hash functions of §11 (SHAKE256; SHA-256 and SHA-512 with MGF1 and
// HMAC). Everything is on the caller's stack or in the key; nothing
// allocates. The SHA-2 sets hash PK.seed padded to a block once per key,
// so that F, H, T and PRF cost the compressions of what follows it.
//
// Constant time: the secrets (SK.seed, SK.prf, the WOTS+ and FORS private
// values derived of them) only go through the hash functions, whose time
// does not depend on the bytes; every index, chain length and branch comes
// from the message's digest, which the signature makes public.
namespace sgcl::crypto::detail::slhdsa {
    // The parameter sets of Table 2: n, h, d, h', a, k, m, and SHA-2 or SHAKE
    template<unsigned N, unsigned H, unsigned D, unsigned HP, unsigned A, unsigned K, unsigned M, bool Sha2>
    struct Params {
        static constexpr unsigned n = N, h = H, d = D, hp = HP, a = A, k = K, m = M;
        static constexpr bool sha2 = Sha2;
        static constexpr unsigned len = 2 * N + 3;                                   // WOTS+ with lg w = 4
        static constexpr size_t public_key = 2 * N;
        static constexpr size_t private_key = 4 * N;
        static constexpr size_t fors_bytes = size_t(K) * (1 + A) * N;
        static constexpr size_t xmss_bytes = size_t(len + HP) * N;
        static constexpr size_t signature = N + fors_bytes + size_t(D) * xmss_bytes;
        static constexpr size_t md_bytes = (size_t(K) * A + 7) / 8;
        static constexpr size_t tree_bytes = (H - H / D + 7) / 8;
        static constexpr size_t leaf_bytes = (H / D + 7) / 8;
    };

    using Sha2_128s = Params<16, 63, 7, 9, 12, 14, 30, true>;
    using Sha2_128f = Params<16, 66, 22, 3, 6, 33, 34, true>;
    using Sha2_192s = Params<24, 63, 7, 9, 14, 17, 39, true>;
    using Sha2_192f = Params<24, 66, 22, 3, 8, 33, 42, true>;
    using Sha2_256s = Params<32, 64, 8, 8, 14, 22, 47, true>;
    using Sha2_256f = Params<32, 68, 17, 4, 9, 35, 49, true>;
    using Shake_128s = Params<16, 63, 7, 9, 12, 14, 30, false>;
    using Shake_128f = Params<16, 66, 22, 3, 6, 33, 34, false>;
    using Shake_192s = Params<24, 63, 7, 9, 14, 17, 39, false>;
    using Shake_192f = Params<24, 66, 22, 3, 8, 33, 42, false>;
    using Shake_256s = Params<32, 64, 8, 8, 14, 22, 47, false>;
    using Shake_256f = Params<32, 68, 17, 4, 9, 35, 49, false>;

    // The address of §4.2: 32 bytes of big-endian words
    enum : uint32_t { WotsHash = 0, WotsPk = 1, Tree = 2, ForsTree = 3, ForsRoots = 4, WotsPrf = 5, ForsPrf = 6 };

    struct Adrs {
        uint8_t b[32] = {};

        SGCL_INLINE_HOT void word(size_t at, uint32_t v) noexcept {
            store_be32(b + at, v);
        }

        SGCL_INLINE_HOT uint32_t word(size_t at) const noexcept {
            return load_be32(b + at);
        }

        SGCL_INLINE_HOT void layer(uint32_t v) noexcept { word(0, v); }
        SGCL_INLINE_HOT void tree(uint64_t v) noexcept {
            word(4, 0);
            store_be64(b + 8, v);
        }
        SGCL_INLINE_HOT void type_and_clear(uint32_t t) noexcept {
            word(16, t);
            std::memset(b + 20, 0, 12);
        }
        SGCL_INLINE_HOT void key_pair(uint32_t v) noexcept { word(20, v); }
        SGCL_INLINE_HOT uint32_t key_pair() const noexcept { return word(20); }
        SGCL_INLINE_HOT void chain(uint32_t v) noexcept { word(24, v); }
        SGCL_INLINE_HOT void height(uint32_t v) noexcept { word(24, v); }
        SGCL_INLINE_HOT void hash(uint32_t v) noexcept { word(28, v); }
        SGCL_INLINE_HOT void index(uint32_t v) noexcept { word(28, v); }
        SGCL_INLINE_HOT uint32_t index() const noexcept { return word(28); }

        // ADRSc of the SHA-2 sets (§11.2): layer, the low 8 bytes of the
        // tree address, type, and the last 12 bytes
        SGCL_INLINE_HOT void compressed(uint8_t out[22]) const noexcept {
            out[0] = b[3];
            std::memcpy(out + 1, b + 8, 8);
            out[9] = b[19];
            std::memcpy(out + 10, b + 20, 12);
        }
    };

    // The hash functions of §11 keyed by PK.seed: F, H and T_l (one
    // function, of any length), PRF; the SHA-2 sets keep the streams of
    // PK.seed padded to a block (SHA-256's for F, PRF and category 1's H
    // and T; SHA-512's for the others' H and T)
    template<class P>
    struct Hash {
        uint8_t pk_seed[P::n];
        MdStream<Sha256Traits> s256;
        MdStream<Sha512Traits> s512;
        uint64_t lanes[25];                 // SHAKE's state with PK.seed in its first lanes (n is a multiple of 8)

        void init(const uint8_t* seed) noexcept {
            std::memcpy(pk_seed, seed, P::n);
            std::memset(lanes, 0, sizeof lanes);
            for (unsigned i = 0; i < P::n / 8; ++i) {
                lanes[i] = load_le64(seed + 8 * i);
            }
            if constexpr (P::sha2) {
                uint8_t block[128] = {};
                std::memcpy(block, seed, P::n);
                s256.init(sha256_iv);
                s256.update(block, 64);
                if constexpr (P::n > 16) {
                    s512.init(sha512_iv);
                    s512.update(block, 128);
                }
            }
        }

        // F, H, T_l: Trunc_n(SHA-2(PK.seed ‖ pad ‖ ADRSc ‖ M)) or
        // SHAKE256(PK.seed ‖ ADRS ‖ M, 8n), M of `size` bytes; F and PRF
        // always the SHA-256 stream (`wide` false), H and T the SHA-512 one
        // in categories 3 and 5
        void tweak(uint8_t* out, const Adrs& adrs, const uint8_t* m, size_t size, bool wide) const noexcept {
            if constexpr (P::sha2) {
                uint8_t c[22];
                adrs.compressed(c);
                if (P::n > 16 && wide) {
                    MdStream<Sha512Traits> s = s512;
                    s.update(c, 22);
                    s.update(m, size);
                    uint8_t digest[64];
                    s.finish(digest);
                    std::memcpy(out, digest, P::n);
                    secure_zero_object(s);
                    secure_zero_object(digest);
                } else {
                    MdStream<Sha256Traits> s = s256;
                    s.update(c, 22);
                    s.update(m, size);
                    uint8_t digest[32];
                    s.finish(digest);
                    std::memcpy(out, digest, P::n);
                    secure_zero_object(s);
                    secure_zero_object(digest);
                }
            } else if (2 * P::n + 32 + size < 136 && size % 8 == 0) {
                // one block, every piece lane-aligned: the lanes XORed in
                // place of the sponge's byte loop
                uint64_t a[25];
                std::memcpy(a, lanes, sizeof a);
                constexpr unsigned at = P::n / 8;
                for (unsigned i = 0; i < 4; ++i) {
                    a[at + i] ^= load_le64(adrs.b + 8 * i);
                }
                for (size_t i = 0; i < size / 8; ++i) {
                    a[at + 4 + i] ^= load_le64(m + 8 * i);
                }
                a[at + 4 + size / 8] ^= 0x1F;
                a[16] ^= uint64_t(0x80) << 56;
                keccak_permute(a);
                for (unsigned i = 0; i < P::n / 8; ++i) {
                    store_le64(out + 8 * i, a[i]);
                }
                secure_zero_object(a);
            } else {
                KeccakSponge<136> s;
                s.init();
                s.absorb(pk_seed, P::n);
                s.absorb(adrs.b, 32);
                s.absorb(m, size);
                s.pad(0x1F);
                s.squeeze(out, P::n);
                secure_zero_object(s);
            }
        }

        SGCL_INLINE_HOT void f(uint8_t* out, const Adrs& adrs, const uint8_t* m) const noexcept {
            tweak(out, adrs, m, P::n, false);
        }

        SGCL_INLINE_HOT void h(uint8_t* out, const Adrs& adrs, const uint8_t* m, size_t size) const noexcept {
            tweak(out, adrs, m, size, true);
        }

        // PRF(PK.seed, SK.seed, ADRS): F's form with SK.seed for M (the
        // stream of SK.seed is not kept: it is zeroed with the sponge)
        void prf(uint8_t* out, const Adrs& adrs, const uint8_t* sk_seed) const noexcept {
            tweak(out, adrs, sk_seed, P::n, false);
        }
    };

    // A message as its pieces: M' = 0 ‖ |ctx| ‖ ctx ‖ M (§10.2)
    struct Message {
        uint8_t prefix[2];
        const uint8_t* context;
        size_t context_size;
        const uint8_t* m;
        size_t m_size;
    };

    // PRF_msg(SK.prf, opt_rand, M) (§11)
    template<class P>
    inline void prf_msg(uint8_t* out, const uint8_t* sk_prf, const uint8_t* opt_rand, const Message& msg) noexcept {
        const auto bytes = [](const uint8_t* p, size_t n) {
            return slice<const byte>(reinterpret_cast<const byte*>(p), n);
        };
        if constexpr (P::sha2) {
            if constexpr (P::n == 16) {
                hmac<sha256> mac(bytes(sk_prf, P::n));
                mac.update(bytes(opt_rand, P::n));
                mac.update(bytes(msg.prefix, 2));
                mac.update(bytes(msg.context, msg.context_size));
                mac.update(bytes(msg.m, msg.m_size));
                auto d = mac.digest();
                std::memcpy(out, d.data(), P::n);
                secure_zero(d.data(), d.size());
            } else {
                hmac<sha512> mac(bytes(sk_prf, P::n));
                mac.update(bytes(opt_rand, P::n));
                mac.update(bytes(msg.prefix, 2));
                mac.update(bytes(msg.context, msg.context_size));
                mac.update(bytes(msg.m, msg.m_size));
                auto d = mac.digest();
                std::memcpy(out, d.data(), P::n);
                secure_zero(d.data(), d.size());
            }
        } else {
            KeccakSponge<136> s;
            s.init();
            s.absorb(sk_prf, P::n);
            s.absorb(opt_rand, P::n);
            s.absorb(msg.prefix, 2);
            s.absorb(msg.context, msg.context_size);
            s.absorb(msg.m, msg.m_size);
            s.pad(0x1F);
            s.squeeze(out, P::n);
            secure_zero_object(s);
        }
    }

    // H_msg(R, PK.seed, PK.root, M) (§11): m bytes; MGF1 over SHA-256
    // (category 1) or SHA-512 of R ‖ PK.seed ‖ SHA-2(R ‖ PK.seed ‖ PK.root ‖ M)
    template<class P>
    inline void h_msg(uint8_t* out, const uint8_t* r, const uint8_t* pk_seed, const uint8_t* pk_root, const Message& msg) noexcept {
        if constexpr (P::sha2) {
            using T = std::conditional_t<P::n == 16, Sha256Traits, Sha512Traits>;
            constexpr size_t hlen = P::n == 16 ? 32 : 64;
            const typename T::word* iv = [] {
                if constexpr (P::n == 16) {
                    return sha256_iv;
                } else {
                    return sha512_iv;
                }
            }();
            MdStream<T> s;
            s.init(iv);
            s.update(r, P::n);
            s.update(pk_seed, P::n);
            s.update(pk_root, P::n);
            s.update(msg.prefix, 2);
            s.update(msg.context, msg.context_size);
            s.update(msg.m, msg.m_size);
            uint8_t inner[hlen];
            s.finish(inner);
            uint8_t block[hlen];
            size_t done = 0;
            for (uint32_t counter = 0; done < P::m; ++counter) {
                MdStream<T> g;
                g.init(iv);
                g.update(r, P::n);
                g.update(pk_seed, P::n);
                g.update(inner, hlen);
                uint8_t c[4];
                store_be32(c, counter);
                g.update(c, 4);
                g.finish(block);
                const size_t take = P::m - done < hlen ? P::m - done : hlen;
                std::memcpy(out + done, block, take);
                done += take;
            }
        } else {
            KeccakSponge<136> s;
            s.init();
            s.absorb(r, P::n);
            s.absorb(pk_seed, P::n);
            s.absorb(pk_root, P::n);
            s.absorb(msg.prefix, 2);
            s.absorb(msg.context, msg.context_size);
            s.absorb(msg.m, msg.m_size);
            s.pad(0x1F);
            s.squeeze(out, P::m);
        }
    }

    // base_2b (Algorithm 4): out_len values of b bits, big-endian bit order
    inline void base_2b(uint32_t* out, const uint8_t* x, unsigned b, size_t out_len) noexcept {
        size_t in = 0;
        unsigned bits = 0;
        uint64_t total = 0;
        for (size_t i = 0; i < out_len; ++i) {
            while (bits < b) {
                total = (total << 8) | x[in++];
                bits += 8;
            }
            bits -= b;
            out[i] = uint32_t(total >> bits) & ((1u << b) - 1);
        }
    }

    // chain (Algorithm 5): s steps of F from step i
    template<class P>
    inline void chain(uint8_t* out, const uint8_t* x, unsigned i, unsigned s, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint8_t tmp[P::n];
        std::memcpy(tmp, x, P::n);
        for (unsigned j = i; j < i + s; ++j) {
            adrs.hash(j);
            hs.f(tmp, adrs, tmp);
        }
        std::memcpy(out, tmp, P::n);
    }

    // The message's base-16 digits and its checksum's (Algorithms 7 and 8)
    template<class P>
    inline void wots_digits(uint32_t* msg, const uint8_t* m) noexcept {
        base_2b(msg, m, 4, 2 * P::n);
        uint32_t csum = 0;
        for (unsigned i = 0; i < 2 * P::n; ++i) {
            csum += 15 - msg[i];
        }
        csum <<= 4;                         // (8 − (len2·lg w mod 8)) mod 8 = 4
        uint8_t c[2] = {uint8_t(csum >> 8), uint8_t(csum)};
        base_2b(msg + 2 * P::n, c, 4, 3);
    }

    // wots_pkGen (Algorithm 6)
    template<class P>
    inline void wots_pk_gen(uint8_t* out, const uint8_t* sk_seed, const Hash<P>& hs, Adrs& adrs) noexcept {
        Adrs sk_adrs = adrs;
        sk_adrs.type_and_clear(WotsPrf);
        sk_adrs.key_pair(adrs.key_pair());
        uint8_t tmp[P::len * P::n];
        uint8_t sk[P::n];
        for (unsigned i = 0; i < P::len; ++i) {
            sk_adrs.chain(i);
            hs.prf(sk, sk_adrs, sk_seed);
            adrs.chain(i);
            chain<P>(tmp + i * P::n, sk, 0, 15, hs, adrs);
        }
        secure_zero_object(sk);
        Adrs pk_adrs = adrs;
        pk_adrs.type_and_clear(WotsPk);
        pk_adrs.key_pair(adrs.key_pair());
        hs.h(out, pk_adrs, tmp, sizeof tmp);
    }

    // wots_sign (Algorithm 7)
    template<class P>
    inline void wots_sign(uint8_t* sig, const uint8_t* m, const uint8_t* sk_seed, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint32_t msg[P::len];
        wots_digits<P>(msg, m);
        Adrs sk_adrs = adrs;
        sk_adrs.type_and_clear(WotsPrf);
        sk_adrs.key_pair(adrs.key_pair());
        uint8_t sk[P::n];
        for (unsigned i = 0; i < P::len; ++i) {
            sk_adrs.chain(i);
            hs.prf(sk, sk_adrs, sk_seed);
            adrs.chain(i);
            chain<P>(sig + i * P::n, sk, 0, msg[i], hs, adrs);
        }
        secure_zero_object(sk);
    }

    // wots_pkFromSig (Algorithm 8)
    template<class P>
    inline void wots_pk_from_sig(uint8_t* out, const uint8_t* sig, const uint8_t* m, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint32_t msg[P::len];
        wots_digits<P>(msg, m);
        uint8_t tmp[P::len * P::n];
        for (unsigned i = 0; i < P::len; ++i) {
            adrs.chain(i);
            chain<P>(tmp + i * P::n, sig + i * P::n, msg[i], 15 - msg[i], hs, adrs);
        }
        Adrs pk_adrs = adrs;
        pk_adrs.type_and_clear(WotsPk);
        pk_adrs.key_pair(adrs.key_pair());
        hs.h(out, pk_adrs, tmp, sizeof tmp);
    }

    // xmss_node (Algorithm 9): the node at height z and index i, its
    // subtree computed bottom up with a stack of h' + 1 nodes (the
    // recursion of the standard, without the recursion)
    template<class P>
    inline void xmss_node(uint8_t* out, const uint8_t* sk_seed, uint32_t i, uint32_t z, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint8_t stack[(P::hp + 1) * P::n];
        uint32_t heights[P::hp + 1];
        unsigned top = 0;
        const uint32_t first = i << z, count = uint32_t(1) << z;
        for (uint32_t leaf = 0; leaf < count; ++leaf) {
            adrs.type_and_clear(WotsHash);
            adrs.key_pair(first + leaf);
            wots_pk_gen<P>(stack + top * P::n, sk_seed, hs, adrs);
            heights[top++] = 0;
            uint32_t index = first + leaf;
            while (top >= 2 && heights[top - 1] == heights[top - 2]) {
                const uint32_t height = heights[top - 1] + 1;
                index >>= 1;
                adrs.type_and_clear(Tree);
                adrs.height(height);
                adrs.index(index);
                hs.h(stack + (top - 2) * P::n, adrs, stack + (top - 2) * P::n, 2 * P::n);
                --top;
                heights[top - 1] = height;
            }
        }
        std::memcpy(out, stack, P::n);
    }

    // xmss_sign (Algorithm 10): the WOTS+ signature of m by leaf idx, and
    // its authentication path
    template<class P>
    inline void xmss_sign(uint8_t* sig, const uint8_t* m, const uint8_t* sk_seed, uint32_t idx, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint8_t* auth = sig + P::len * P::n;
        for (uint32_t j = 0; j < P::hp; ++j) {
            const uint32_t k = (idx >> j) ^ 1;
            xmss_node<P>(auth + j * P::n, sk_seed, k, j, hs, adrs);
        }
        adrs.type_and_clear(WotsHash);
        adrs.key_pair(idx);
        wots_sign<P>(sig, m, sk_seed, hs, adrs);
    }

    // xmss_pkFromSig (Algorithm 11)
    template<class P>
    inline void xmss_pk_from_sig(uint8_t* out, uint32_t idx, const uint8_t* sig, const uint8_t* m, const Hash<P>& hs, Adrs& adrs) noexcept {
        adrs.type_and_clear(WotsHash);
        adrs.key_pair(idx);
        uint8_t node[2 * P::n];
        wots_pk_from_sig<P>(node, sig, m, hs, adrs);
        const uint8_t* auth = sig + P::len * P::n;
        adrs.type_and_clear(Tree);
        adrs.index(idx);
        for (uint32_t k = 0; k < P::hp; ++k) {
            adrs.height(k + 1);
            if (((idx >> k) & 1) == 0) {
                adrs.index(adrs.index() / 2);
                std::memcpy(node + P::n, auth + k * P::n, P::n);
            } else {
                adrs.index((adrs.index() - 1) / 2);
                std::memcpy(node + P::n, node, P::n);
                std::memcpy(node, auth + k * P::n, P::n);
            }
            hs.h(node, adrs, node, 2 * P::n);
        }
        std::memcpy(out, node, P::n);
    }

    // ht_sign (Algorithm 12)
    template<class P>
    inline void ht_sign(uint8_t* sig, const uint8_t* m, const uint8_t* sk_seed, const Hash<P>& hs, uint64_t idx_tree, uint32_t idx_leaf) noexcept {
        Adrs adrs;
        adrs.tree(idx_tree);
        xmss_sign<P>(sig, m, sk_seed, idx_leaf, hs, adrs);
        uint8_t root[P::n];
        xmss_pk_from_sig<P>(root, idx_leaf, sig, m, hs, adrs);
        for (uint32_t j = 1; j < P::d; ++j) {
            idx_leaf = uint32_t(idx_tree & ((uint64_t(1) << P::hp) - 1));
            idx_tree >>= P::hp;
            adrs.layer(j);
            adrs.tree(idx_tree);
            uint8_t* s = sig + j * P::xmss_bytes;
            xmss_sign<P>(s, root, sk_seed, idx_leaf, hs, adrs);
            if (j < P::d - 1) {
                xmss_pk_from_sig<P>(root, idx_leaf, s, root, hs, adrs);
            }
        }
    }

    // ht_verify (Algorithm 13)
    template<class P>
    inline bool ht_verify(const uint8_t* m, const uint8_t* sig, const Hash<P>& hs, uint64_t idx_tree, uint32_t idx_leaf, const uint8_t* pk_root) noexcept {
        Adrs adrs;
        adrs.tree(idx_tree);
        uint8_t node[P::n];
        xmss_pk_from_sig<P>(node, idx_leaf, sig, m, hs, adrs);
        for (uint32_t j = 1; j < P::d; ++j) {
            idx_leaf = uint32_t(idx_tree & ((uint64_t(1) << P::hp) - 1));
            idx_tree >>= P::hp;
            adrs.layer(j);
            adrs.tree(idx_tree);
            xmss_pk_from_sig<P>(node, idx_leaf, sig + j * P::xmss_bytes, node, hs, adrs);
        }
        return std::memcmp(node, pk_root, P::n) == 0;
    }

    // fors_skGen (Algorithm 14)
    template<class P>
    inline void fors_sk_gen(uint8_t* out, const uint8_t* sk_seed, const Hash<P>& hs, const Adrs& adrs, uint32_t idx) noexcept {
        Adrs sk_adrs = adrs;
        sk_adrs.type_and_clear(ForsPrf);
        sk_adrs.key_pair(adrs.key_pair());
        sk_adrs.index(idx);
        hs.prf(out, sk_adrs, sk_seed);
    }

    // fors_node (Algorithm 15), its subtree bottom up as xmss_node's
    template<class P>
    inline void fors_node(uint8_t* out, const uint8_t* sk_seed, uint32_t i, uint32_t z, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint8_t stack[(P::a + 1) * P::n];
        uint32_t heights[P::a + 1];
        unsigned top = 0;
        const uint32_t first = i << z, count = uint32_t(1) << z;
        uint8_t sk[P::n];
        for (uint32_t leaf = 0; leaf < count; ++leaf) {
            uint32_t index = first + leaf;
            fors_sk_gen<P>(sk, sk_seed, hs, adrs, index);
            adrs.height(0);
            adrs.index(index);
            hs.f(stack + top * P::n, adrs, sk);
            heights[top++] = 0;
            while (top >= 2 && heights[top - 1] == heights[top - 2]) {
                const uint32_t height = heights[top - 1] + 1;
                index >>= 1;
                adrs.height(height);
                adrs.index(index);
                hs.h(stack + (top - 2) * P::n, adrs, stack + (top - 2) * P::n, 2 * P::n);
                --top;
                heights[top - 1] = height;
            }
        }
        secure_zero_object(sk);
        std::memcpy(out, stack, P::n);
    }

    // fors_sign (Algorithm 16)
    template<class P>
    inline void fors_sign(uint8_t* sig, const uint8_t* md, const uint8_t* sk_seed, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint32_t indices[P::k];
        base_2b(indices, md, P::a, P::k);
        for (uint32_t i = 0; i < P::k; ++i) {
            uint8_t* s = sig + i * (P::a + 1) * P::n;
            fors_sk_gen<P>(s, sk_seed, hs, adrs, (i << P::a) + indices[i]);
            for (uint32_t j = 0; j < P::a; ++j) {
                const uint32_t sib = (indices[i] >> j) ^ 1;
                fors_node<P>(s + (1 + j) * P::n, sk_seed, (i << (P::a - j)) + sib, j, hs, adrs);
            }
        }
    }

    // fors_pkFromSig (Algorithm 17)
    template<class P>
    inline void fors_pk_from_sig(uint8_t* out, const uint8_t* sig, const uint8_t* md, const Hash<P>& hs, Adrs& adrs) noexcept {
        uint32_t indices[P::k];
        base_2b(indices, md, P::a, P::k);
        uint8_t roots[P::k * P::n];
        for (uint32_t i = 0; i < P::k; ++i) {
            const uint8_t* s = sig + i * (P::a + 1) * P::n;
            uint8_t node[2 * P::n];
            adrs.height(0);
            adrs.index((i << P::a) + indices[i]);
            hs.f(node, adrs, s);
            const uint8_t* auth = s + P::n;
            for (uint32_t j = 0; j < P::a; ++j) {
                adrs.height(j + 1);
                if (((indices[i] >> j) & 1) == 0) {
                    adrs.index(adrs.index() / 2);
                    std::memcpy(node + P::n, auth + j * P::n, P::n);
                } else {
                    adrs.index((adrs.index() - 1) / 2);
                    std::memcpy(node + P::n, node, P::n);
                    std::memcpy(node, auth + j * P::n, P::n);
                }
                hs.h(node, adrs, node, 2 * P::n);
            }
            std::memcpy(roots + i * P::n, node, P::n);
        }
        Adrs pk_adrs = adrs;
        pk_adrs.type_and_clear(ForsRoots);
        pk_adrs.key_pair(adrs.key_pair());
        hs.h(out, pk_adrs, roots, sizeof roots);
    }

    // The digest's split (Algorithms 19 and 20): md, idx_tree, idx_leaf
    template<class P>
    inline void split_digest(const uint8_t* digest, uint64_t& idx_tree, uint32_t& idx_leaf) noexcept {
        uint64_t t = 0;
        for (size_t i = 0; i < P::tree_bytes; ++i) {
            t = (t << 8) | digest[P::md_bytes + i];
        }
        uint32_t l = 0;
        for (size_t i = 0; i < P::leaf_bytes; ++i) {
            l = (l << 8) | digest[P::md_bytes + P::tree_bytes + i];
        }
        constexpr unsigned tree_bits = P::h - P::h / P::d;
        idx_tree = tree_bits >= 64 ? t : t & ((uint64_t(1) << (tree_bits % 64)) - 1);
        idx_leaf = l & ((uint32_t(1) << (P::h / P::d)) - 1);
    }

    // slh_keygen_internal (Algorithm 18): PK.root of the seeds; sk is
    // SK.seed ‖ SK.prf ‖ PK.seed ‖ PK.root, its first 3n bytes given
    template<class P>
    inline void keygen(uint8_t* sk) noexcept {
        Hash<P> hs;
        hs.init(sk + 2 * P::n);
        Adrs adrs;
        adrs.layer(P::d - 1);
        xmss_node<P>(sk + 3 * P::n, sk, 0, P::hp, hs, adrs);
    }

    // slh_sign_internal (Algorithm 19); opt_rand is PK.seed for a
    // deterministic signature
    template<class P>
    inline void sign(uint8_t* sig, const uint8_t* sk, const Hash<P>& hs, const Message& msg, const uint8_t* opt_rand) noexcept {
        const uint8_t* sk_seed = sk;
        const uint8_t* sk_prf = sk + P::n;
        const uint8_t* pk_seed = sk + 2 * P::n;
        const uint8_t* pk_root = sk + 3 * P::n;
        prf_msg<P>(sig, sk_prf, opt_rand, msg);
        uint8_t digest[P::m];
        h_msg<P>(digest, sig, pk_seed, pk_root, msg);
        uint64_t idx_tree;
        uint32_t idx_leaf;
        split_digest<P>(digest, idx_tree, idx_leaf);
        Adrs adrs;
        adrs.tree(idx_tree);
        adrs.type_and_clear(ForsTree);
        adrs.key_pair(idx_leaf);
        uint8_t* fors = sig + P::n;
        fors_sign<P>(fors, digest, sk_seed, hs, adrs);
        uint8_t pk_fors[P::n];
        fors_pk_from_sig<P>(pk_fors, fors, digest, hs, adrs);
        ht_sign<P>(fors + P::fors_bytes, pk_fors, sk_seed, hs, idx_tree, idx_leaf);
    }

    // slh_verify_internal (Algorithm 20), of a signature of the right length
    template<class P>
    inline bool verify(const uint8_t* pk, const Hash<P>& hs, const Message& msg, const uint8_t* sig) noexcept {
        uint8_t digest[P::m];
        h_msg<P>(digest, sig, pk, pk + P::n, msg);
        uint64_t idx_tree;
        uint32_t idx_leaf;
        split_digest<P>(digest, idx_tree, idx_leaf);
        Adrs adrs;
        adrs.tree(idx_tree);
        adrs.type_and_clear(ForsTree);
        adrs.key_pair(idx_leaf);
        uint8_t pk_fors[P::n];
        fors_pk_from_sig<P>(pk_fors, sig + P::n, digest, hs, adrs);
        return ht_verify<P>(pk_fors, sig + P::n + P::fors_bytes, hs, idx_tree, idx_leaf, pk + P::n);
    }
}
