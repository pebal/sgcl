//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "wire.h"
#include "../../../crypto/constant_time.h"
#include "../../../crypto/ctr.h"
#include "../../../crypto/gcm.h"
#include "../../../crypto/hmac.h"
#include "../../../crypto/random.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/sha512.h"
#include "../../../crypto/detail/chacha_core.h"
#include "../../../crypto/detail/poly1305.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

// The binary packet of SSH (RFC 4253 §6) under the ciphers of this module,
// one direction's keys at a time: a packet sealed from a payload, and a
// packet opened in two steps, its length first (so that the reader knows
// how much more to read) and then the rest, checked before a byte of it is
// decrypted (but for the MACs that are not encrypt-then-MAC, which RFC
// 4253 defines over the plaintext).
//
//   chacha20-poly1305@openssh.com  OpenSSH's PROTOCOL.chacha20poly1305: the
//                                  key's second half encrypts the length
//                                  (ChaCha20, block 0, the sequence number
//                                  as the 64-bit nonce), its first half the
//                                  rest from block 1, block 0's first 32
//                                  bytes the Poly1305 key, the tag over the
//                                  length and the rest as sent
//   aes128-gcm@openssh.com,        RFC 5647 as OpenSSH does it: the length in
//   aes256-gcm@openssh.com         the clear as the additional data, a nonce
//                                  of a fixed 4 bytes and a counter of 8
//                                  incremented after each packet
//   aes128-ctr, aes192-ctr,        RFC 4344 with a MAC: hmac-sha2-256 and
//   aes256-ctr                     hmac-sha2-512 over the sequence number and
//                                  the packet in the clear (RFC 6668), or
//                                  their -etm@openssh.com forms over the
//                                  sequence number, the length in the clear
//                                  and the ciphertext
//   none                           the packets of the first key exchange
//
// Every key lives in unmanaged memory (this object is held by a
// unique_ptr), zeroed by the destructors of crypto's types and of the
// ChaCha states here.
namespace sgcl::net::ssh::detail {
    enum class CipherKind : uint8_t {
        none,
        chacha20_poly1305,
        aes128_gcm,
        aes256_gcm,
        aes128_ctr,
        aes192_ctr,
        aes256_ctr,
    };

    enum class MacKind : uint8_t {
        none,
        hmac_sha2_256,
        hmac_sha2_512,
        hmac_sha2_256_etm,
        hmac_sha2_512_etm,
    };

    struct CipherInfo {
        std::string_view name;
        CipherKind kind;
        uint8_t key_size;
        uint8_t iv_size;
        bool aead;   // no MAC negotiated beside it (RFC 5647 §5.1: the MAC's name is ignored)
    };

    struct MacInfo {
        std::string_view name;
        MacKind kind;
        uint8_t key_size;
        uint8_t tag_size;
        bool etm;
    };

    // The ciphers and MACs of the module, in its order of preference
    inline constexpr CipherInfo cipher_table[] = {
        {"chacha20-poly1305@openssh.com", CipherKind::chacha20_poly1305, 64, 0, true},
        {"aes128-gcm@openssh.com", CipherKind::aes128_gcm, 16, 12, true},
        {"aes256-gcm@openssh.com", CipherKind::aes256_gcm, 32, 12, true},
        {"aes128-ctr", CipherKind::aes128_ctr, 16, 16, false},
        {"aes192-ctr", CipherKind::aes192_ctr, 24, 16, false},
        {"aes256-ctr", CipherKind::aes256_ctr, 32, 16, false},
    };

    inline constexpr MacInfo mac_table[] = {
        {"hmac-sha2-256-etm@openssh.com", MacKind::hmac_sha2_256_etm, 32, 32, true},
        {"hmac-sha2-512-etm@openssh.com", MacKind::hmac_sha2_512_etm, 64, 64, true},
        {"hmac-sha2-256", MacKind::hmac_sha2_256, 32, 32, false},
        {"hmac-sha2-512", MacKind::hmac_sha2_512, 64, 64, false},
    };

    inline const CipherInfo* find_cipher(std::string_view name) noexcept {
        for (const auto& c : cipher_table) {
            if (c.name == name) {
                return &c;
            }
        }
        return nullptr;
    }

    inline const MacInfo* find_mac(std::string_view name) noexcept {
        for (const auto& m : mac_table) {
            if (m.name == name) {
                return &m;
            }
        }
        return nullptr;
    }

    // The largest packet taken (packet_length), as OpenSSH's PACKET_MAX_SIZE
    inline constexpr uint32_t MaxPacketLength = 256 * 1024;

