//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../error.h"
#include "../../crypto/aes.h"
#include "../../crypto/random.h"
#include "../../crypto/secret.h"
#include "../../crypto/secure_zero.h"
#include "../../crypto/sha256.h"

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
            std::memcpy(out.salt, p.data() + 2, salt);
            std::memcpy(out.iv, p.data() + 2 + salt, iv);
            return true;
        }

        // The properties of a random salt and IV of 16 bytes each at k
        inline std::vector<uint8_t> props(uint32_t k, const uint8_t* salt, const uint8_t* iv) {
            std::vector<uint8_t> p;
            p.push_back(uint8_t(k | 0x80 | 0x40));
            p.push_back(uint8_t((15 << 4) | 15));
            p.insert(p.end(), salt, salt + 16);
            p.insert(p.end(), iv, iv + 16);
            return p;
        }

        inline void random_bytes(uint8_t* p, size_t n) noexcept {
            crypto::random::fill(slice<byte>(reinterpret_cast<byte*>(p), n));
        }

        // A password as the key derivation takes it: UTF-16LE bytes,
        // zeroed when dropped
        class Password {
        public:
            Password() = default;

            explicit Password(std::string_view utf8) {
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

            Password& operator=(Password&& o) noexcept {
                _wipe();
                _bytes.swap(o._bytes);
                _set = o._set;
                o._set = false;
                return *this;
            }

            ~Password() {
                _wipe();
            }

            bool empty() const noexcept {
                return !_set;
            }

            const std::vector<uint8_t>& bytes() const noexcept {
                return _bytes;
            }

        private:
            void _unit(uint32_t u) {
                _bytes.push_back(uint8_t(u));
                _bytes.push_back(uint8_t(u >> 8));
            }

            void _wipe() noexcept {
                if (!_bytes.empty()) {
                    crypto::detail::secure_zero(_bytes.data(), _bytes.size());
                }
                _bytes.clear();
            }

            std::vector<uint8_t> _bytes;
            bool _set = false;
        };

        // The key of the password, the salt and k
        inline crypto::secret<32> derive(const Password& password, const Props& p) {
            auto key = crypto::detail::SecretAccess::make<32>();
            unsigned char* out = crypto::detail::SecretAccess::data(key);
            const auto& pw = password.bytes();
            if (p.k == NoHash) {
                std::memset(out, 0, 32);
                size_t n = std::min<size_t>(p.salt_size, 32);
                std::memcpy(out, p.salt, n);
                std::memcpy(out + n, pw.data(), std::min(pw.size(), 32 - n));
                return key;
            }
            // one round's bytes in one buffer: the salt, the password, the counter
            std::vector<uint8_t> round(p.salt_size + pw.size() + 8);
            std::memcpy(round.data(), p.salt, p.salt_size);
            std::memcpy(round.data() + p.salt_size, pw.data(), pw.size());
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
            explicit Keys(std::string_view password)
            : _password(password) {
            }

            Keys() = default;

            bool has_password() const noexcept {
                return !_password.empty();
            }

            crypto::secret<32> get(const Props& p) {
                std::lock_guard<std::mutex> lock(_mutex);
                for (auto& e : _made) {
                    if (e.k == p.k && e.salt_size == p.salt_size && std::memcmp(e.salt, p.salt, p.salt_size) == 0) {
                        return e.key.clone();
                    }
                }
                _made.push_back(Made{p.k, {}, p.salt_size, derive(_password, p)});
                std::memcpy(_made.back().salt, p.salt, p.salt_size);
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

        // AES-256-CBC over whole blocks, in place: decryption eight blocks
        // at a time (crypto's aes_cbc_decrypt; the blocks are independent),
        // encryption one after another through crypto::aes (each block
        // waits for the one before)
        class Cbc {
        public:
            Cbc(const crypto::secret<32>& key, const uint8_t* iv)
            : _aes(key.bytes()) {
                auto k = key.bytes();
                crypto::detail::aes_setup(_enc, reinterpret_cast<const unsigned char*>(k.data()), 32);
                crypto::detail::aes_setup_decrypt(_dec, _enc);
                std::memcpy(_chain, iv, 16);
            }

            Cbc(const Cbc&) = delete;
            Cbc& operator=(const Cbc&) = delete;

            ~Cbc() {
                crypto::detail::secure_zero(_chain, sizeof(_chain));
                crypto::detail::secure_zero(&_enc, sizeof(_enc));
                crypto::detail::secure_zero(&_dec, sizeof(_dec));
            }

            void decrypt(uint8_t* p, size_t n) {
                crypto::detail::aes_cbc_decrypt(_enc, _dec, _chain, p, p, n / 16);
            }

            void encrypt(uint8_t* p, size_t n) {
                array<byte, 16> in;
                for (size_t i = 0; i + 16 <= n; i += 16) {
                    for (int k = 0; k < 16; ++k) {
                        in[k] = byte(p[i + k] ^ _chain[k]);
                    }
                    auto out = _aes.encrypt_block(in);
                    std::memcpy(p + i, out.data(), 16);
                    std::memcpy(_chain, out.data(), 16);
                }
            }

        private:
            crypto::aes _aes;
            crypto::detail::AesEncryptKey _enc;
            crypto::detail::AesDecryptKey _dec;
            uint8_t _chain[16];
        };
    }
}
