//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/expected.h"
#include "../core/detail/bytes.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/vector.h"
#include "constant_time.h"
#include "detail/chacha_core.h"
#include "detail/keys.h"
#include "detail/poly1305.h"
#include "error.h"
#include "mixin/aead.h"
#include "random.h"

#include <array>
#include <cstddef>
#include <cstdint>

// ChaCha20-Poly1305 (RFC 8439 §2.8), the other AEAD of TLS 1.3 and the one
// of WireGuard: ChaCha20 for secrecy and Poly1305 for integrity, a key of
// 256 bits, a nonce of 96 and a tag of 128. Go's
// golang.org/x/crypto/chacha20poly1305.New. Constant-time on every machine
// without special instructions, and on machines without AES instructions
// the faster of the two AEADs by far.
//
// XChaCha20-Poly1305 (draft-irtf-cfrg-xchacha-03) is the same with a nonce
// of 192 bits: the key and the nonce's first 16 bytes go through HChaCha20
// to a key for this message, the last 8 bytes are the nonce. 24 random
// bytes do not collide in any number of messages a program will seal, so
// that one may draw the nonce at random for each message: seal_random does.
//
// The nonce of chacha20_poly1305 must never repeat under one key (a repeat
// gives away the XOR of the plaintexts and the one-time Poly1305 key of that
// nonce, and with it forgeries): a counter (crypto::nonce_counter), or
// xchacha20_poly1305 with random nonces.
namespace sgcl::crypto {
    namespace detail {
        // Poly1305 over data padded with zeros to a whole block (§2.8: the
        // AAD and the ciphertext each)
        inline void poly_padded(Poly1305& p, const unsigned char* d, size_t n) noexcept {
            const size_t whole = n / 16;
            p.blocks(d, whole, uint64_t(1) << 40);
            if (n > whole * 16) {
                unsigned char last[16] = {};
                sgcl::detail::copy_bytes(last, d + whole * 16, n - whole * 16);
                p.blocks(last, 1, uint64_t(1) << 40);
                secure_zero(last, sizeof last);
            }
        }

        inline void poly_lengths(Poly1305& p, uint64_t aad_size, uint64_t text_size, unsigned char* tag) noexcept {
            unsigned char lengths[16];
            store_le64(lengths, aad_size);
            store_le64(lengths + 8, text_size);
            p.blocks(lengths, 1, uint64_t(1) << 40);
            p.finish(tag);
        }

        // Block 0 (the one-time key, its first 32 bytes, §2.6) and the first
        // blocks of the text's keystream in one batch; the text's keystream
        // bytes made (at most 512), in ks + 64
        inline size_t chacha_poly_start(Poly1305& p, const ChachaState& s, size_t n, unsigned char* ks) noexcept {
            size_t made = chacha_first_blocks(s, n, ks);
            p.init(ks);
            return made - 64;
        }

        // Encryption and the MAC over the ciphertext: the first blocks from
        // the batch that made the key, the rest in pieces of seven steps of
        // nine blocks (4032 bytes), so that what was written is still in the
        // first level of the cache when Poly1305 reads it
        inline void chacha_poly_seal(const ChachaState& s, const unsigned char* in, size_t n, const unsigned char* aad, size_t aad_size, unsigned char* out) noexcept {
            Poly1305 p;
            unsigned char ks[576];
            const size_t first_room = chacha_poly_start(p, s, n, ks);
            poly_padded(p, aad, aad_size);
            const size_t first = n < first_room ? n : first_room;
            for (size_t i = 0; i < first; ++i) {
                out[i] = (unsigned char)(in[i] ^ ks[64 + i]);
            }
            secure_zero(ks, sizeof ks);
            if (first == n) {
                poly_padded(p, out, n);
            } else {
                // first is whole blocks here
                p.blocks(out, first / 16, uint64_t(1) << 40);
                size_t done = first;
                uint32_t counter = uint32_t(1 + first_room / 64);
                constexpr size_t piece = 7 * 576;
                while (n - done >= piece) {
                    chacha_xor(s, counter, in + done, out + done, piece);
                    p.blocks(out + done, piece / 16, uint64_t(1) << 40);
                    counter += piece / 64;
                    done += piece;
                }
                chacha_xor(s, counter, in + done, out + done, n - done);
                poly_padded(p, out + done, n - done);
            }
            poly_lengths(p, aad_size, n, out + n);
        }