    // A failure to open a packet
    enum class OpenError : uint8_t {
        none,
        length,    // a length out of range or not a multiple of the block
        mac,       // a tag or MAC that does not match
        padding,   // a padding length out of range
    };

    // One direction's keys, and the work of sealing and opening packets
    class PacketKeys {
    public:
        PacketKeys() noexcept = default;
        PacketKeys(const PacketKeys&) = delete;
        PacketKeys& operator=(const PacketKeys&) = delete;

        ~PacketKeys() {
            crypto::detail::secure_zero_object(_main);
            crypto::detail::secure_zero_object(_header);
            crypto::detail::secure_zero(_iv, sizeof _iv);
        }

        // The keys of a cipher and a MAC (key and iv of the cipher's sizes,
        // mac_key of the MAC's; the MAC ignored for an AEAD)
        void set(CipherKind c, const uint8_t* key, const uint8_t* iv, MacKind m, const uint8_t* mac_key) {
            _kind = c;
            _mac = MacKind::none;
            _gcm.reset();
            _ctr.reset();
            _h256.reset();
            _h512.reset();
            switch (c) {
                case CipherKind::none:
                    break;
                case CipherKind::chacha20_poly1305:
                    // K_2, the first 32 bytes, for the packet; K_1, the
                    // next 32, for the length
                    crypto::detail::chacha_load(_main, key, _zero_nonce);
                    crypto::detail::chacha_load(_header, key + 32, _zero_nonce);
                    break;
                case CipherKind::aes128_gcm:
                case CipherKind::aes256_gcm:
                    _gcm.emplace(bytes_of(key, c == CipherKind::aes128_gcm ? 16 : 32));
                    sgcl::detail::copy_bytes(_iv, iv, 12);
                    break;
                case CipherKind::aes128_ctr:
                case CipherKind::aes192_ctr:
                case CipherKind::aes256_ctr: {
                    size_t ks = c == CipherKind::aes128_ctr ? 16 : c == CipherKind::aes192_ctr ? 24 : 32;
                    _ctr.emplace(bytes_of(key, ks), bytes_of(iv, 16));
                    _mac = m;
                    if (m == MacKind::hmac_sha2_256 || m == MacKind::hmac_sha2_256_etm) {
                        _h256.emplace(bytes_of(mac_key, 32));
                    } else {
                        _h512.emplace(bytes_of(mac_key, 64));
                    }
                    break;
                }
            }
        }

        SGCL_INLINE_HOT CipherKind kind() const noexcept {
            return _kind;
        }

        // The alignment of the encrypted part
        SGCL_INLINE_HOT size_t block() const noexcept {
            return _ctr || _gcm ? 16 : 8;
        }

        // Whether the 4 bytes of the length count in the alignment (they
        // are encrypted with the rest by a cipher without its own place for
        // them, and in the clear packets of the first exchange)
        SGCL_INLINE_HOT bool length_aligned() const noexcept {
            return _kind == CipherKind::none || (_ctr && !_etm());
        }

        SGCL_INLINE_HOT size_t tag_size() const noexcept {
            switch (_kind) {
                case CipherKind::none: return 0;
                case CipherKind::chacha20_poly1305:
                case CipherKind::aes128_gcm:
                case CipherKind::aes256_gcm: return 16;
                default: return _h256 ? 32 : 64;
            }
        }

        // What a packet of a payload of n bytes adds to it, at most
        SGCL_INLINE_HOT size_t overhead() const noexcept {
            return 4 + 1 + 255 + tag_size();
        }

        // --- sealing ----------------------------------------------------------

        // The packet of a payload appended to out; seq is the direction's
        // sequence number
        SGCL_INLINE_HOT void seal(uint32_t seq, const uint8_t* payload, size_t n, Bytes& out) {
            seal(seq, payload, n, nullptr, 0, out);
        }

