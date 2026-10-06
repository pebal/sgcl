//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../crypto/aes.h"
#include "../../crypto/cbc.h"
#include "../../crypto/random.h"
#include "../../crypto/secret.h"
#include "../../crypto/secure_zero.h"
#include "../../crypto/sha256.h"
#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace sgcl::compress::detail {
    // 7zAES (method 06 F1 07 01), 7-Zip's encryption: AES-256 in CBC mode,
    // the key made from the password (UTF-16LE) by SHA-256 over 2^k rounds
    // of the salt, the password and a round counter of 8 bytes little-endian
    // (k = 63: no hashing, the salt and the password themselves, zero-padded
    // to 32 bytes). The properties: a byte of k with two flags (a salt, an
    // IV), then a byte of their sizes less one and the salt and the IV
    // themselves; an IV shorter than 16 bytes is padded with zeros. The data
    // is padded with zeros to whole blocks; the coder's output size in the
    // header says where it ends. Built from crypto::aes (one block at a
    // time) and crypto::sha256; the key held in a crypto::secret<32>, the
    // password's bytes zeroed when they are dropped.
    namespace sevenzip_aes {
#if defined(SGCL_COMPRESS_7ZAES_FUZZ)
        // the fuzzer's: few rounds, so that an input costs microseconds
        constexpr uint32_t MaxRounds = 10;
        constexpr uint32_t WriteRounds = 6;
#else
        constexpr uint32_t MaxRounds = 24;      // k read at most: 2^24 rounds take ~0.4 s; each k more doubles it (a denial of service)
        constexpr uint32_t WriteRounds = 19;    // what 7-Zip writes
#endif
        constexpr uint32_t NoHash = 0x3F;

        struct Props {
            uint32_t k = WriteRounds;
            uint8_t salt[16] = {};
            size_t salt_size = 0;
            uint8_t iv[16] = {};                // zero-padded
        };

        // The coder's properties; false when they are not 7zAES's
        inline bool parse(const std::vector<uint8_t>& p, Props& out) noexcept {
            out = Props();
            if (p.empty()) {
                return false;
            }
            uint8_t b0 = p[0];
            out.k = b0 & 0x3F;
            if ((b0 & 0xC0) == 0) {
                return p.size() == 1;
            }
            if (p.size() < 2) {
                return false;
            }
            uint8_t b1 = p[1];
            size_t salt = ((b0 >> 7) & 1) + (b1 >> 4);
            size_t iv = ((b0 >> 6) & 1) + (b1 & 0x0F);
            if (p.size() != 2 + salt + iv) {
                return false;
            }
            out.salt_size = salt;
            sgcl::detail::copy_bytes(out.salt, p.data() + 2, salt);
            sgcl::detail::copy_bytes(out.iv, p.data() + 2 + salt, iv);
            return true;
        }

        // The properties of a random salt and IV of 16 bytes each at k
        inline std::vector<uint8_t> props(uint32_t k, const uint8_t* salt, const uint8_t* iv) noexcept {
            std::vector<uint8_t> p;
            p.push_back(uint8_t(k | 0x80 | 0x40));
            p.push_back(uint8_t((15 << 4) | 15));
            p.insert(p.end(), salt, salt + 16);
            p.insert(p.end(), iv, iv + 16);
            return p;
        }

        SGCL_INLINE_HOT void random_bytes(uint8_t* p, size_t n) noexcept {
            crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(p), n));
        }

        // A password as the key derivation takes it: UTF-16LE bytes,
        // zeroed when dropped
        class Password {
        public:
            Password() = default;

            explicit Password(std::string_view utf8) noexcept {
                // The room for the whole password at once: a UTF-8 byte is
                // at most two bytes of UTF-16 (four bytes of UTF-8 are a
                // surrogate pair, four), so the vector never grows, and no
                // block holding a part of the password is let go unwiped
                // (_unit wipes one if it ever had to grow)
                _bytes.reserve(2 * utf8.size());
                // UTF-8 to UTF-16LE; bytes that are not UTF-8 go in as they are (as 7-Zip's locale would)
                for (size_t i = 0; i < utf8.size();) {
                    uint8_t c = uint8_t(utf8[i]);
                    uint32_t cp = c;
                    size_t len = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
                    if (len > 1 && i + len <= utf8.size()) {
                        cp = c & (0x7F >> len);
                        for (size_t k = 1; k < len; ++k) {
                            cp = (cp << 6) | (uint8_t(utf8[i + k]) & 0x3F);
                        }
                    } else {
                        len = 1;
                    }
                    if (cp >= 0x10000) {
                        cp -= 0x10000;
                        _unit(0xD800 + (cp >> 10));
                        _unit(0xDC00 + (cp & 0x3FF));
                    } else {
                        _unit(cp);
                    }
                    i += len;
                }
                _set = true;
            }

            Password(const Password&) = delete;
            Password& operator=(const Password&) = delete;

            SGCL_INLINE_HOT Password& operator=(Password&& o) noexcept {
                _wipe();
                _bytes.swap(o._bytes);
                _set = o._set;
                o._set = false;
                return *this;
            }

            SGCL_INLINE_HOT ~Password() {
                _wipe();
            }

            SGCL_INLINE_HOT bool empty() const noexcept {
                return !_set;
            }

            SGCL_INLINE_HOT const std::vector<uint8_t>& bytes() const noexcept {
                return _bytes;
            }

        private:
            void _unit(uint32_t u) noexcept {
                if (_bytes.size() + 2 > _bytes.capacity()) {
                    // a growth the reserve above rules out, done by hand
                    // so that the old block is wiped before it is freed
                    std::vector<uint8_t> wider;
                    wider.reserve(2 * _bytes.capacity() + 2);
                    wider.assign(_bytes.begin(), _bytes.end());
                    _wipe();
                    _bytes.swap(wider);
                }
                _bytes.push_back(uint8_t(u));
                _bytes.push_back(uint8_t(u >> 8));
            }

            SGCL_INLINE_HOT void _wipe() noexcept {
                if (!_bytes.empty()) {
                    crypto::detail::secure_zero(_bytes.data(), _bytes.size());
                }
                _bytes.clear();
            }

            std::vector<uint8_t> _bytes;
            bool _set = false;
        };

        // The key of the password, the salt and k
        inline crypto::secret<32> derive(const Password& password, const Props& p) noexcept {
            auto key = crypto::detail::SecretAccess::make<32>();
            unsigned char* out = crypto::detail::SecretAccess::data(key);
            const auto& pw = password.bytes();
            if (p.k == NoHash) {
                std::memset(out, 0, 32);
                size_t n = std::min<size_t>(p.salt_size, 32);
                sgcl::detail::copy_bytes(out, p.salt, n);
                sgcl::detail::copy_bytes(out + n, pw.data(), std::min(pw.size(), 32 - n));
                return key;
            }
            // one round's bytes in one buffer: the salt, the password, the counter
            std::vector<uint8_t> round(p.salt_size + pw.size() + 8);
            sgcl::detail::copy_bytes(round.data(), p.salt, p.salt_size);
            sgcl::detail::copy_bytes(round.data() + p.salt_size, pw.data(), pw.size());
            uint8_t* counter = round.data() + p.salt_size + pw.size();
            crypto::sha256 h;
            slice<const byte> view(reinterpret_cast<const byte*>(round.data()), round.size());
            for (uint64_t i = 0, n = uint64_t(1) << p.k; i < n; ++i) {
                for (int b = 0; b < 8; ++b) {
                    counter[b] = uint8_t(i >> (8 * b));
                }
                h.update(view);
            }
            auto d = h.value();
            std::memcpy(out, d.data(), 32);
            crypto::detail::secure_zero(&h, sizeof(h));   // its last block holds the password
            crypto::detail::secure_zero(round.data(), round.size());
            crypto::detail::secure_zero(d.data(), d.size());
            return key;
        }

        // The keys an archive's folders ask for, made once for each salt and
        // k (a derivation takes ~12 ms at k = 19); shared by
        // the readers of the archive, from any threads
        class Keys {
        public:
            SGCL_INLINE_HOT explicit Keys(std::string_view password) noexcept
            : _password(password) {
            }

            Keys() = default;

            SGCL_INLINE_HOT bool has_password() const noexcept {
                return !_password.empty();
            }

            crypto::secret<32> get(const Props& p) noexcept {
                std::lock_guard<std::mutex> lock(_mutex);
                for (auto& e : _made) {
                    if (e.k == p.k && e.salt_size == p.salt_size && std::memcmp(e.salt, p.salt, p.salt_size) == 0) {
                        return e.key.clone();
                    }
                }
                _made.push_back(Made{p.k, {}, p.salt_size, derive(_password, p)});
                sgcl::detail::copy_bytes(_made.back().salt, p.salt, p.salt_size);
                return _made.back().key.clone();
            }

        private:
            struct Made {
                uint32_t k;
                uint8_t salt[16];
                size_t salt_size;
                crypto::secret<32> key;
            };

            Password _password;
            std::mutex _mutex;
            std::vector<Made> _made;
        };

        // AES-256-CBC over whole blocks, in place, through crypto::aes_cbc:
        // decryption eight blocks at a time (the blocks are independent),
        // encryption one after another (each block waits for the one
        // before), the chain carried from call to call
        class Cbc {
        public:
            SGCL_INLINE_HOT Cbc(const crypto::secret<32>& key, const uint8_t* iv)
            : _cbc(key.bytes(), slice<const byte>(reinterpret_cast<const byte*>(iv), 16)) {
            }

            Cbc(const Cbc&) = delete;
            Cbc& operator=(const Cbc&) = delete;

            // n rounded down to whole blocks, as before: the callers pass whole blocks
            SGCL_INLINE_HOT void decrypt(uint8_t* p, size_t n) {
                slice<byte> b(reinterpret_cast<byte*>(p), n / 16 * 16);
                _cbc.decrypt_blocks(b, b);
            }

            SGCL_INLINE_HOT void encrypt(uint8_t* p, size_t n) {
                slice<byte> b(reinterpret_cast<byte*>(p), n / 16 * 16);
                _cbc.encrypt_blocks(b, b);
            }

        private:
            crypto::aes_cbc _cbc;
        };
    }
}
