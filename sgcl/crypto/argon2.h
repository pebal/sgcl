//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/argon2.h"
#include "detail/bytes.h"
#include "detail/password_hash.h"
#include "error.h"
#include "random.h"
#include "secret.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../encoding/base64.h"

#include <cstddef>
#include <cstdint>

// Argon2 (RFC 9106), Go's golang.org/x/crypto/argon2: the password hash of
// the Password Hashing Competition, memory-hard so that each guess of an
// attacker costs memory as well as time. Argon2id is the one to use;
// Argon2i and Argon2d are there for formats that name them.
//
// Two uses: derive, a key from a password and a salt (the raw function, as
// pbkdf2's), and generate/verify, a password stored as a PHC string,
// $argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>, which carries its variant,
// costs and salt, so that a check needs only the password and the string.
namespace sgcl::crypto {
    class argon2 {
    public:
        // y of RFC 9106 §3.2: the variant
        enum class variant : uint8_t {
            d = 0,    // blocks chosen by the data: fastest against trade-offs, open to side channels
            i = 1,    // blocks chosen independently of the data
            id = 2    // the first half of the first pass as Argon2i, the rest as Argon2d: the recommended one
        };

        // The variant and the costs, and the optional inputs of RFC 9106
        // §3.1. The defaults are RFC 9106 §4's second recommended option,
        // for a machine without the 2 GiB of the first: Argon2id, 64 MiB, 3
        // passes, 4 lanes
        struct options {
            argon2::variant variant = argon2::variant::id;
            uint32_t memory = 65536;            // m: KiB of memory, at least 8 per lane
            uint32_t iterations = 3;            // t: passes over the memory, at least 1
            uint32_t parallelism = 4;           // p: lanes, 1 to 2^24 - 1, run side by side on the scheduler's workers
            slice<const byte> secret;           // K: a secret key (a "pepper") kept apart from the stored hashes
            slice<const byte> associated_data;  // X: data bound into the hash
        };

        // n bytes (at least 4) derived from password and salt (at least 8
        // bytes; 16 random ones as a rule), as the options say; out of the
        // RFC's ranges is std::invalid_argument
        static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n = 32) {
            return derive(password, salt, n, options{});
        }

        static secret_bytes derive(const slice<const byte>& password, const slice<const byte>& salt, size_t n, const options& o) {
            _check(salt.size(), n, o);
            secret_bytes out(n);
            _derive(out.as_slice().data(), n, password, salt, o);
            return out;
        }

        // derive() into the caller's buffer: out.size() bytes, which may lie
        // over the inputs (they are hashed whole before the first byte is
        // written)
        static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt) {
            derive_to(out, password, salt, options{});
        }

        static void derive_to(const slice<byte>& out, const slice<const byte>& password, const slice<const byte>& salt,
                              const options& o) {
            _check(salt.size(), out.size(), o);
            _derive(out.data(), out.size(), password, salt, o);
        }

        // A password hash for storage: 16 random bytes of salt and 32 bytes
        // of hash under the options, as a PHC string,
        // $argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>, both in base64
        // without padding. The secret is not in the string (verify takes it
        // again); associated data has no place in it: std::invalid_argument
        static string generate(const slice<const byte>& password) {
            return generate(password, options{});
        }

        static string generate(const slice<const byte>& password, const options& o) {
            if (o.associated_data.size() != 0) {
                throw invalid_argument("sgcl::crypto::argon2: a PHC string has no place for associated data");
            }
            unsigned char salt[16];
            random::fill(slice<byte>(reinterpret_cast<byte*>(salt), sizeof salt));
            _check(sizeof salt, 32, o);
            unsigned char hash[32];
            _derive(reinterpret_cast<byte*>(hash), sizeof hash, password, slice<const byte>(reinterpret_cast<const byte*>(salt), sizeof salt), o);
            string phc = detail::argon2_phc_format(uint32_t(o.variant), o.memory, o.iterations, o.parallelism, salt, sizeof salt, hash, sizeof hash);
            detail::secure_zero(hash, sizeof hash);
            return phc;
        }

        // Whether password is the one the PHC string phc was generated from
        // (any variant, version 19, its costs, salt and hash), the hash
        // computed again and compared in constant time; secret is the one
        // generate had. Success, or errc::authentication for another
        // password, errc::malformed for a string that is not an Argon2 PHC
        // string, errc::unsupported for another version or costs past what a
        // stored string may ask (4 GiB of memory, 2^16 passes)
        [[nodiscard]] static expected<void, error> verify(const slice<const byte>& password, const string& phc,
                                                          const slice<const byte>& secret = {}) noexcept {
            detail::Argon2Phc parsed;
            if (auto e = detail::argon2_phc_parse(slice<const char>(phc.data(), phc.size()), parsed); !e) {
                return unexpected(e.error());
            }
            options o;
            o.variant = argon2::variant(parsed.variant);
            o.memory = parsed.memory;
            o.iterations = parsed.iterations;
            o.parallelism = parsed.parallelism;
            o.secret = secret;
            unsigned char mine[detail::argon2_phc_max_hash];
            _derive(reinterpret_cast<byte*>(mine), parsed.hash_size, password,
                    slice<const byte>(reinterpret_cast<const byte*>(parsed.salt), parsed.salt_size), o);
            bool ok = detail::equal_bytes(mine, parsed.hash, parsed.hash_size);
            detail::secure_zero(mine, sizeof mine);
            detail::secure_zero_object(parsed);
            if (!ok) {
                return unexpected(error(errc::authentication, "sgcl::crypto::argon2: the password does not match"));
            }
            return {};
        }

    private:
        static void _check(size_t salt, size_t n, const options& o) {
            if (o.iterations == 0) {
                throw invalid_argument("sgcl::crypto::argon2: no iterations");
            }
            if (o.parallelism == 0 || o.parallelism > 0xffffffu) {
                throw invalid_argument("sgcl::crypto::argon2: parallelism outside 1 to 2^24 - 1");
            }
            if (uint64_t(o.memory) < 8ull * o.parallelism) {
                throw invalid_argument("sgcl::crypto::argon2: less memory than 8 KiB a lane");
            }
            if (salt < 8 || salt > 0xffffffffu) {
                throw invalid_argument("sgcl::crypto::argon2: a salt shorter than 8 bytes");
            }
            if (n < 4 || n > 0xffffffffu) {
                throw invalid_argument("sgcl::crypto::argon2: an output shorter than 4 bytes");
            }
            if (o.secret.size() > 0xffffffffu || o.associated_data.size() > 0xffffffffu) {
                throw invalid_argument("sgcl::crypto::argon2: a secret or associated data past 2^32 - 1 bytes");
            }
        }

        static void _derive(byte* out, size_t n, const slice<const byte>& password, const slice<const byte>& salt,
                            const options& o) noexcept {
            detail::Argon2Params p{detail::bytes(password.data()), password.size(), detail::bytes(salt.data()), salt.size(),
                                   detail::bytes(o.secret.data()), o.secret.size(), detail::bytes(o.associated_data.data()),
                                   o.associated_data.size(), uint32_t(o.variant), o.memory, o.iterations, o.parallelism};
            detail::Argon2Run run(p);
            run.run(detail::bytes(out), uint32_t(n));
        }
    };
}
