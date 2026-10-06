//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "constant_time.h"
#include "detail/bcrypt_pbkdf.h"
#include "detail/bytes.h"
#include "error.h"
#include "random.h"
#include "secure_zero.h"
#include "../core/aliases.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../encoding/base64.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// bcrypt (Provos and Mazières, "A Future-Adaptable Password Scheme",
// USENIX 1999), Go's golang.org/x/crypto/bcrypt: the password hash of
// OpenBSD and of most web frameworks of the last twenty years, eksblowfish
// keyed 2^cost times by the password and a salt, which then encrypts
// "OrpheanBeholderScryDoubt" 64 times. A hash is OpenBSD's string of 60
// characters, $2b$10$ and 22 characters of salt and 31 of hash in bcrypt's
// own base64; $2a$ and $2y$ are read as $2b$.
//
// bcrypt reads at most 72 bytes of a password (with its terminating NUL,
// 72 of the key): generate refuses a longer one, as Go does, where a cut
// would make two passwords one; verify cuts it, as every implementation
// does, so that the hashes other programs made of long passwords verify.
//
// Blowfish reads its S-boxes at indices made of the key, as every bcrypt
// does: no table-driven Blowfish is constant time. The hashes are compared
// in constant time.
namespace sgcl::crypto {
    namespace detail {
        inline constexpr encoding::base64 bcrypt_base64 =
            encoding::base64("./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", nullopt).lenient();

        // The 23 bytes of a bcrypt hash of the key (the password, its NUL,
        // at most 72 bytes) under a salt of 16 bytes and a cost
        inline void bcrypt_raw(const uint8_t* key, size_t key_size, const uint8_t* salt, uint32_t cost, uint8_t* out) noexcept {
            static constexpr char magic[] = "OrpheanBeholderScryDoubt";
            Blowfish* st = new Blowfish;
            st->init();
            st->expand(salt, 16, key, key_size);
            for (uint64_t i = 0, rounds = uint64_t(1) << cost; i < rounds; ++i) {
                st->expand0(key, key_size);
                st->expand0(salt, 16);
            }
            uint32_t c[6];
            size_t j = 0;
            for (int i = 0; i < 6; ++i) {
                c[i] = Blowfish::stream_word(reinterpret_cast<const uint8_t*>(magic), 24, j);
            }
            for (int i = 0; i < 64; ++i) {
                for (int b = 0; b < 6; b += 2) {
                    st->encipher(c[b], c[b + 1]);
                }
            }
            uint8_t full[24];
            for (int i = 0; i < 6; ++i) {
                store_be32(full + 4 * i, c[i]);
            }
            std::memcpy(out, full, 23);
            secure_zero(full, sizeof full);
            secure_zero(c, sizeof c);
            secure_zero(st, sizeof *st);
            delete st;
        }

        // The password and its NUL, cut at 72 bytes
        struct BcryptKey {
            uint8_t bytes[72];
            size_t size;

            explicit BcryptKey(const slice<const byte>& password) noexcept {
                size_t n = password.size() < 72 ? password.size() : 72;
                if (n != 0) {
                    copy_bytes(bytes, password.data(), n);
                }
                if (n < 72) {
                    bytes[n++] = 0;
                }
                size = n;
            }

            BcryptKey(const BcryptKey&) = delete;
            BcryptKey& operator=(const BcryptKey&) = delete;

            ~BcryptKey() {
                secure_zero(bytes, sizeof bytes);
            }
        };

        struct BcryptHash {
            uint32_t cost;
            uint8_t salt[16];
            uint8_t hash[23];
        };

