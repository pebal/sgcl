//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/keys.h"
#include "detail/mlkem_core.h"
#include "error.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// ML-KEM (FIPS 203), Go's crypto/mlkem: a key encapsulation mechanism on
// module lattices, believed secure against a quantum computer. The owner
// of a decapsulation key publishes its encapsulation key; anyone
// encapsulates to it, which gives a 32-byte shared key and a ciphertext;
// the ciphertext sent to the owner decapsulates to the same shared key.
// TLS 1.3 pairs ML-KEM-768 with X25519 (X25519MLKEM768, in net). Three
// parameter sets, each a namespace of the same three types: mlkem512
// (category 1), mlkem768 (3, the one to use), mlkem1024 (5).
//
// A decapsulation key is held as its seed d‖z, 64 bytes (FIPS 203 §7.1,
// the form Go keeps), and the expanded key made from it once. It is a
// secret held in the object's own memory — never in managed memory, which
// the collector frees without zeroing — move-only, clone() for a second
// one, zeroed on destruction and in an object moved from. The shared key
// is a secret<32>. An encapsulation key is public: a value, checked when
// read (§7.2: every coefficient below q), which keeps the matrix Â its
// encapsulations need, made once when it is read, as OpenSSL and Go keep
// it: 2, 4.5 or 8 KB, so a key is passed by const& or kept once (a copy
// copies the matrix). The decapsulation key keeps its own. A ciphertext of the right length
// that is not a genuine one is no error: decapsulation gives a
// pseudorandom key for it (the implicit rejection of §6.3), in the same
// time, as Go and OpenSSL do; one of another length is errc::malformed.
//
// What is secret: the seed, the vector s and the errors made from it,
// the message m, everything derived from them, the shared key. The ring's
// arithmetic neither branches nor divides on a value (detail/mlkem_poly.h),
// decapsulation computes both keys and chooses one by a mask on a
// comparison of every byte (detail/mlkem_core.h); what held a secret on
// the way is zeroed before a function returns.
namespace sgcl::crypto {
    namespace detail::mlkem {
        // What encapsulation gives: the shared key, and the ciphertext to
        // send to the owner of the decapsulation key. A type of its own per
        // parameter set, so that one set's result is not taken for another's
        template<class P>
        struct Encapsulation {
            secret<32> shared_key;
            vector<byte> ciphertext;
        };

        // The tag of the constructors only the library calls
        struct Made {};

        SGCL_INLINE_HOT const uint8_t* bytes_of(const slice<const byte>& s) noexcept {
            return reinterpret_cast<const uint8_t*>(s.data());
        }

        template<class P>
        SGCL_INLINE_HOT Encapsulation<P> encapsulation_of(const uint8_t* ek, const Matrix<P>& a, const uint8_t h[32], const uint8_t m[32]) noexcept {
            Encapsulation<P> e{SecretAccess::make<32>(), vector<byte>(Sizes<P>::ciphertext)};
            encaps_with_hash<P>(SecretAccess::data(e.shared_key), reinterpret_cast<uint8_t*>(e.ciphertext.data()), ek, a, h, m);
            return e;
        }

        template<class P, class Self>
        class EncapsulationKeyOf;

        // The library's way in (a key of bytes it has checked) and the
        // tests' (encapsulation with a message given: FIPS 203 §6.2 with m
        // from outside, the Known Answer Tests' form)
        struct Access {
            // An encapsulation key of bytes the library has checked
            template<class Ek, class P>
            SGCL_INLINE_HOT static Ek make(const uint8_t* ek, const Matrix<P>& a) noexcept {
                return Ek(Made{}, ek, a);
            }

            template<class P, class Self>
            SGCL_INLINE_HOT static Encapsulation<P> encapsulate_with(const EncapsulationKeyOf<P, Self>& ek, const uint8_t m[32]) noexcept {
                return encapsulation_of<P>(ek._ek, ek._a, ek._h, m);
            }
        };

        // An encapsulation key of the parameter set P, the public type Self
        template<class P, class Self>
        class EncapsulationKeyOf {
        public:
            // The key of Sizes<P>::ek bytes; another length, or a
            // coefficient of q or more (§7.2), is errc::invalid_key
            SGCL_INLINE_HOT static expected<Self, error> from_bytes(const slice<const byte>& bytes) noexcept {
                if (!encapsulation_key_valid<P>(bytes_of(bytes), bytes.size())) {
                    return unexpected<error>(error(errc::invalid_key, string(P::name) + string(": an encapsulation key of the wrong length or with a coefficient not below q")));
                }
                return Self(Made{}, bytes_of(bytes));
            }

