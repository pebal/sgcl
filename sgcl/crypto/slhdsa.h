//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/keys.h"
#include "detail/slhdsa_core.h"
#include "error.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

// SLH-DSA, the stateless hash-based digital signature of FIPS 205, in its
// twelve parameter sets, each a namespace of the same types: SHA-2 or
// SHAKE, security category 1, 3 or 5 (128, 192, 256), small signatures (s,
// slow signing) or fast signing (f, larger signatures):
// slhdsa_sha2_128s ... slhdsa_shake_256f. Its security rests on its hash
// function alone. A key is a value in the object: the public key 2n bytes
// (and the SHA-2 streams of PK.seed, made once), copied freely; the private
// key 4n bytes (SK.seed, SK.prf, PK.seed, PK.root), move-only and zeroed
// when it goes. Signatures are the pure SLH-DSA of §10.2 with an optional
// context string, hedged (n bytes of crypto::random) or deterministic
// (opt_rand = PK.seed); OpenSSL 3.6's keys and signatures are the same bytes.
namespace sgcl::crypto {
    namespace detail::slhdsa {
        struct Access;

        // What a signature is made or checked with besides the message
        struct Options {
            slice<const byte> context;          // at most 255 bytes, the same on both sides; empty by default
            bool deterministic = false;         // opt_rand = PK.seed (§10.2.1) instead of n bytes of crypto::random
        };

        SGCL_INLINE_HOT Message message_of(const slice<const byte>& m, const Options& o) noexcept {
            return Message{{0, uint8_t(o.context.size())}, reinterpret_cast<const uint8_t*>(o.context.data()), o.context.size(),
                           reinterpret_cast<const uint8_t*>(m.data()), m.size()};
        }

        template<class P, class Self>
        class PublicKeyOf {
        public:
            // pk = PK.seed ‖ PK.root, 2n bytes, else errc::invalid_key
            static expected<Self, error> from_bytes(const slice<const byte>& bytes) noexcept {
                if (bytes.size() != P::public_key) {
                    return unexpected<error>(error(errc::invalid_key, string(Self::name) + string(": a public key of the wrong length")));
                }
                Self k;
                k._set(reinterpret_cast<const uint8_t*>(bytes.data()));
                return k;
            }

            vector<byte> bytes() const {
                return vector<byte>(reinterpret_cast<const byte*>(_pk), reinterpret_cast<const byte*>(_pk) + P::public_key);
            }

            // slh_verify (Algorithm 24): false for a signature of another
            // length, a context over 255 bytes, anything that does not verify
            [[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature) const noexcept {
                return verify(message, signature, Options());
            }

            [[nodiscard]] bool verify(const slice<const byte>& message, const slice<const byte>& signature, const Options& o) const noexcept {
                if (signature.size() != P::signature || o.context.size() > 255) {
                    return false;
                }
                return slhdsa::verify<P>(_pk, _hash, message_of(message, o), reinterpret_cast<const uint8_t*>(signature.data()));
            }

            friend bool operator==(const PublicKeyOf& a, const PublicKeyOf& b) noexcept {
                return std::memcmp(a._pk, b._pk, P::public_key) == 0;
            }

        protected:
            PublicKeyOf() noexcept = default;

        private:
            template<class, class, class>
            friend class PrivateKeyOf;
            uint8_t _pk[P::public_key] = {};
            Hash<P> _hash;

            void _set(const uint8_t* pk) noexcept {
                std::memcpy(_pk, pk, P::public_key);
                _hash.init(pk);
            }
        };

        template<class P, class Self, class Public>
        class PrivateKeyOf {
        public:
            // slh_keygen (Algorithm 21): SK.seed, SK.prf, PK.seed of
            // crypto::random, PK.root computed
            static Self generate() noexcept {
                Self k;
                random::fill(slice<byte>(reinterpret_cast<byte*>(k._sk), 3 * P::n));
                k._made();
                return k;
            }

            // The private key's 4n bytes (OpenSSL's raw form); another
            // length, or a PK.root that is not the seeds', is errc::invalid_key
            static expected<Self, error> from_bytes(const slice<const byte>& bytes) noexcept {
                if (bytes.size() != P::private_key) {
                    return unexpected<error>(error(errc::invalid_key, string(Self::name) + string(": a private key of the wrong length")));
                }
                Self k;
                std::memcpy(k._sk, bytes.data(), 3 * P::n);
                k._made();
                if (!constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(k._sk + 3 * P::n), P::n), bytes.subslice(3 * P::n, P::n))) {
                    return unexpected<error>(error(errc::invalid_key, string(Self::name) + string(": a PK.root that is not the seeds'")));
                }
                return k;
            }

            PrivateKeyOf(const PrivateKeyOf&) = delete;
            PrivateKeyOf& operator=(const PrivateKeyOf&) = delete;