        // The tag checked over the ciphertext first; then, and only when it
        // matches, the decryption (the first blocks' keystream kept from the
        // batch that made the key)
        inline bool chacha_poly_open(const ChachaState& s, const unsigned char* in, size_t n, const unsigned char* tag, const unsigned char* aad, size_t aad_size, unsigned char* out) noexcept {
            Poly1305 p;
            unsigned char ks[576];
            const size_t first_room = chacha_poly_start(p, s, n, ks);
            poly_padded(p, aad, aad_size);
            poly_padded(p, in, n);
            unsigned char want[16];
            poly_lengths(p, aad_size, n, want);
            const bool ok = equal_bytes(want, tag, 16);
            secure_zero(want, sizeof want);
            if (!ok) {
                secure_zero(ks, sizeof ks);
                secure_zero(out, n);
                return false;
            }
            const size_t first = n < first_room ? n : first_room;
            for (size_t i = 0; i < first; ++i) {
                out[i] = (unsigned char)(in[i] ^ ks[64 + i]);
            }
            secure_zero(ks, sizeof ks);
            chacha_xor(s, uint32_t(1 + first_room / 64), in + first, out + first, n - first);
            return true;
        }

        // The key of an XChaCha20 message: HChaCha20 of the key and the
        // nonce's first 16 bytes, and the nonce 0^32 || the last 8 bytes
        inline void xchacha_state(ChachaState& s, const std::array<unsigned char, 32>& key, const unsigned char* nonce) noexcept {
            uint32_t sub[8];
            hchacha20(key.data(), nonce, sub);
            for (int i = 0; i < 8; ++i) {
                s.key[i] = sub[i];
            }
            secure_zero(sub, sizeof sub);
            s.nonce[0] = 0;
            s.nonce[1] = load_le32(nonce + 16);
            s.nonce[2] = load_le32(nonce + 20);
        }

        // What the two classes share: the key, 32 bytes in the object's
        // own memory, move-only, zeroed when destroyed and when moved from
        template<class Derived>
        class ChachaKey {
        public:
            static constexpr size_t key_size = 32;
            static constexpr size_t tag_size = 16;
            static constexpr size_t overhead = 16;

            // RFC 8439 §2.8: the counter is 32 bits and block 0 is the
            // Poly1305 key, so (2^32 - 1) blocks of 64 bytes
            static constexpr uint64_t max_plaintext_size = ((uint64_t(1) << 32) - 1) * 64;

            ChachaKey(const ChachaKey&) = delete;
            ChachaKey& operator=(const ChachaKey&) = delete;

        protected:
            std::array<unsigned char, 32> _key = {};
            bool _keyed = false;

            ChachaKey() = default;

            explicit ChachaKey(const slice<const byte>& key) {
                if (key.size() != key_size) {
                    throw invalid_argument(key_size_message(Derived::_name, key.size()));
                }
                std::memcpy(_key.data(), key.data(), key_size);
                _keyed = true;
            }

            ChachaKey(ChachaKey&& other) noexcept
            : _key(other._key), _keyed(other._keyed) {
                other._wipe();
            }

            ChachaKey& operator=(ChachaKey&& other) noexcept {
                if (this != &other) {
                    _key = other._key;
                    _keyed = other._keyed;
                    other._wipe();
                }
                return *this;
            }

            ~ChachaKey() {
                _wipe();
            }

            void _copy(const ChachaKey& other) noexcept {
                _key = other._key;
                _keyed = other._keyed;
            }

            void _check() const {
                if (!_keyed) {
                    moved_from(Derived::_name);
                }
            }

            void _wipe() noexcept {
                secure_zero(_key.data(), _key.size());
                _keyed = false;
            }
        };
    }