        // The same of a payload in two parts, the second after the first
        // (a message's fields and the data it carries)
        void seal(uint32_t seq, const uint8_t* head, size_t head_size, const uint8_t* body, size_t body_size, Bytes& out) {
            const size_t n = head_size + body_size;
            const size_t b = block();
            size_t unpadded = 1 + n + (length_aligned() ? 4 : 0);
            size_t pad = b - unpadded % b;
            if (pad < 4) {
                pad += b;
            }
            const uint32_t length = uint32_t(1 + n + pad);
            const size_t at = out.size();
            const size_t total = 4 + length + tag_size();
            out.resize(at + total);
            uint8_t* p = out.data() + at;
            store32(p, length);
            p[4] = uint8_t(pad);
            if (head_size) {
                sgcl::detail::copy_bytes(p + 5, head, head_size);
            }
            if (body_size) {
                sgcl::detail::copy_bytes(p + 5 + head_size, body, body_size);
            }
            crypto::random::fill(mutable_bytes_of(p + 5 + n, pad));
            switch (_kind) {
                case CipherKind::none:
                    break;
                case CipherKind::chacha20_poly1305:
                    _seal_chacha(seq, p, length);
                    break;
                case CipherKind::aes128_gcm:
                case CipherKind::aes256_gcm:
                    _gcm->seal_to(mutable_bytes_of(p + 4, length + 16), bytes_of(_iv, 12), bytes_of(p + 4, length), bytes_of(p, 4));
                    _next_iv();
                    break;
                default:
                    if (_etm()) {
                        _ctr->xor_key_stream(mutable_bytes_of(p + 4, length), bytes_of(p + 4, length));
                        _mac_of(seq, p, 4 + length, p + 4 + length);
                    } else {
                        _mac_of(seq, p, 4 + length, p + 4 + length);
                        _ctr->xor_key_stream(mutable_bytes_of(p, 4 + length), bytes_of(p, 4 + length));
                    }
                    break;
            }
        }

        // --- opening ----------------------------------------------------------

        // The bytes needed before the length can be read: 16 for a CTR
        // cipher with a MAC over the plaintext (the first block), 4 for the
        // rest
        SGCL_INLINE_HOT size_t head_size() const noexcept {
            return _ctr && !_etm() ? 16 : 4;
        }

        // The packet's length from its first head_size() bytes (decrypted in
        // place for a CTR cipher whose MAC is over the plaintext: its
        // keystream moves on), and the bytes of the whole packet with its
        // tag; OpenError::length for one out of range
        OpenError length(uint32_t seq, uint8_t* head, uint32_t& length, size_t& total) {
            uint32_t l;
            if (_kind == CipherKind::chacha20_poly1305) {
                uint8_t clear[4];
                crypto::detail::ChachaState s = _header;
                _set_nonce(s, seq);
                crypto::detail::chacha_xor(s, 0, head, clear, 4);
                crypto::detail::secure_zero_object(s);
                l = load32(clear);
            } else if (_ctr && !_etm()) {
                _ctr->xor_key_stream(mutable_bytes_of(head, 16), bytes_of(head, 16));
                l = load32(head);
            } else {
                l = load32(head);
            }
            const size_t b = block();
            const size_t aligned = length_aligned() ? size_t(l) + 4 : size_t(l);
            if (l < 5 || l > MaxPacketLength || aligned % b != 0 || 4 + size_t(l) < head_size()) {
                return OpenError::length;
            }
            length = l;
            total = 4 + size_t(l) + tag_size();
            return OpenError::none;
        }

        // The whole packet (total bytes, as length() told), checked and
        // decrypted in place: the payload's place and size in it
        OpenError open(uint32_t seq, uint8_t* p, uint32_t length, size_t& payload_at, size_t& payload_size) {
            switch (_kind) {
                case CipherKind::none:
                    break;
                case CipherKind::chacha20_poly1305:
                    if (!_open_chacha(seq, p, length)) {
                        return OpenError::mac;
                    }
                    break;
                case CipherKind::aes128_gcm:
                case CipherKind::aes256_gcm: {
                    auto r = _gcm->open_to(mutable_bytes_of(p + 4, length), bytes_of(_iv, 12), bytes_of(p + 4, length + 16), bytes_of(p, 4));
                    _next_iv();
                    if (!r) {
                        return OpenError::mac;
                    }
                    break;
                }
                default:
                    if (_etm()) {
                        if (!_mac_matches(seq, p, 4 + length, p + 4 + length)) {
                            return OpenError::mac;
                        }
                        _ctr->xor_key_stream(mutable_bytes_of(p + 4, length), bytes_of(p + 4, length));
                    } else {
                        // the first block was decrypted by length(): the rest
                        _ctr->xor_key_stream(mutable_bytes_of(p + 16, 4 + length - 16), bytes_of(p + 16, 4 + length - 16));
                        if (!_mac_matches(seq, p, 4 + length, p + 4 + length)) {
                            return OpenError::mac;
                        }
                    }
                    break;
            }
            const uint8_t pad = p[4];
            if (pad < 4 || size_t(pad) + 1 > length) {
                return OpenError::padding;
            }
            payload_at = 5;
            payload_size = length - 1 - pad;
            return OpenError::none;
        }

    private:
        static constexpr uint8_t _zero_nonce[12] = {};

        SGCL_INLINE_HOT bool _etm() const noexcept {
            return _mac == MacKind::hmac_sha2_256_etm || _mac == MacKind::hmac_sha2_512_etm;
        }