            // The object moved from is zeroed and is no key until assigned again
            PrivateKeyOf(PrivateKeyOf&& other) noexcept {
                _take(other);
            }

            PrivateKeyOf& operator=(PrivateKeyOf&& other) noexcept {
                if (this != &other) {
                    _take(other);
                }
                return *this;
            }

            ~PrivateKeyOf() {
                _wipe();
            }

            Self clone() const {
                _check();
                Self k;
                std::memcpy(k._sk, _sk, sizeof _sk);
                k._hash = _hash;
                k._live = true;
                return k;
            }

            // The 4n bytes, which from_bytes takes back, as a secret:
            // move-only, zeroed when it goes
            secret<P::private_key> bytes() const {
                _check();
                auto s = SecretAccess::make<P::private_key>();
                std::memcpy(SecretAccess::data(s), _sk, P::private_key);
                return s;
            }

            Public public_key() const {
                _check();
                Public p;
                p._set(_sk + 2 * P::n);
                return p;
            }

            // slh_sign (Algorithm 22) of message: hedged by default.
            // std::invalid_argument for a context over 255 bytes,
            // std::logic_error for a key moved from
            vector<byte> sign(const slice<const byte>& message) const {
                return sign(message, Options());
            }

            vector<byte> sign(const slice<const byte>& message, const Options& o) const {
                _check();
                if (o.context.size() > 255) {
                    throw std::invalid_argument(std::string(Self::name) + "::private_key::sign: a context over 255 bytes");
                }
                uint8_t rnd[P::n];
                if (o.deterministic) {
                    std::memcpy(rnd, _sk + 2 * P::n, P::n);
                } else {
                    random::fill(slice<byte>(reinterpret_cast<byte*>(rnd), P::n));
                }
                vector<byte> sig(P::signature);
                slhdsa::sign<P>(reinterpret_cast<uint8_t*>(sig.data()), _sk, _hash, message_of(message, o), rnd);
                secure_zero(rnd, sizeof rnd);
                return sig;
            }

            // The same key: the 4n bytes compared in constant time;
            // std::logic_error for a key moved from, as the module's other keys
            friend bool operator==(const PrivateKeyOf& a, const PrivateKeyOf& b) {
                a._check();
                b._check();
                return constant_time::equal(slice<const byte>(reinterpret_cast<const byte*>(a._sk), P::private_key),
                                            slice<const byte>(reinterpret_cast<const byte*>(b._sk), P::private_key));
            }

        protected:
            PrivateKeyOf() noexcept = default;

        private:
            friend struct Access;
            uint8_t _sk[P::private_key] = {};
            Hash<P> _hash;
            bool _live = false;

            void _made() noexcept {
                keygen<P>(_sk);
                _hash.init(_sk + 2 * P::n);
                _live = true;
            }

            void _take(PrivateKeyOf& other) noexcept {
                std::memcpy(_sk, other._sk, sizeof _sk);
                _hash = other._hash;
                _live = other._live;
                other._wipe();
            }

            void _wipe() noexcept {
                secure_zero(_sk, sizeof _sk);
                secure_zero_object(_hash);
                _live = false;
            }

            void _check() const {
                if (!_live) {
                    moved_from(Self::name);
                }
            }
        };