        // $2a$, $2b$ or $2y$, two digits of cost, '$', 22 + 31 characters of
        // bcrypt's base64: 60 in all
        inline expected<void, error> bcrypt_parse(const slice<const char>& text, BcryptHash& out) noexcept {
            auto malformed = [] {
                return unexpected(error(errc::malformed, "sgcl::crypto::bcrypt: not a bcrypt hash"));
            };
            const char* s = text.data();
            const size_t n = text.size();
            if (n >= 4 && s[0] == '$' && s[1] == '2' && (s[2] == 'x' || s[2] == '$') ) {
                return unexpected(error(errc::unsupported, "sgcl::crypto::bcrypt: a hash of $2$ or $2x$, which are not read"));
            }
            if (n != 60 || s[0] != '$' || s[1] != '2' || (s[2] != 'a' && s[2] != 'b' && s[2] != 'y') || s[3] != '$' || s[6] != '$'
                || s[4] < '0' || s[4] > '9' || s[5] < '0' || s[5] > '9') {
                return malformed();
            }
            out.cost = uint32_t(s[4] - '0') * 10 + uint32_t(s[5] - '0');
            if (out.cost < 4 || out.cost > 31) {
                return malformed();
            }
            uint8_t buf[24];
            auto salt = bcrypt_base64.decode_to(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf), slice<const char>(s + 7, 22));
            if (!salt || *salt != 16) {
                return malformed();
            }
            std::memcpy(out.salt, buf, 16);
            auto hash = bcrypt_base64.decode_to(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf), slice<const char>(s + 29, 31));
            if (!hash || *hash != 23) {
                return malformed();
            }
            std::memcpy(out.hash, buf, 23);
            secure_zero(buf, sizeof buf);
            return {};
        }

        inline string bcrypt_format(uint32_t cost, const uint8_t* salt, const uint8_t* hash) {
            char buf[64] = {'$', '2', 'b', '$', char('0' + cost / 10), char('0' + cost % 10), '$'};
            size_t n = 7;
            n += bcrypt_base64.encode_to(slice<char>(buf + n, sizeof buf - n), slice<const byte>(reinterpret_cast<const byte*>(salt), 16));
            n += bcrypt_base64.encode_to(slice<char>(buf + n, sizeof buf - n), slice<const byte>(reinterpret_cast<const byte*>(hash), 23));
            return string(buf, n);
        }
    }

    class bcrypt {
    public:
        static constexpr int min_cost = 4;
        static constexpr int max_cost = 31;
        static constexpr int default_cost = 10;
        static constexpr size_t max_password_size = 72;

        // A hash of password for storage: 16 random bytes of salt and 2^cost
        // rounds, as "$2b$" cost "$" salt hash, 60 characters. A password
        // longer than 72 bytes is errc::invalid_key (bcrypt would read 72 of
        // it); a cost outside 4 to 31 is std::invalid_argument
        static expected<string, error> generate(const slice<const byte>& password, int cost = default_cost) {
            if (cost < min_cost || cost > max_cost) {
                throw invalid_argument("sgcl::crypto::bcrypt: a cost outside 4 to 31");
            }
            if (password.size() > max_password_size) {
                return unexpected(error(errc::invalid_key, "sgcl::crypto::bcrypt: a password longer than 72 bytes"));
            }
            uint8_t salt[16], hash[23];
            random::fill(slice<byte>(reinterpret_cast<byte*>(salt), sizeof salt));
            detail::BcryptKey key(password);
            detail::bcrypt_raw(key.bytes, key.size, salt, uint32_t(cost), hash);
            string out = detail::bcrypt_format(uint32_t(cost), salt, hash);
            detail::secure_zero(hash, sizeof hash);
            return out;
        }

        // Whether password is the one hash was made from ($2a$, $2b$, $2y$,
        // any cost), compared in constant time; the first 72 bytes of the
        // password count. Success, or errc::authentication for another
        // password, errc::malformed for a string that is not a bcrypt hash,
        // errc::unsupported for $2$ and $2x$
        [[nodiscard]] static expected<void, error> verify(const slice<const byte>& password, const string& hash) noexcept {
            detail::BcryptHash parsed;
            if (auto e = detail::bcrypt_parse(slice<const char>(hash.data(), hash.size()), parsed); !e) {
                return unexpected(e.error());
            }
            detail::BcryptKey key(password);
            uint8_t mine[23];
            detail::bcrypt_raw(key.bytes, key.size, parsed.salt, parsed.cost, mine);
            bool ok = detail::equal_bytes(mine, parsed.hash, 23);
            detail::secure_zero(mine, sizeof mine);
            detail::secure_zero_object(parsed);
            if (!ok) {
                return unexpected(error(errc::authentication, "sgcl::crypto::bcrypt: the password does not match"));
            }
            return {};
        }

        // The cost hash was made with, Go's bcrypt.Cost: what tells a
        // program to make a new hash at the next login when its cost is
        // raised. The errors of verify for a string that is not a hash
        static expected<int, error> cost(const string& hash) noexcept {
            detail::BcryptHash parsed;
            if (auto e = detail::bcrypt_parse(slice<const char>(hash.data(), hash.size()), parsed); !e) {
                return unexpected(e.error());
            }
            return int(parsed.cost);
        }
    };
}