        // The 64-bit nonce of ChaCha20 as OpenSSH has it: the sequence
        // number in 8 bytes, big-endian, as state words 14 and 15; word 13
        // (the counter's high half) zero
        SGCL_INLINE_HOT static void _set_nonce(crypto::detail::ChachaState& s, uint32_t seq) noexcept {
            uint8_t n[8] = {0, 0, 0, 0, uint8_t(seq >> 24), uint8_t(seq >> 16), uint8_t(seq >> 8), uint8_t(seq)};
            s.nonce[0] = 0;
            s.nonce[1] = crypto::detail::load_le32(n);
            s.nonce[2] = crypto::detail::load_le32(n + 4);
        }

        void _poly_key(const crypto::detail::ChachaState& s, uint8_t* key) const noexcept {
            uint8_t block[64];
            crypto::detail::chacha_block(s, 0, block);
            sgcl::detail::copy_bytes(key, block, 32);
            crypto::detail::secure_zero(block, sizeof block);
        }

        void _seal_chacha(uint32_t seq, uint8_t* p, uint32_t length) noexcept {
            crypto::detail::ChachaState h = _header, m = _main;
            _set_nonce(h, seq);
            _set_nonce(m, seq);
            crypto::detail::chacha_xor(h, 0, p, p, 4);
            crypto::detail::chacha_xor(m, 1, p + 4, p + 4, length);
            uint8_t key[32];
            _poly_key(m, key);
            crypto::detail::Poly1305 mac;
            mac.init(key);
            mac.update(p, 4 + length);
            mac.finish(p + 4 + length);
            crypto::detail::secure_zero(key, sizeof key);
            crypto::detail::secure_zero_object(h);
            crypto::detail::secure_zero_object(m);
            crypto::detail::secure_zero_object(mac);
        }

        bool _open_chacha(uint32_t seq, uint8_t* p, uint32_t length) noexcept {
            crypto::detail::ChachaState m = _main;
            _set_nonce(m, seq);
            uint8_t key[32], tag[16];
            _poly_key(m, key);
            crypto::detail::Poly1305 mac;
            mac.init(key);
            mac.update(p, 4 + length);
            mac.finish(tag);
            const bool ok = crypto::detail::equal_bytes(tag, p + 4 + length, 16);
            if (ok) {
                crypto::detail::chacha_xor(m, 1, p + 4, p + 4, length);
                // the length too, for the caller: what the wire had encrypted
                crypto::detail::ChachaState h = _header;
                _set_nonce(h, seq);
                crypto::detail::chacha_xor(h, 0, p, p, 4);
                crypto::detail::secure_zero_object(h);
            }
            crypto::detail::secure_zero(key, sizeof key);
            crypto::detail::secure_zero_object(m);
            crypto::detail::secure_zero_object(mac);
            return ok;
        }

        // RFC 5647 §7.1: the invocation counter, the nonce's last 8 bytes,
        // one more after each packet
        SGCL_INLINE_HOT void _next_iv() noexcept {
            store64(_iv + 4, load64(_iv + 4) + 1);
        }

        void _mac_of(uint32_t seq, const uint8_t* data, size_t n, uint8_t* out) noexcept {
            uint8_t s[4];
            store32(s, seq);
            if (_h256) {
                _h256->reset();
                _h256->update(bytes_of(s, 4));
                _h256->update(bytes_of(data, n));
                auto t = _h256->value();
                sgcl::detail::copy_bytes(out, t.data(), 32);
            } else {
                _h512->reset();
                _h512->update(bytes_of(s, 4));
                _h512->update(bytes_of(data, n));
                auto t = _h512->value();
                sgcl::detail::copy_bytes(out, t.data(), 64);
            }
        }

        bool _mac_matches(uint32_t seq, const uint8_t* data, size_t n, const uint8_t* tag) noexcept {
            uint8_t mine[64];
            _mac_of(seq, data, n, mine);
            bool ok = crypto::detail::equal_bytes(mine, tag, _h256 ? 32 : 64);
            crypto::detail::secure_zero(mine, sizeof mine);
            return ok;
        }

        CipherKind _kind = CipherKind::none;
        MacKind _mac = MacKind::none;
        crypto::detail::ChachaState _main = {};
        crypto::detail::ChachaState _header = {};
        uint8_t _iv[12] = {};
        optional<crypto::aes_gcm> _gcm;
        optional<crypto::aes_ctr> _ctr;
        optional<crypto::hmac<crypto::sha256>> _h256;
        optional<crypto::hmac<crypto::sha512>> _h512;
    };
}
