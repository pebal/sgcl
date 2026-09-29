//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "../../../crypto/chacha20_poly1305.h"
#include "../../../crypto/gcm.h"

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <memory>
#include <stdexcept>

// The record layer of TLS 1.3 (RFC 8446 §5): the protection of one
// direction (the plaintext records of the first flights, then
// TLSCiphertext under the keys of a traffic secret, §5.2; the per-record
// nonce, §5.3; padding, §5.4; the limits on the keys, §5.5, and their
// update, §7.2), the framing of a byte stream into whole records, and the
// assembly of handshake messages from the records' fragments (§5.1).
//
// Every buffer here is outside the managed heap: the classes live in the
// connection's unmanaged block of secrets, their buffers are their own
// heap blocks, zeroed when a record or a message is done with and when
// they die. A slice this file hands out has no owner and lives only in a
// frame, until the next call on the object that gave it.
namespace sgcl::net::tls::detail {
    inline constexpr size_t MaxPlaintext = 16384;                  // 2^14 (§5.1)
    inline constexpr size_t MaxCiphertext = MaxPlaintext + 256;    // 2^14 + 256 (§5.2)
    inline constexpr size_t HeaderSize = 5;
    inline constexpr size_t TagSize = 16;                          // every AEAD of v1
    inline constexpr size_t MaxRecord = HeaderSize + MaxCiphertext;

    inline Alert record_alert(AlertDescription d, const char* what) noexcept {
        return Alert{d, 0, what};
    }

    // The tests' look at what a connection lets go of (tls_zeroing.cpp):
    // `installed` hears of every traffic secret a direction takes (install,
    // update), `released` of every block of the record layer as it goes
    // back — a record's block, a buffer, the connection's block of secrets
    // after its destructor, before its memory is freed — so that a test can
    // look for the secrets there. Unset (the default) they cost a relaxed
    // load where a block goes; nothing is kept here.
    struct ZeroingProbe {
        static inline std::atomic<void (*)(Cipher, const Secret&)> installed{nullptr};
        enum Kind { record_block, buffer, connection_block };

        static inline std::atomic<void (*)(const void*, size_t, Kind)> released{nullptr};

        static void on_install(Cipher c, const Secret& s) noexcept {
            if (auto f = installed.load(std::memory_order_acquire)) {
                f(c, s);
            }
        }

        static void on_release(const void* p, size_t n, Kind k) noexcept {
            if (auto f = released.load(std::memory_order_acquire)) {
                f(p, n, k);
            }
        }
    };

    // An unmanaged byte buffer that zeroes what it lets go of: the old
    // block when it grows, the whole block when it dies
    class SecureBuffer {
    public:
        explicit SecureBuffer(size_t capacity)
        : _data(new uint8_t[capacity]()), _capacity(capacity) {
        }

        SecureBuffer(const SecureBuffer&) = delete;
        SecureBuffer& operator=(const SecureBuffer&) = delete;

        ~SecureBuffer() {
            crypto::detail::secure_zero(_data.get(), _capacity);
            ZeroingProbe::on_release(_data.get(), _capacity, ZeroingProbe::buffer);
        }

        uint8_t* data() noexcept {
            return _data.get();
        }

        const uint8_t* data() const noexcept {
            return _data.get();
        }

        size_t capacity() const noexcept {
            return _capacity;
        }

        // At least n bytes of room, the first `used` kept
        void reserve(size_t n, size_t used) {
            if (n <= _capacity) {
                return;
            }
            size_t c = _capacity * 2 > n ? _capacity * 2 : n;
            std::unique_ptr<uint8_t[]> d(new uint8_t[c]());
            std::memcpy(d.get(), _data.get(), used);
            crypto::detail::secure_zero(_data.get(), _capacity);
            ZeroingProbe::on_release(_data.get(), _capacity, ZeroingProbe::buffer);
            _data = std::move(d);
            _capacity = c;
        }

    private:
        std::unique_ptr<uint8_t[]> _data;
        size_t _capacity;
    };

    // A record opened: its content type and its fragment, the plaintext
    // without the inner type and the padding. ContentType::invalid with an
    // empty fragment is a record dropped: a compatibility change_cipher_spec
    // (§5, D.4) or an early data record skipped after 0-RTT was refused
    // (§4.2.10).
    struct Opened {
        ContentType type = ContentType::invalid;
        slice<const byte> fragment;
    };

