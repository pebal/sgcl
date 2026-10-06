//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "../../../crypto/mlkem.h"
#include "../../../crypto/p256.h"
#include "../../../crypto/p384.h"
#include "../../../crypto/p521.h"
#include "../../../crypto/random.h"
#include "../../../crypto/x25519.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

// The key shares of TLS 1.3 (RFC 8446 §4.2.8, §7.4) for the groups of v1:
// X25519 (RFC 7748; the share is the 32-byte u-coordinate, §4.2.8.2),
// secp256r1, secp384r1 and secp521r1 (the uncompressed point 04 ‖ X ‖ Y, §4.2.8.2;
// the shared secret is X, §7.4.1) and X25519MLKEM768
// (draft-ietf-tls-ecdhe-mlkem-05 §3–§4: the client's share is the ML-KEM-768
// encapsulation key ‖ its X25519 share, 1184 + 32 bytes; the server's the
// ciphertext ‖ its X25519 share, 1088 + 32; the shared secret the ML-KEM
// key ‖ the X25519 secret, 32 + 32 — ML-KEM first in all three, the
// reverse of the name).
//
// The private keys are drawn from an Entropy (crypto::random by default;
// the tests give the bytes of RFC 8448), in a fixed order per group:
// X25519 32 bytes; P-256 32, P-384 48 and P-521 66, its first byte cut
// to its one bit of the 521 (a scalar out of range draws again); X25519MLKEM768 the 64-byte ML-KEM seed d ‖ z, then 32 for
// X25519; the server of X25519MLKEM768 32 bytes of the encapsulation's
// message m, then 32 for X25519. Every private key and shared secret lives
// in the object that holds it (the connection's unmanaged block of
// secrets), zeroed when it goes; a peer's share that is not a valid one is
// illegal_parameter (§4.2.8, §7.4.2: an X25519 secret of all zeros, a
// point off the curve).
namespace sgcl::net::tls::detail {
    // Where the random bytes of a handshake come from: the private keys,
    // the random fields of the hellos, the session id
    struct Entropy {
        void (*fill)(void* context, uint8_t* out, size_t n) = &Entropy::system_fill;
        void* context = nullptr;

        SGCL_INLINE_HOT void operator()(uint8_t* out, size_t n) const noexcept {
            fill(context, out, n);
        }