            SGCL_INLINE_HOT vector<byte> bytes() const noexcept {
                vector<byte> v(Sizes<P>::ek);
                std::memcpy(v.data(), _ek, Sizes<P>::ek);
                return v;
            }

            // A shared key and its ciphertext, from 32 bytes of crypto::random
            SGCL_INLINE_HOT Encapsulation<P> encapsulate() const noexcept {
                uint8_t m[32];
                random::fill(slice<byte>(reinterpret_cast<byte*>(m), sizeof m));
                auto e = encapsulation_of<P>(_ek, _a, _h, m);
                secure_zero(m, sizeof m);
                return e;
            }

            SGCL_INLINE_HOT friend bool operator==(const EncapsulationKeyOf& a, const EncapsulationKeyOf& b) noexcept {
                return std::memcmp(a._ek, b._ek, Sizes<P>::ek) == 0;
            }

        private:
            friend struct Access;

            // Only the library makes a key of bytes it has checked (from_bytes,
            // a decapsulation key's own, Access::make): the constructor is
            // private, and the public type's `using` inherits it private
            SGCL_INLINE_HOT EncapsulationKeyOf(Made, const uint8_t* ek) noexcept {
                std::memcpy(_ek, ek, Sizes<P>::ek);
                sha3_256(_h, _ek, Sizes<P>::ek);
                expand_matrix<P>(_a, _ek + 384 * P::k);
            }

            // A decapsulation key's own, whose matrix is made already
            SGCL_INLINE_HOT EncapsulationKeyOf(Made, const uint8_t* ek, const Matrix<P>& a) noexcept
            : _a(a) {
                std::memcpy(_ek, ek, Sizes<P>::ek);
                sha3_256(_h, _ek, Sizes<P>::ek);
            }

            uint8_t _ek[Sizes<P>::ek];
            uint8_t _h[32];                 // H(ek), which every encapsulation hashes
            Matrix<P> _a;                   // Â of ek's ρ, made once, in the constructor: 2, 4.5 or 8 KB
        };

        // A decapsulation key of P, the public type Self, whose
        // encapsulation key is of the type Ek
        template<class P, class Self, class Ek>
        class DecapsulationKeyOf {
        public:
            // A seed from crypto::random
            SGCL_INLINE_HOT static Self generate() noexcept {
                Self k(Made{});
                random::fill(slice<byte>(reinterpret_cast<byte*>(k._seed), sizeof k._seed));
                k._expand();
                return k;
            }

            // The key of its seed d‖z, 64 bytes; another length is invalid_key
            SGCL_INLINE_HOT static expected<Self, error> from_seed(const slice<const byte>& seed) noexcept {
                if (seed.size() != 64) {
                    return unexpected<error>(error(errc::invalid_key, string(P::name) + string(": a seed is 64 bytes")));
                }
                Self k(Made{});
                std::memcpy(k._seed, seed.data(), 64);
                k._expand();
                return k;
            }

            DecapsulationKeyOf(const DecapsulationKeyOf&) = delete;
            DecapsulationKeyOf& operator=(const DecapsulationKeyOf&) = delete;

            // The object moved from is zeroed and is no key of any use
            // until assigned again
            SGCL_INLINE_HOT DecapsulationKeyOf(DecapsulationKeyOf&& other) noexcept {
                _take(other);
            }

            SGCL_INLINE_HOT DecapsulationKeyOf& operator=(DecapsulationKeyOf&& other) noexcept {
                if (this != &other) {
                    _take(other);
                }
                return *this;
            }

            SGCL_INLINE_HOT ~DecapsulationKeyOf() {
                _wipe();
            }

            // A second key of the same seed
            Self clone() const {
                _check();
                Self k(Made{});
                std::memcpy(k._seed, _seed, sizeof _seed);
                std::memcpy(k._dk, _dk, sizeof _dk);
                k._a = _a;
                k._live = 1;
                return k;
            }

            // The seed d‖z, the form the key is kept in, as a secret
            SGCL_INLINE_HOT secret<64> seed() const {
                _check();
                auto s = SecretAccess::make<64>();
                std::memcpy(SecretAccess::data(s), _seed, 64);
                return s;
            }

            SGCL_INLINE_HOT Ek encapsulation_key() const {
                _check();
                return Access::make<Ek>(_dk + Sizes<P>::dk_pke, _a);
            }

