//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/keys.h"
#include "detail/mldsa_core.h"
#include "error.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>

// ML-DSA, the module-lattice digital signature of FIPS 204, in its three
// parameter sets, each a namespace of the same types: mldsa44 (category 2),
// mldsa65 (category 3) and mldsa87 (category 5). A private key is kept as
// its 32-byte seed ξ and what key generation makes of it once (s1, s2 and t0
// transformed, Â, K, tr), in plain memory of its own, zeroed when it goes;
// a public key is a handle of one word to a managed state made once of its
// bytes (Â, t1 transformed, tr), so that copies are free and a verification
// only hashes and multiplies. Signatures are the pure ML-DSA of §5.2 with
// an optional context string, hedged (32 bytes of crypto::random each) or
// deterministic. What Go 1.27's crypto/mldsa does; its keys, seeds and
// signatures are the same bytes.
namespace sgcl::crypto {
    namespace detail::mldsa {
        struct Access;

        // A public key's state, in plain memory owned by a managed object of
        // one word (the state outgrows a page: Â of ML-DSA-87 is 56 KB)
        template<class P>
        struct PublicHolder {
            std::unique_ptr<const PublicState<P>> state;
        };

        // What a signature is made or checked with besides the message
        struct Options {
            slice<const byte> context;          // at most 255 bytes, the same on both sides; empty by default
            bool deterministic = false;         // rnd all zeros (§3.4) instead of 32 bytes of crypto::random
        };

        template<class P, class Self>
        class PublicKeyOf {
        public:
            // pkEncode's bytes (§7.2): Sizes<P>::public_key of them, else
            // errc::invalid_key
            static expected<Self, error> from_bytes(const slice<const byte>& bytes) noexcept {
                if (bytes.size() != Sizes<P>::public_key) {
                    return unexpected<error>(error(errc::invalid_key, string(P::name) + string(": a public key of the wrong length")));
                }
                auto state = std::make_unique<PublicState<P>>();
                public_state<P>(*state, reinterpret_cast<const uint8_t*>(bytes.data()));
                auto s = make_tracked<PublicHolder<P>>();
                s->state = std::move(state);
                return Self(std::move(s));
            }

            vector<byte> bytes() const {
                return vector<byte>(reinterpret_cast<const byte*>(_st().pk), reinterpret_cast<const byte*>(_st().pk) + Sizes<P>::public_key);
            }