    // The protection of one direction of a connection. Until install() its
    // records are TLSPlaintext; after it, TLSCiphertext under the keys of
    // the traffic secret given, the sequence number starting at zero with
    // every install and update.
    class RecordProtection {
    public:
        RecordProtection() noexcept = default;
        RecordProtection(const RecordProtection&) = delete;
        RecordProtection& operator=(const RecordProtection&) = delete;

        ~RecordProtection() {
            _drop_keys();
        }

        // The keys of a traffic secret (§7.3) for the cipher suite; the
        // secret is kept for update()
        void install(Cipher cipher, const Secret& traffic_secret) {
            _cipher = cipher;
            _hash = hash_of(cipher);
            std::memcpy(_secret.bytes, traffic_secret.bytes, sizeof _secret.bytes);
            _secret.size = traffic_secret.size;
            _derive();
        }

        bool installed() const noexcept {
            return _installed;
        }

        Cipher cipher() const noexcept {
            return _cipher;
        }

        uint64_t sequence() const noexcept {
            return _sequence;
        }

        // The next traffic secret and its keys (§7.2), after a KeyUpdate
        // sent or received
        void update() {
            if (!_installed) {
                throw std::logic_error("sgcl::net::tls: a key update with no keys installed");
            }
            update_traffic_secret(_hash, _secret);
            _derive();
        }

        // Whether the keys are near their limit (§5.5) and the side that
        // writes should send a KeyUpdate: 2^24 records under AES-GCM (the
        // RFC's bound is 2^24.5 full-size records), 2^48 under
        // ChaCha20-Poly1305 (whose bound is past the sequence number)
        bool needs_update() const noexcept {
            if (!_installed) {
                return false;
            }
            const uint64_t limit = _cipher == Cipher::chacha20_poly1305_sha256 ? uint64_t(1) << 48 : uint64_t(1) << 24;
            return _sequence >= limit;
        }

        // The bytes a record of the type with n bytes of content and the
        // padding takes
        size_t sealed_size(ContentType type, size_t n, size_t padding = 0) const noexcept {
            return _installed && type != ContentType::change_cipher_spec ? HeaderSize + n + 1 + padding + TagSize : HeaderSize + n;
        }

        // One record of the fragment into out: sealed_size(type,
        // fragment.size(), padding) bytes, the count returned. The fragment
        // may already sit at out + HeaderSize. Before install() a TLSPlaintext (padding must be
        // 0); after it a TLSCiphertext with the inner type and `padding`
        // zeros (§5.4). A change_cipher_spec is always the plaintext record
        // of compatibility mode, the single byte 1 (§D.4). A fragment over
        // 2^14 bytes, or content and padding over 2^14 + 1, is a broken
        // contract (std::length_error).
        size_t seal(ContentType type, const slice<const byte>& fragment, uint8_t* out, size_t padding = 0) {
            const size_t n = fragment.size();
            const uint8_t* in = reinterpret_cast<const uint8_t*>(fragment.data());
            if (n > MaxPlaintext || (_installed && n + 1 + padding > MaxPlaintext + 1)) {
                throw std::length_error("sgcl::net::tls: a record's content over 2^14 bytes");
            }
            if (!_installed || type == ContentType::change_cipher_spec) {
                if (padding != 0) {
                    throw std::logic_error("sgcl::net::tls: padding on a plaintext record");
                }
                uint16_t version = Tls12;
                if (type == ContentType::handshake) {
                    // The records of the initial ClientHello may carry 0x0301
                    // (§5.1), as Go and OpenSSL send them
                    if (!_plain_written && n >= 4 && in[0] == uint8_t(HandshakeType::client_hello)) {
                        _hello_left = 4 + (size_t(in[1]) << 16 | size_t(in[2]) << 8 | in[3]);
                    }
                    if (_hello_left > 0) {
                        version = 0x0301;
                        _hello_left -= n < _hello_left ? n : _hello_left;
                    }
                    _plain_written = true;
                }
                if (n > 0 && in != out + HeaderSize) {
                    std::memmove(out + HeaderSize, in, n);
                }
                _header(out, type, version, n);
                return HeaderSize + n;
            }
            if (_sequence == ~uint64_t(0)) {
                throw std::logic_error("sgcl::net::tls: the sequence number is exhausted (no key update)");
            }
            const size_t inner = n + 1 + padding;
            uint8_t* body = out + HeaderSize;
            if (n > 0 && in != body) {
                std::memmove(body, in, n);
            }
            body[n] = uint8_t(type);
            if (padding > 0) {
                std::memset(body + n + 1, 0, padding);
            }
            _header(out, ContentType::application_data, Tls12, inner + TagSize);
            uint8_t nonce[12];
            _nonce(nonce);
            const auto room = room_of(body, inner + TagSize);
            const auto plain = bytes_of(body, inner);
            const auto aad = bytes_of(out, HeaderSize);
            if (_cipher == Cipher::chacha20_poly1305_sha256) {
                _chacha->seal_to(room, bytes_of(nonce, 12), plain, aad);
            } else {
                _gcm->seal_to(room, bytes_of(nonce, 12), plain, aad);
            }
            ++_sequence;
            return HeaderSize + inner + TagSize;
        }