            // The shared key of a ciphertext: the encapsulated one, or for a
            // ciphertext that is no genuine one a pseudorandom key (§6.3);
            // a ciphertext of another length is errc::malformed
            SGCL_INLINE_HOT expected<secret<32>, error> decapsulate(const slice<const byte>& ciphertext) const {
                _check();
                if (ciphertext.size() != Sizes<P>::ciphertext) {
                    return unexpected<error>(error(errc::malformed, string(P::name) + string(": a ciphertext of the wrong length")));
                }
                auto key = SecretAccess::make<32>();
                decaps_internal<P>(SecretAccess::data(key), _dk, _a, bytes_of(ciphertext));
                return key;
            }

            // The same key, compared in constant time; a key moved from is
            // std::logic_error, as everywhere
            SGCL_INLINE_HOT friend bool operator==(const DecapsulationKeyOf& a, const DecapsulationKeyOf& b) {
                a._check();
                b._check();
                return equal_bytes(a._seed, b._seed, 64);
            }

        private:
            friend struct Access;

            SGCL_INLINE_HOT explicit DecapsulationKeyOf(Made) noexcept {
            }

            unsigned char _seed[64] = {};
            uint8_t _dk[Sizes<P>::dk] = {};   // the expanded key, dk_PKE‖ek‖H(ek)‖z (§7.1)
            Matrix<P> _a = {};                 // Â of the key's ρ, for the encryption again in decapsulation
            uint8_t _live = 0;                 // 0: moved from (zeroed), no key

            SGCL_INLINE_HOT void _expand() noexcept {
                uint8_t ek[Sizes<P>::ek];
                keygen_internal<P>(ek, _dk, _seed, _seed + 32, _a);
                _live = 1;
            }

            SGCL_INLINE_HOT void _check() const {
                if (!_live) {
                    moved_from(P::name);
                }
            }

            SGCL_INLINE_HOT void _take(DecapsulationKeyOf& other) noexcept {
                std::memcpy(_seed, other._seed, sizeof _seed);
                std::memcpy(_dk, other._dk, sizeof _dk);
                _a = other._a;
                _live = other._live;
                other._wipe();
            }

            // The whole object, the padding after _live included (the
            // matrix's alignment leaves some): nothing of the key left
            SGCL_INLINE_HOT void _wipe() noexcept {
                secure_zero(static_cast<void*>(this), sizeof *this);
            }
        };

    }

    namespace mlkem512 {
        inline constexpr size_t encapsulation_key_size = 800;
        inline constexpr size_t ciphertext_size = 768;
        inline constexpr size_t seed_size = 64;
        inline constexpr size_t shared_key_size = 32;

        using encapsulation = detail::mlkem::Encapsulation<detail::mlkem::Params512>;

        class encapsulation_key final : public detail::mlkem::EncapsulationKeyOf<detail::mlkem::Params512, encapsulation_key> {
        public:
            using EncapsulationKeyOf::EncapsulationKeyOf;
        };

        class decapsulation_key final : public detail::mlkem::DecapsulationKeyOf<detail::mlkem::Params512, decapsulation_key, encapsulation_key> {
        public:
            using DecapsulationKeyOf::DecapsulationKeyOf;
        };
    }

    namespace mlkem768 {
        inline constexpr size_t encapsulation_key_size = 1184;
        inline constexpr size_t ciphertext_size = 1088;
        inline constexpr size_t seed_size = 64;
        inline constexpr size_t shared_key_size = 32;

        using encapsulation = detail::mlkem::Encapsulation<detail::mlkem::Params768>;

        class encapsulation_key final : public detail::mlkem::EncapsulationKeyOf<detail::mlkem::Params768, encapsulation_key> {
        public:
            using EncapsulationKeyOf::EncapsulationKeyOf;
        };

        class decapsulation_key final : public detail::mlkem::DecapsulationKeyOf<detail::mlkem::Params768, decapsulation_key, encapsulation_key> {
        public:
            using DecapsulationKeyOf::DecapsulationKeyOf;
        };
    }

    namespace mlkem1024 {
        inline constexpr size_t encapsulation_key_size = 1568;
        inline constexpr size_t ciphertext_size = 1568;
        inline constexpr size_t seed_size = 64;
        inline constexpr size_t shared_key_size = 32;

        using encapsulation = detail::mlkem::Encapsulation<detail::mlkem::Params1024>;

        class encapsulation_key final : public detail::mlkem::EncapsulationKeyOf<detail::mlkem::Params1024, encapsulation_key> {
        public:
            using EncapsulationKeyOf::EncapsulationKeyOf;
        };

        class decapsulation_key final : public detail::mlkem::DecapsulationKeyOf<detail::mlkem::Params1024, decapsulation_key, encapsulation_key> {
        public:
            using DecapsulationKeyOf::DecapsulationKeyOf;
        };
    }
}