        // The tests' way to the internal algorithms: a key of the three
        // seeds (the vectors' keygen), a signature of M' given whole
        struct Access {
            template<class Self>
            static Self from_seeds(const uint8_t* seeds) {
                Self k;
                std::memcpy(k._sk, seeds, 3 * Self::params::n);
                k._made();
                return k;
            }
        };
    }

    namespace slhdsa_sha2_128s {
        inline constexpr size_t public_key_size = 32;
        inline constexpr size_t private_key_size = 64;
        inline constexpr size_t signature_size = 7856;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Sha2_128s, public_key> {
        public:
            using params = detail::slhdsa::Sha2_128s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_128s::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Sha2_128s, private_key, slhdsa_sha2_128s::public_key> {
        public:
            using params = detail::slhdsa::Sha2_128s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_128s::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_sha2_128s::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_sha2_128f {
        inline constexpr size_t public_key_size = 32;
        inline constexpr size_t private_key_size = 64;
        inline constexpr size_t signature_size = 17088;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Sha2_128f, public_key> {
        public:
            using params = detail::slhdsa::Sha2_128f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_128f::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Sha2_128f, private_key, slhdsa_sha2_128f::public_key> {
        public:
            using params = detail::slhdsa::Sha2_128f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_128f::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_sha2_128f::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_sha2_192s {
        inline constexpr size_t public_key_size = 48;
        inline constexpr size_t private_key_size = 96;
        inline constexpr size_t signature_size = 16224;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Sha2_192s, public_key> {
        public:
            using params = detail::slhdsa::Sha2_192s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_192s::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Sha2_192s, private_key, slhdsa_sha2_192s::public_key> {
        public:
            using params = detail::slhdsa::Sha2_192s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_192s::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_sha2_192s::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_sha2_192f {
        inline constexpr size_t public_key_size = 48;
        inline constexpr size_t private_key_size = 96;
        inline constexpr size_t signature_size = 35664;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Sha2_192f, public_key> {
        public:
            using params = detail::slhdsa::Sha2_192f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_192f::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Sha2_192f, private_key, slhdsa_sha2_192f::public_key> {
        public:
            using params = detail::slhdsa::Sha2_192f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_192f::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_sha2_192f::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_sha2_256s {
        inline constexpr size_t public_key_size = 64;
        inline constexpr size_t private_key_size = 128;
        inline constexpr size_t signature_size = 29792;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Sha2_256s, public_key> {
        public:
            using params = detail::slhdsa::Sha2_256s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_256s::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Sha2_256s, private_key, slhdsa_sha2_256s::public_key> {
        public:
            using params = detail::slhdsa::Sha2_256s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_256s::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_sha2_256s::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_sha2_256f {
        inline constexpr size_t public_key_size = 64;
        inline constexpr size_t private_key_size = 128;
        inline constexpr size_t signature_size = 49856;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Sha2_256f, public_key> {
        public:
            using params = detail::slhdsa::Sha2_256f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_256f::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Sha2_256f, private_key, slhdsa_sha2_256f::public_key> {
        public:
            using params = detail::slhdsa::Sha2_256f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_sha2_256f::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_sha2_256f::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_shake_128s {
        inline constexpr size_t public_key_size = 32;
        inline constexpr size_t private_key_size = 64;
        inline constexpr size_t signature_size = 7856;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Shake_128s, public_key> {
        public:
            using params = detail::slhdsa::Shake_128s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_128s::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Shake_128s, private_key, slhdsa_shake_128s::public_key> {
        public:
            using params = detail::slhdsa::Shake_128s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_128s::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_shake_128s::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_shake_128f {
        inline constexpr size_t public_key_size = 32;
        inline constexpr size_t private_key_size = 64;
        inline constexpr size_t signature_size = 17088;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Shake_128f, public_key> {
        public:
            using params = detail::slhdsa::Shake_128f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_128f::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Shake_128f, private_key, slhdsa_shake_128f::public_key> {
        public:
            using params = detail::slhdsa::Shake_128f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_128f::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_shake_128f::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_shake_192s {
        inline constexpr size_t public_key_size = 48;
        inline constexpr size_t private_key_size = 96;
        inline constexpr size_t signature_size = 16224;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Shake_192s, public_key> {
        public:
            using params = detail::slhdsa::Shake_192s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_192s::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Shake_192s, private_key, slhdsa_shake_192s::public_key> {
        public:
            using params = detail::slhdsa::Shake_192s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_192s::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_shake_192s::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_shake_192f {
        inline constexpr size_t public_key_size = 48;
        inline constexpr size_t private_key_size = 96;
        inline constexpr size_t signature_size = 35664;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Shake_192f, public_key> {
        public:
            using params = detail::slhdsa::Shake_192f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_192f::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Shake_192f, private_key, slhdsa_shake_192f::public_key> {
        public:
            using params = detail::slhdsa::Shake_192f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_192f::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_shake_192f::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_shake_256s {
        inline constexpr size_t public_key_size = 64;
        inline constexpr size_t private_key_size = 128;
        inline constexpr size_t signature_size = 29792;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Shake_256s, public_key> {
        public:
            using params = detail::slhdsa::Shake_256s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_256s::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Shake_256s, private_key, slhdsa_shake_256s::public_key> {
        public:
            using params = detail::slhdsa::Shake_256s;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_256s::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_shake_256s::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }

    namespace slhdsa_shake_256f {
        inline constexpr size_t public_key_size = 64;
        inline constexpr size_t private_key_size = 128;
        inline constexpr size_t signature_size = 49856;

        using options = detail::slhdsa::Options;

        class public_key final : public detail::slhdsa::PublicKeyOf<detail::slhdsa::Shake_256f, public_key> {
        public:
            using params = detail::slhdsa::Shake_256f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_256f::public_key";

        private:
            friend class detail::slhdsa::PublicKeyOf<params, public_key>;
            template<class, class, class>
            friend class detail::slhdsa::PrivateKeyOf;
            public_key() noexcept = default;
        };

        class private_key final : public detail::slhdsa::PrivateKeyOf<detail::slhdsa::Shake_256f, private_key, slhdsa_shake_256f::public_key> {
        public:
            using params = detail::slhdsa::Shake_256f;
            static constexpr const char* name = "sgcl::crypto::slhdsa_shake_256f::private_key";

        private:
            friend class detail::slhdsa::PrivateKeyOf<params, private_key, slhdsa_shake_256f::public_key>;
            friend struct detail::slhdsa::Access;
            private_key() noexcept = default;
        };
    }
}