            // ML-DSA.Verify (Algorithm 3): false for a signature of another
            // length, a context over 255 bytes, hints that do not decode,
            // anything that does not verify
            [[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature) const noexcept {
                return verify(message, signature, Options());
            }

            [[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature, const Options& o) const noexcept {
                if (signature.size() != Sizes<P>::signature || o.context.size() > 255) {
                    return false;
                }
                uint8_t mu[64];
                message_representative(mu, _st().tr, reinterpret_cast<const uint8_t*>(o.context.data()), o.context.size(),
                                       reinterpret_cast<const uint8_t*>(message.data()), message.size());
                return mldsa::verify<P>(_st(), mu, reinterpret_cast<const uint8_t*>(signature.data()));
            }

            friend bool operator==(const PublicKeyOf& a, const PublicKeyOf& b) noexcept {
                return std::memcmp(a._st().pk, b._st().pk, Sizes<P>::public_key) == 0;
            }

        protected:
            explicit PublicKeyOf(tracked_ptr<PublicHolder<P>> s) noexcept
            : _s(std::move(s)) {
            }

        private:
            friend struct Access;
            tracked_ptr<PublicHolder<P>> _s;

            SGCL_INLINE_HOT const PublicState<P>& _st() const noexcept {
                return *_s->state;
            }
        };

        template<class P, class Self, class Public>
        class PrivateKeyOf {
        public:
            // ML-DSA.KeyGen (Algorithm 1): a seed of crypto::random
            static Self generate() noexcept {
                uint8_t xi[32];
                random::fill(slice<byte>(reinterpret_cast<byte*>(xi), sizeof xi));
                Self k(_made(xi));
                secure_zero(xi, sizeof xi);
                return k;
            }

            // ML-DSA.KeyGen_internal (Algorithm 6) of a 32-byte seed ξ (the
            // private key's form of RFC 9881 and Go's NewPrivateKey);
            // another length is errc::invalid_key
            static expected<Self, error> from_seed(const slice<const byte>& seed) noexcept {
                if (seed.size() != 32) {
                    return unexpected<error>(error(errc::invalid_key, string(P::name) + string(": a seed is 32 bytes")));
                }
                return Self(_made(reinterpret_cast<const uint8_t*>(seed.data())));
            }

            PrivateKeyOf(const PrivateKeyOf&) = delete;
            PrivateKeyOf& operator=(const PrivateKeyOf&) = delete;
            PrivateKeyOf(PrivateKeyOf&&) noexcept = default;
            PrivateKeyOf& operator=(PrivateKeyOf&&) noexcept = default;

            // A second key of the same seed
            Self clone() const {
                _check();
                auto s = std::make_unique<PrivateState<P>>(*_s);
                return Self(std::move(s));
            }

            // The seed (Go's Bytes), as a secret: move-only, zeroed when it goes
            secret<32> seed() const {
                _check();
                auto s = SecretAccess::make<32>();
                std::memcpy(SecretAccess::data(s), _s->seed, 32);
                return s;
            }

            Public public_key() const {
                _check();
                return Public::from_bytes(slice<const byte>(reinterpret_cast<const byte*>(_s->pk), Sizes<P>::public_key)).value();
            }

            // ML-DSA.Sign (Algorithm 2) of message: hedged by default.
            // std::invalid_argument for a context over 255 bytes,
            // std::logic_error for a key moved from
            vector<byte> sign(const slice<const byte>& message) const {
                return sign(message, Options());
            }

            vector<byte> sign(const slice<const byte>& message, const Options& o) const {
                _check();
                if (o.context.size() > 255) {
                    throw std::invalid_argument(std::string(P::name) + "::private_key::sign: a context over 255 bytes");
                }
                uint8_t mu[64];
                message_representative(mu, _s->tr, reinterpret_cast<const uint8_t*>(o.context.data()), o.context.size(),
                                       reinterpret_cast<const uint8_t*>(message.data()), message.size());
                uint8_t rnd[32] = {};
                if (!o.deterministic) {
                    random::fill(slice<byte>(reinterpret_cast<byte*>(rnd), sizeof rnd));
                }
                vector<byte> sig(Sizes<P>::signature);
                mldsa::sign<P>(reinterpret_cast<uint8_t*>(sig.data()), *_s, mu, rnd);
                secure_zero(rnd, sizeof rnd);
                secure_zero(mu, sizeof mu);
                return sig;
            }

            // The same key: the seeds (and the public keys) compared in
            // constant time; std::logic_error for a key moved from, as the
            // module's other keys
            friend bool operator==(const PrivateKeyOf& a, const PrivateKeyOf& b) {
                a._check();
                b._check();
                const int seeds = constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(a._s->seed), 32),
                                                       slice<const byte>(reinterpret_cast<const byte*>(b._s->seed), 32));
                const int keys = constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(a._s->pk), Sizes<P>::public_key),
                                                      slice<const byte>(reinterpret_cast<const byte*>(b._s->pk), Sizes<P>::public_key));
                return (seeds & keys) != 0;    // both compared whole, whatever the first gives
            }

        protected:
            explicit PrivateKeyOf(std::unique_ptr<PrivateState<P>> s) noexcept
            : _s(std::move(s)) {
            }

        private:
            friend struct Access;
            std::unique_ptr<PrivateState<P>> _s;

            static std::unique_ptr<PrivateState<P>> _made(const uint8_t xi[32]) {
                auto s = std::make_unique<PrivateState<P>>();
                keygen<P>(*s, xi);
                return s;
            }