        // A whole record of `size` bytes (the header's length matching, as
        // RecordFramer gives it) opened into out, which may be record +
        // HeaderSize (in place) or a buffer of at least size - HeaderSize
        // bytes that overlaps no byte of the record. Errors: bad_record_mac,
        // record_overflow, unexpected_message, decode_error.
        [[nodiscard]] expected<Opened, Alert> open(uint8_t* record, size_t size, uint8_t* out) {
            if (size < HeaderSize || size - HeaderSize != (size_t(record[3]) << 8 | record[4])) {
                return unexpected(record_alert(AlertDescription::decode_error, "a record's length does not match its header"));
            }
            const ContentType type = ContentType(record[0]);
            const size_t length = size - HeaderSize;
            if (type == ContentType::change_cipher_spec) {
                // §5: the single byte 1, unprotected, between the first
                // ClientHello and the peer's Finished, dropped
                if (!_ccs_accepted) {
                    return unexpected(record_alert(AlertDescription::unexpected_message, "a change_cipher_spec record outside compatibility mode"));
                }
                if (length != 1 || record[HeaderSize] != 1) {
                    return unexpected(record_alert(AlertDescription::unexpected_message, "a change_cipher_spec record other than the byte 1"));
                }
                return Opened{};
            }
            if (!_installed) {
                if (type != ContentType::handshake && type != ContentType::alert) {
                    return unexpected(record_alert(AlertDescription::unexpected_message, "a plaintext record of a type other than handshake or alert"));
                }
                if (length > MaxPlaintext) {
                    return unexpected(record_alert(AlertDescription::record_overflow, "a plaintext record over 2^14 bytes"));
                }
                if (length == 0) {
                    return unexpected(record_alert(AlertDescription::unexpected_message, "an empty handshake or alert record"));
                }
                if (out != record + HeaderSize) {
                    std::memcpy(out, record + HeaderSize, length);
                }
                return Opened{type, bytes_of(out, length)};
            }
            if (type != ContentType::application_data) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "a plaintext record after the keys"));
            }
            if (length > MaxCiphertext) {
                return unexpected(record_alert(AlertDescription::record_overflow, "a protected record over 2^14 + 256 bytes"));
            }
            if (length < TagSize + 1) {
                return _failed(length);
            }
            if (_sequence == ~uint64_t(0)) {
                return unexpected(record_alert(AlertDescription::internal_error, "the sequence number is exhausted (no key update)"));
            }
            uint8_t nonce[12];
            _nonce(nonce);
            const auto room = room_of(out, length - TagSize);
            const auto sealed = bytes_of(record + HeaderSize, length);
            const auto aad = bytes_of(record, HeaderSize);
            const bool ok = _cipher == Cipher::chacha20_poly1305_sha256
                ? _chacha->open_to(room, bytes_of(nonce, 12), sealed, aad).has_value()
                : _gcm->open_to(room, bytes_of(nonce, 12), sealed, aad).has_value();
            if (!ok) {
                return _failed(length);
            }
            _skipping = false;
            ++_sequence;
            if (length - TagSize > MaxPlaintext + 1) {
                return unexpected(record_alert(AlertDescription::record_overflow, "a protected record's content over 2^14 + 1 bytes"));
            }
            // The inner type is the last byte that is not zero (§5.4)
            size_t n = length - TagSize;
            while (n > 0 && out[n - 1] == 0) {
                --n;
            }
            if (n == 0) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "a protected record with no content type"));
            }
            const ContentType inner = ContentType(out[n - 1]);
            --n;
            if (inner != ContentType::handshake && inner != ContentType::alert && inner != ContentType::application_data) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "a protected record of an unknown or forbidden inner type"));
            }
            if (n == 0 && inner != ContentType::application_data) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "an empty handshake or alert record"));
            }
            return Opened{inner, bytes_of(out, n)};
        }

        [[nodiscard]] expected<Opened, Alert> open(uint8_t* record, size_t size) {
            return open(record, size, record + HeaderSize);
        }

        // Compatibility mode (§D.4): a change_cipher_spec record is dropped
        // while this is on (after the first ClientHello is sent or
        // received, until the peer's Finished), refused otherwise
        void accept_ccs(bool on) noexcept {
            _ccs_accepted = on;
        }

        // 0-RTT refused (§4.2.10): a protected record that does not open is
        // dropped, up to `limit` bytes of such records in all; the first
        // record that opens ends it
        void skip_undecryptable(size_t limit) noexcept {
            _skipping = true;
            _skip_left = limit;
        }

    private:
        Cipher _cipher = Cipher::aes_128_gcm_sha256;
        Hash _hash = Hash::sha256;
        bool _installed = false;
        bool _plain_written = false;
        bool _ccs_accepted = false;
        bool _skipping = false;
        size_t _hello_left = 0;
        size_t _skip_left = 0;
        uint64_t _sequence = 0;
        Secret _secret;
        uint8_t _iv[12] = {};
        optional<crypto::aes_gcm> _gcm;
        optional<crypto::chacha20_poly1305> _chacha;

        void _derive() {
            _drop_keys();
            TrafficKeys keys;
            traffic_keys(_hash, keys, _secret, key_size(_cipher));
            std::memcpy(_iv, keys.iv, 12);
            if (_cipher == Cipher::chacha20_poly1305_sha256) {
                _chacha.emplace(bytes_of(keys.key, keys.key_size));
            } else {
                _gcm.emplace(bytes_of(keys.key, keys.key_size));
            }
            _installed = true;
            _sequence = 0;
            ZeroingProbe::on_install(_cipher, _secret);
        }

        void _drop_keys() noexcept {
            _gcm.reset();
            _chacha.reset();
            crypto::detail::secure_zero(_iv, sizeof _iv);
        }

        // §5.3: the IV XOR the sequence number, big-endian, left-padded
        void _nonce(uint8_t* nonce) const noexcept {
            std::memcpy(nonce, _iv, 12);
            for (int i = 0; i < 8; ++i) {
                nonce[11 - i] ^= uint8_t(_sequence >> (8 * i));
            }
        }

        static void _header(uint8_t* out, ContentType type, uint16_t version, size_t length) noexcept {
            out[0] = uint8_t(type);
            out[1] = uint8_t(version >> 8);
            out[2] = uint8_t(version);
            out[3] = uint8_t(length >> 8);
            out[4] = uint8_t(length);
        }

        expected<Opened, Alert> _failed(size_t length) {
            if (_skipping && length <= _skip_left) {
                _skip_left -= length;
                return Opened{};
            }
            _skipping = false;
            return unexpected(record_alert(AlertDescription::bad_record_mac, "a record that does not authenticate"));
        }
    };

    // The unmanaged blocks the record layer reads records into, lent for as
    // long as a record needs one and taken back zeroed: a free list per
    // thread (a worker's), of two sizes — a small block for the records of
    // a request or a response, a large one for two whole records of the
    // largest size. A block is not zeroed when first made, so its pages are
    // touched only as far as records reach; a list keeps at most 64
    // blocks, the rest go back to the system.
    class RecordBlocks {
    public:
        static constexpr size_t Small = 4096;
        static constexpr size_t Large = 2 * MaxRecord;
        static constexpr size_t Kept = 64;

        static uint8_t* take(size_t size) {
            auto& list = _list(size);
            if (!list.empty()) {
                uint8_t* p = list.back();
                list.pop_back();
                return p;
            }
            return new uint8_t[size];
        }

        // The first `used` bytes zeroed (what was ever written), then kept
        static void give(uint8_t* p, size_t size, size_t used) noexcept {
            crypto::detail::secure_zero(p, used);
            ZeroingProbe::on_release(p, size, ZeroingProbe::record_block);
            auto& list = _list(size);
            if (list.size() < Kept) {
                list.push_back(p);   // capacity reserved: no allocation here
            } else {
                delete[] p;
            }
        }

    private:
        struct Lists {
            std::vector<uint8_t*> small, large;

            Lists() {
                small.reserve(Kept);
                large.reserve(Kept);
            }

            ~Lists() {
                for (auto* p : small) {
                    delete[] p;
                }
                for (auto* p : large) {
                    delete[] p;
                }
            }
        };

        static std::vector<uint8_t*>& _list(size_t size) noexcept {
            thread_local Lists lists;
            return size == Small ? lists.small : lists.large;
        }
    };

    // Whole records out of a byte stream: room() to read into, commit(n)
    // for the bytes read, next() for the record at the front once all of
    // it is here (its header checked: a known content type, §5; a length
    // of at most 2^14 + 256, §5.2), consume() when done with it, which
    // zeroes its bytes (it may hold plaintext opened in place). The buffer
    // is a block of RecordBlocks, taken by room() and given back zeroed as
    // soon as nothing is held (consume() of the last record, release()): a
    // small one first, a large one when a record's header says it is
    // longer than the small one holds.
    class RecordFramer {
    public:
        static constexpr size_t Capacity = RecordBlocks::Large;

        RecordFramer() noexcept = default;
        RecordFramer(const RecordFramer&) = delete;
        RecordFramer& operator=(const RecordFramer&) = delete;

        ~RecordFramer() {
            _give();
        }

        // The free room at the end, after moving the bytes still to be
        // framed to the front; never empty while no whole record waits
        slice<byte> room() {
            _drop();
            if (!_data) {
                _data = RecordBlocks::take(RecordBlocks::Small);
                _cap = RecordBlocks::Small;
                _start = _end = _written = 0;
            }
            if (_start == _end) {
                _start = _end = 0;
            }
            // the record at the front: all of it must fit
            size_t need = HeaderSize;
            if (_end - _start >= HeaderSize) {
                const uint8_t* h = _data + _start;
                need = HeaderSize + (size_t(h[3]) << 8 | h[4]);
            }
            if (need > _cap - _start || (_cap == Capacity && _end + MaxRecord > _cap) || _end == _cap) {
                if (need > _cap) {
                    _grow();
                } else if (_start > 0) {
                    const size_t rest = _end - _start;
                    std::memmove(_data, _data + _start, rest);
                    crypto::detail::secure_zero(_data + rest, _end - rest);
                    _start = 0;
                    _end = rest;
                }
            }
            return room_of(_data + _end, _cap - _end);
        }

        void commit(size_t n) noexcept {
            assert(_data && n <= _cap - _end);
            _end += n;
            _written = _end > _written ? _end : _written;
        }

        // The record at the front, header included, or an empty slice when
        // more bytes are needed
        [[nodiscard]] expected<slice<byte>, Alert> next() noexcept {
            _drop();
            const size_t have = _end - _start;
            if (have < HeaderSize) {
                return slice<byte>();
            }
            const uint8_t* h = _data + _start;
            if (h[0] < uint8_t(ContentType::change_cipher_spec) || h[0] > uint8_t(ContentType::application_data)) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "a record of an unknown content type"));
            }
            const size_t length = size_t(h[3]) << 8 | h[4];
            if (length > MaxCiphertext) {
                return unexpected(record_alert(AlertDescription::record_overflow, "a record over 2^14 + 256 bytes"));
            }
            if (have < HeaderSize + length) {
                return slice<byte>();
            }
            _taken = HeaderSize + length;
            return room_of(_data + _start, _taken);
        }

        // Done with the record next() gave; the block goes back when nothing
        // else is held
        void consume() noexcept {
            _drop();
            release();
        }

        // The block given back when it holds nothing (after a read that
        // brought nothing, say)
        void release() noexcept {
            if (_data && _start == _end && _taken == 0) {
                _give();
            }
        }

        size_t buffered() const noexcept {
            return _end - _start - _taken;
        }

    private:
        uint8_t* _data = nullptr;
        size_t _cap = 0;
        size_t _start = 0;
        size_t _end = 0;
        size_t _taken = 0;
        size_t _written = 0;   // the bytes of the block ever written: what is zeroed when it goes back

        void _drop() noexcept {
            if (_taken > 0) {
                crypto::detail::secure_zero(_data + _start, _taken);
                _start += _taken;
                _taken = 0;
            }
        }

        void _grow() {
            uint8_t* d = RecordBlocks::take(Capacity);
            const size_t rest = _end - _start;
            std::memcpy(d, _data + _start, rest);
            RecordBlocks::give(_data, _cap, _written);
            _data = d;
            _cap = Capacity;
            _start = 0;
            _end = _written = rest;
        }

        void _give() noexcept {
            if (_data) {
                RecordBlocks::give(_data, _cap, _written);
                _data = nullptr;
                _cap = _start = _end = _taken = _written = 0;
            }
        }
    };

    // Handshake messages out of the fragments of handshake records (§5.1):
    // a message may span records and a record may hold several, but no
    // message spans a change of keys. push() the fragment of each record
    // with the epoch it was read under; next() gives each whole message,
    // its header included, valid until the next push() or next(). The
    // length in a message's header is checked as soon as the header is
    // here, before its body is buffered: at most 64 KiB, 256 KiB for a
    // Certificate (Go's limits), illegal_parameter beyond them (as
    // OpenSSL and BoringSSL). Bytes done with are zeroed.
    class HandshakeAssembler {
    public:
        static constexpr size_t MaxMessage = 65536;
        static constexpr size_t MaxCertificate = 262144;

        HandshakeAssembler() noexcept = default;

        HandshakeAssembler(const HandshakeAssembler&) = delete;
        HandshakeAssembler& operator=(const HandshakeAssembler&) = delete;

        [[nodiscard]] expected<void, Alert> push(const slice<const byte>& fragment, Epoch epoch) {
            _drop();
            if (fragment.empty()) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "an empty handshake fragment"));
            }
            if (_end > _start && epoch != _epoch) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "a handshake message spans a change of keys"));
            }
            _epoch = epoch;
            if (!_buffer) {
                _buffer = std::make_unique<SecureBuffer>(MaxPlaintext);   // for as long as a message is held
            }
            // Move what is left to the front, then append
            if (_start > 0) {
                const size_t rest = _end - _start;
                std::memmove(_buffer->data(), _buffer->data() + _start, rest);
                crypto::detail::secure_zero(_buffer->data() + rest, _end - rest);
                _start = 0;
                _end = rest;
            }
            _buffer->reserve(_end + fragment.size(), _end);
            std::memcpy(_buffer->data() + _end, fragment.data(), fragment.size());
            _end += fragment.size();
            // Every header now whole, checked before more is buffered
            size_t at = _start;
            while (_end - at >= 4) {
                const uint8_t* h = _buffer->data() + at;
                const size_t length = size_t(h[1]) << 16 | size_t(h[2]) << 8 | h[3];
                const size_t limit = h[0] == uint8_t(HandshakeType::certificate) ? MaxCertificate : MaxMessage;
                if (length > limit) {
                    return unexpected(record_alert(AlertDescription::illegal_parameter, "a handshake message over the size limit"));
                }
                if (_end - at < 4 + length) {
                    break;
                }
                at += 4 + length;
            }
            return {};
        }

        // The next whole message, header included, or nullopt
        optional<slice<const byte>> next() noexcept {
            _drop();
            const size_t have = _end - _start;
            if (have < 4) {
                return nullopt;
            }
            const uint8_t* h = _buffer->data() + _start;
            const size_t length = size_t(h[1]) << 16 | size_t(h[2]) << 8 | h[3];
            if (have < 4 + length) {
                return nullopt;
            }
            _taken = 4 + length;
            return bytes_of(h, _taken);
        }

        // The keys of the direction change (§5.1): nothing may be left of a
        // message read under the old ones, whole or in part
        [[nodiscard]] expected<void, Alert> on_key_change() noexcept {
            _drop();
            if (_end > _start) {
                return unexpected(record_alert(AlertDescription::unexpected_message, "a handshake message not aligned with a change of keys"));
            }
            return {};
        }

        // Whether a message, whole or in part, waits
        bool empty() const noexcept {
            return _end - _start == _taken;
        }

    private:
        std::unique_ptr<SecureBuffer> _buffer;   // while a message is held; zeroed when it goes
        size_t _start = 0;
        size_t _end = 0;
        size_t _taken = 0;
        Epoch _epoch = Epoch::initial;

        void _drop() noexcept {
            if (_taken > 0) {
                crypto::detail::secure_zero(_buffer->data() + _start, _taken);
                _start += _taken;
                _taken = 0;
                if (_start == _end) {
                    _start = _end = 0;
                    _buffer.reset();
                }
            }
        }
    };
}