    class chacha20_poly1305
    : public detail::ChachaKey<chacha20_poly1305>
    , public mixin::aead<chacha20_poly1305> {
        friend class detail::ChachaKey<chacha20_poly1305>;
        friend class mixin::aead<chacha20_poly1305>;

    public:
        static constexpr size_t nonce_size = 12;

        // The key, 32 bytes; another length is std::invalid_argument. A
        // key read from data goes through from_key instead.
        explicit chacha20_poly1305(const slice<const byte>& key)
        : ChachaKey(key) {
        }

        // The key from data: a wrong length is errc::invalid_key
        static expected<chacha20_poly1305, error> from_key(const slice<const byte>& key) noexcept {
            if (key.size() != key_size) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("chacha20_poly1305", key.size()))));
            }
            return chacha20_poly1305(key);
        }

        chacha20_poly1305(chacha20_poly1305&&) noexcept = default;
        chacha20_poly1305& operator=(chacha20_poly1305&&) noexcept = default;

        chacha20_poly1305 clone() const {
            _check();
            chacha20_poly1305 c;
            c._copy(*this);
            return c;
        }

        // seal, open, seal_to, open_to: mixin/aead.h

    private:
        static constexpr const char* _name = "sgcl::crypto::chacha20_poly1305";

        chacha20_poly1305() = default;

        void _seal(const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* aad, size_t aad_size, unsigned char* out) const noexcept {
            detail::ChachaState s;
            detail::chacha_load(s, _key.data(), nonce);
            detail::chacha_poly_seal(s, in, n, aad, aad_size, out);
            detail::secure_zero_object(s);
        }

        bool _open(const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* tag, const unsigned char* aad, size_t aad_size, unsigned char* out) const noexcept {
            detail::ChachaState s;
            detail::chacha_load(s, _key.data(), nonce);
            bool ok = detail::chacha_poly_open(s, in, n, tag, aad, aad_size, out);
            detail::secure_zero_object(s);
            return ok;
        }
    };

    class xchacha20_poly1305
    : public detail::ChachaKey<xchacha20_poly1305>
    , public mixin::aead<xchacha20_poly1305> {
        friend class detail::ChachaKey<xchacha20_poly1305>;
        friend class mixin::aead<xchacha20_poly1305>;

    public:
        static constexpr size_t nonce_size = 24;

        explicit xchacha20_poly1305(const slice<const byte>& key)
        : ChachaKey(key) {
        }

        static expected<xchacha20_poly1305, error> from_key(const slice<const byte>& key) noexcept {
            if (key.size() != key_size) {
                return unexpected(error(errc::invalid_key, 0, string(detail::key_size_message("xchacha20_poly1305", key.size()))));
            }
            return xchacha20_poly1305(key);
        }

        xchacha20_poly1305(xchacha20_poly1305&&) noexcept = default;
        xchacha20_poly1305& operator=(xchacha20_poly1305&&) noexcept = default;

        xchacha20_poly1305 clone() const {
            _check();
            xchacha20_poly1305 c;
            c._copy(*this);
            return c;
        }

        // seal, open, seal_to, open_to: mixin/aead.h

        // A nonce of 24 bytes drawn from the system's generator, written
        // before the ciphertext: nonce || ciphertext || tag, plaintext.size()
        // + nonce_size + tag_size bytes. The one AEAD of the module where a
        // random nonce is safe for any number of messages.
        vector<byte> seal_random(const slice<const byte>& plaintext) const {
            return seal_random(plaintext, slice<const byte>());
        }

        vector<byte> seal_random(const slice<const byte>& plaintext, const slice<const byte>& aad) const {
            _check();
            if (uint64_t(plaintext.size()) > max_plaintext_size) {
                throw length_error("sgcl::crypto::xchacha20_poly1305::seal_random: a plaintext longer than the cipher can seal under one nonce");
            }
            vector<byte> out(nonce_size + plaintext.size() + tag_size);
            unsigned char* o = detail::bytes(out.data());
            random::fill(out.as_slice().first(nonce_size));
            _seal(o, detail::bytes(plaintext.data()), plaintext.size(), detail::bytes(aad.data()), aad.size(), o + nonce_size);
            return out;
        }

        // What seal_random made, the nonce read from its first 24 bytes:
        // the plaintext, or errc::authentication (data shorter than a
        // nonce and a tag is one too)
        [[nodiscard]] expected<vector<byte>, error> open_random(const slice<const byte>& sealed) const {
            return open_random(sealed, slice<const byte>());
        }

        [[nodiscard]] expected<vector<byte>, error> open_random(const slice<const byte>& sealed, const slice<const byte>& aad) const {
            _check();
            if (sealed.size() < nonce_size + tag_size) {
                return unexpected(error(errc::authentication));
            }
            return open(sealed.subslice(0, nonce_size), sealed.subslice(nonce_size), aad);
        }

    private:
        static constexpr const char* _name = "sgcl::crypto::xchacha20_poly1305";

        xchacha20_poly1305() = default;

        void _seal(const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* aad, size_t aad_size, unsigned char* out) const noexcept {
            detail::ChachaState s;
            detail::xchacha_state(s, _key, nonce);
            detail::chacha_poly_seal(s, in, n, aad, aad_size, out);
            detail::secure_zero_object(s);
        }

        bool _open(const unsigned char* nonce, const unsigned char* in, size_t n, const unsigned char* tag, const unsigned char* aad, size_t aad_size, unsigned char* out) const noexcept {
            detail::ChachaState s;
            detail::xchacha_state(s, _key, nonce);
            bool ok = detail::chacha_poly_open(s, in, n, tag, aad, aad_size, out);
            detail::secure_zero_object(s);
            return ok;
        }
    };
}