            void _check() const {
                if (!_s) {
                    moved_from(P::name);
                }
            }
        };

        // The tests' way to the internal algorithms: a key of skDecode's
        // bytes (the vectors without a seed), a signature of a given μ
        struct Access {
            template<class Self, class P = typename Self::params>
            static optional<Self> from_expanded(const slice<const byte>& sk) {
                if (sk.size() != Sizes<P>::private_key) {
                    return nullopt;
                }
                auto s = std::make_unique<PrivateState<P>>();
                if (!decode_private<P>(*s, reinterpret_cast<const uint8_t*>(sk.data()))) {
                    return nullopt;
                }
                return Self(std::move(s));
            }

            template<class Self, class P = typename Self::params>
            static vector<byte> sign_mu(const Self& k, const uint8_t mu[64], const uint8_t rnd[32]) {
                vector<byte> sig(Sizes<P>::signature);
                mldsa::sign<P>(reinterpret_cast<uint8_t*>(sig.data()), *k._s, mu, rnd);
                return sig;
            }
        };
    }

    namespace mldsa44 {
        inline constexpr size_t public_key_size = 1312;
        inline constexpr size_t signature_size = 2420;
        inline constexpr size_t seed_size = 32;

        using options = detail::mldsa::Options;

        class public_key final : public detail::mldsa::PublicKeyOf<detail::mldsa::Params44, public_key> {
        public:
            using params = detail::mldsa::Params44;

        private:
            friend class detail::mldsa::PublicKeyOf<params, public_key>;
            using PublicKeyOf::PublicKeyOf;
        };

        class private_key final : public detail::mldsa::PrivateKeyOf<detail::mldsa::Params44, private_key, public_key> {
        public:
            using params = detail::mldsa::Params44;

        private:
            friend class detail::mldsa::PrivateKeyOf<params, private_key, mldsa44::public_key>;
            friend struct detail::mldsa::Access;
            using PrivateKeyOf::PrivateKeyOf;
        };
    }

    namespace mldsa65 {
        inline constexpr size_t public_key_size = 1952;
        inline constexpr size_t signature_size = 3309;
        inline constexpr size_t seed_size = 32;

        using options = detail::mldsa::Options;

        class public_key final : public detail::mldsa::PublicKeyOf<detail::mldsa::Params65, public_key> {
        public:
            using params = detail::mldsa::Params65;

        private:
            friend class detail::mldsa::PublicKeyOf<params, public_key>;
            using PublicKeyOf::PublicKeyOf;
        };

        class private_key final : public detail::mldsa::PrivateKeyOf<detail::mldsa::Params65, private_key, public_key> {
        public:
            using params = detail::mldsa::Params65;

        private:
            friend class detail::mldsa::PrivateKeyOf<params, private_key, mldsa65::public_key>;
            friend struct detail::mldsa::Access;
            using PrivateKeyOf::PrivateKeyOf;
        };
    }

    namespace mldsa87 {
        inline constexpr size_t public_key_size = 2592;
        inline constexpr size_t signature_size = 4627;
        inline constexpr size_t seed_size = 32;

        using options = detail::mldsa::Options;

        class public_key final : public detail::mldsa::PublicKeyOf<detail::mldsa::Params87, public_key> {
        public:
            using params = detail::mldsa::Params87;

        private:
            friend class detail::mldsa::PublicKeyOf<params, public_key>;
            using PublicKeyOf::PublicKeyOf;
        };

        class private_key final : public detail::mldsa::PrivateKeyOf<detail::mldsa::Params87, private_key, public_key> {
        public:
            using params = detail::mldsa::Params87;

        private:
            friend class detail::mldsa::PrivateKeyOf<params, private_key, mldsa87::public_key>;
            friend struct detail::mldsa::Access;
            using PrivateKeyOf::PrivateKeyOf;
        };
    }
}