        SGCL_INLINE_HOT static void system_fill(void*, uint8_t* out, size_t n) noexcept {
            crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(out), n));
        }
    };

    // A shared secret of a key exchange: up to 66 bytes (P-521's; 64 for
    // X25519MLKEM768), zeroed when it goes and when moved from
    struct SharedSecret {
        uint8_t bytes[66] = {};
        uint8_t size = 0;

        SharedSecret() noexcept = default;
        SharedSecret(const SharedSecret&) = delete;
        SharedSecret& operator=(const SharedSecret&) = delete;

        SGCL_INLINE_HOT SharedSecret(SharedSecret&& other) noexcept {
            *this = std::move(other);
        }

        SGCL_INLINE_HOT SharedSecret& operator=(SharedSecret&& other) noexcept {
            if (this != &other) {
                std::memcpy(bytes, other.bytes, sizeof bytes);
                size = other.size;
                other.wipe();
            }
            return *this;
        }

        SGCL_INLINE_HOT ~SharedSecret() {
            wipe();
        }

        SGCL_INLINE_HOT void wipe() noexcept {
            crypto::detail::secure_zero(bytes, sizeof bytes);
            size = 0;
        }

        SGCL_INLINE_HOT slice<const byte> view() const noexcept {
            return bytes_of(bytes, size);
        }
    };

    SGCL_INLINE_HOT constexpr bool supported(Group g) noexcept {
        return g == Group::x25519 || g == Group::secp256r1 || g == Group::secp384r1 || g == Group::secp521r1 || g == Group::x25519_mlkem768;
    }

    // The sizes of the shares of a group on the wire (0: not a group of v1)
    constexpr size_t client_share_size(Group g) noexcept {
        switch (g) {
        case Group::x25519:
            return 32;
        case Group::secp256r1:
            return 65;
        case Group::secp384r1:
            return 97;
        case Group::secp521r1:
            return 133;
        case Group::x25519_mlkem768:
            return crypto::mlkem768::encapsulation_key_size + 32;
        }
        return 0;
    }

    SGCL_INLINE_HOT constexpr size_t server_share_size(Group g) noexcept {
        return g == Group::x25519_mlkem768 ? crypto::mlkem768::ciphertext_size + 32 : client_share_size(g);
    }

    namespace key_share_detail {
        inline Alert bad_share(const char* what) noexcept {
            return Alert{AlertDescription::illegal_parameter, 0, what};
        }

        SGCL_INLINE_HOT const uint8_t* raw(const slice<const byte>& s) noexcept {
            return reinterpret_cast<const uint8_t*>(s.data());
        }

        SGCL_INLINE_HOT crypto::x25519::private_key x25519_key(const Entropy& entropy) noexcept {
            uint8_t b[32];
            entropy(b, 32);
            auto k = crypto::x25519::private_key::from_bytes(bytes_of(b, 32));
            crypto::detail::secure_zero(b, 32);
            return std::move(*k);   // any 32 bytes are a key
        }

        template<class Key, size_t N>
        Key ec_key(const Entropy& entropy) noexcept {
            uint8_t b[N];
            for (;;) {
                entropy(b, N);
                if constexpr (N == 66) {
                    b[0] &= 1;   // P-521: 521 bits
                }
                auto k = Key::from_bytes(bytes_of(b, N));
                if (k) {
                    crypto::detail::secure_zero(b, N);
                    return std::move(*k);
                }
                // zero or at least n: another draw (for a random scalar
                // about 2^-128 for P-256, 2^-190 for P-384, 2^-260 for P-521)
            }
        }

        // X25519(key, peer) into out; illegal_parameter for a peer's key of
        // small order (the secret of all zeros, §7.4.2)
        inline bool x25519_shared(const crypto::x25519::private_key& key, const uint8_t* peer, uint8_t* out) noexcept {
            auto p = crypto::x25519::public_key::from_bytes(bytes_of(peer, 32));
            if (!p) {
                return false;
            }
            auto s = key.shared_secret(*p);
            if (!s) {
                return false;
            }
            std::memcpy(out, s->bytes().data(), 32);
            return true;
        }

        // The uncompressed point only (§4.2.8.2), on the curve
        template<class Key, class PublicKey>
        bool ec_shared(const Key& key, const slice<const byte>& peer, uint8_t* out) noexcept {
            if (peer.size() != PublicKey::size || raw(peer)[0] != 0x04) {
                return false;
            }
            auto p = PublicKey::from_bytes(peer);
            if (!p) {
                return false;
            }
            auto s = key.shared_secret(*p);
            if (!s) {
                return false;
            }
            std::memcpy(out, s->bytes().data(), s->bytes().size());
            return true;
        }
    }

    // The client's shares: one per group it offers a share for, the
    // private keys kept until the ServerHello names the group (or a
    // HelloRetryRequest asks for another: clear() and add() again)
    class ClientShares {
    public:
        static constexpr size_t MaxShares = 4;

        ClientShares() noexcept = default;
        ClientShares(const ClientShares&) = delete;
        ClientShares& operator=(const ClientShares&) = delete;

        // A new share of the group, its private key from the entropy.
        // A group not of v1, or one already here, is a broken contract
        // (std::logic_error)
        void add(Group g, const Entropy& entropy) {
            if (!supported(g) || find(g) || _count == MaxShares) {
                throw std::logic_error("sgcl::net::tls: a key share of a group not supported, twice or too many");
            }
            Share& s = _shares[_count];
            s.group = g;
            s.public_share.clear();
            switch (g) {
            case Group::x25519: {
                s.x25519.emplace(key_share_detail::x25519_key(entropy));
                auto pk = s.x25519->public_key();
                auto& p = pk.bytes();
                _append(s, p.data(), p.size());
                break;
            }
            case Group::secp256r1: {
                s.p256.emplace(key_share_detail::ec_key<crypto::p256::ecdh_key, 32>(entropy));
                auto p = s.p256->public_key().bytes();
                _append(s, p.data(), p.size());
                break;
            }
            case Group::secp384r1: {
                s.p384.emplace(key_share_detail::ec_key<crypto::p384::ecdh_key, 48>(entropy));
                auto p = s.p384->public_key().bytes();
                _append(s, p.data(), p.size());
                break;
            }
            case Group::secp521r1: {
                s.p521.emplace(key_share_detail::ec_key<crypto::p521::ecdh_key, 66>(entropy));
                auto p = s.p521->public_key().bytes();
                _append(s, p.data(), p.size());
                break;
            }
            case Group::x25519_mlkem768: {
                uint8_t seed[64];
                entropy(seed, 64);
                auto dk = crypto::mlkem768::decapsulation_key::from_seed(bytes_of(seed, 64));
                crypto::detail::secure_zero(seed, 64);
                s.mlkem.emplace(std::move(*dk));
                s.x25519.emplace(key_share_detail::x25519_key(entropy));
                auto ek = s.mlkem->encapsulation_key().bytes();
                _append(s, ek.data(), ek.size());
                auto pk = s.x25519->public_key();
                auto& p = pk.bytes();
                _append(s, p.data(), p.size());
                break;
            }
            }
            ++_count;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _count;
        }

        SGCL_INLINE_HOT Group group(size_t i) const noexcept {
            return _shares[i].group;
        }

        // The share as it goes into the ClientHello's key_share
        SGCL_INLINE_HOT slice<const byte> public_share(size_t i) const noexcept {
            return bytes_of(_shares[i].public_share.data(), _shares[i].public_share.size());
        }

        SGCL_INLINE_HOT bool has(Group g) const noexcept {
            return find(g) != nullptr;
        }

        // The secret shared with the server's share of the group (§7.4);
        // illegal_parameter when the group has no share here (§4.2.8) or
        // the server's share is not a valid one
        [[nodiscard]] expected<SharedSecret, Alert> shared(Group g, const slice<const byte>& server_share) const noexcept {
            using namespace key_share_detail;
            const Share* s = find(g);
            if (!s) {
                return unexpected(bad_share("a key share of a group the client did not offer"));
            }
            if (server_share.size() != server_share_size(g)) {
                return unexpected(bad_share("a key share of the wrong length"));
            }
            SharedSecret out;
            bool ok = false;
            switch (g) {
            case Group::x25519:
                ok = x25519_shared(*s->x25519, raw(server_share), out.bytes);
                out.size = 32;
                break;
            case Group::secp256r1:
                ok = ec_shared<crypto::p256::ecdh_key, crypto::p256::public_key>(*s->p256, server_share, out.bytes);
                out.size = 32;
                break;
            case Group::secp384r1:
                ok = ec_shared<crypto::p384::ecdh_key, crypto::p384::public_key>(*s->p384, server_share, out.bytes);
                out.size = 48;
                break;
            case Group::secp521r1:
                ok = ec_shared<crypto::p521::ecdh_key, crypto::p521::public_key>(*s->p521, server_share, out.bytes);
                out.size = 66;
                break;
            case Group::x25519_mlkem768: {
                const size_t ct = crypto::mlkem768::ciphertext_size;
                auto k = s->mlkem->decapsulate(server_share.subslice(0, ct));
                if (k) {
                    std::memcpy(out.bytes, k->bytes().data(), 32);
                    ok = x25519_shared(*s->x25519, raw(server_share) + ct, out.bytes + 32);
                }
                out.size = 64;
                break;
            }
            }
            if (!ok) {
                return unexpected(bad_share("a key share that is not a valid one"));
            }
            return out;
        }

        // Every private key gone (after the ServerHello, or before the
        // shares of a second ClientHello)
        void clear() noexcept {
            for (size_t i = 0; i < _count; ++i) {
                _shares[i].reset();
            }
            _count = 0;
        }

    private:
        struct Share {
            Group group = Group::x25519;
            optional<crypto::x25519::private_key> x25519;
            optional<crypto::p256::ecdh_key> p256;
            optional<crypto::p384::ecdh_key> p384;
            optional<crypto::p521::ecdh_key> p521;
            optional<crypto::mlkem768::decapsulation_key> mlkem;
            std::vector<uint8_t> public_share;

            SGCL_INLINE_HOT void reset() noexcept {
                x25519.reset();
                p256.reset();
                p384.reset();
                p521.reset();
                mlkem.reset();
                public_share.clear();
            }
        };

        Share _shares[MaxShares];
        size_t _count = 0;

        const Share* find(Group g) const noexcept {
            for (size_t i = 0; i < _count; ++i) {
                if (_shares[i].group == g) {
                    return &_shares[i];
                }
            }
            return nullptr;
        }

        SGCL_INLINE_HOT static void _append(Share& s, const void* p, size_t n) noexcept {
            auto b = static_cast<const uint8_t*>(p);
            s.public_share.insert(s.public_share.end(), b, b + n);
        }
    };

    // The server's side of a group: its share for the ServerHello and the
    // secret, from the client's share (§4.2.8, §7.4); illegal_parameter
    // when the client's share is not a valid one
    struct ServerShare {
        std::vector<uint8_t> public_share;
        SharedSecret secret;
    };

    [[nodiscard]] inline expected<ServerShare, Alert> server_share(Group g, const slice<const byte>& client_share, const Entropy& entropy) {
        using namespace key_share_detail;
        if (!supported(g)) {
            throw std::logic_error("sgcl::net::tls: a server share of a group not supported");
        }
        if (client_share.size() != client_share_size(g)) {
            return unexpected(bad_share("a key share of the wrong length"));
        }
        ServerShare out;
        bool ok = false;
        auto append = [&](const void* p, size_t n) {
            auto b = static_cast<const uint8_t*>(p);
            out.public_share.insert(out.public_share.end(), b, b + n);
        };
        switch (g) {
        case Group::x25519: {
            auto k = x25519_key(entropy);
            ok = x25519_shared(k, raw(client_share), out.secret.bytes);
            out.secret.size = 32;
            append(k.public_key().bytes().data(), 32);
            break;
        }
        case Group::secp256r1: {
            auto k = ec_key<crypto::p256::ecdh_key, 32>(entropy);
            ok = ec_shared<crypto::p256::ecdh_key, crypto::p256::public_key>(k, client_share, out.secret.bytes);
            out.secret.size = 32;
            auto p = k.public_key().bytes();
            append(p.data(), p.size());
            break;
        }
        case Group::secp384r1: {
            auto k = ec_key<crypto::p384::ecdh_key, 48>(entropy);
            ok = ec_shared<crypto::p384::ecdh_key, crypto::p384::public_key>(k, client_share, out.secret.bytes);
            out.secret.size = 48;
            auto p = k.public_key().bytes();
            append(p.data(), p.size());
            break;
        }
        case Group::secp521r1: {
            auto k = ec_key<crypto::p521::ecdh_key, 66>(entropy);
            ok = ec_shared<crypto::p521::ecdh_key, crypto::p521::public_key>(k, client_share, out.secret.bytes);
            out.secret.size = 66;
            auto p = k.public_key().bytes();
            append(p.data(), p.size());
            break;
        }
        case Group::x25519_mlkem768: {
            const size_t ek_size = crypto::mlkem768::encapsulation_key_size;
            auto ek = crypto::mlkem768::encapsulation_key::from_bytes(client_share.subslice(0, ek_size));
            if (!ek) {
                break;   // a coefficient not below q (FIPS 203 §7.2)
            }
            uint8_t m[32];
            entropy(m, 32);
            auto e = crypto::detail::mlkem::Access::encapsulate_with(*ek, m);
            crypto::detail::secure_zero(m, 32);
            std::memcpy(out.secret.bytes, e.shared_key.bytes().data(), 32);
            append(e.ciphertext.data(), e.ciphertext.size());
            auto k = x25519_key(entropy);
            ok = x25519_shared(k, raw(client_share) + ek_size, out.secret.bytes + 32);
            out.secret.size = 64;
            append(k.public_key().bytes().data(), 32);
            break;
        }
        }
        if (!ok) {
            return unexpected(bad_share("a key share that is not a valid one"));
        }
        return out;
    }
}
