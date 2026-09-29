//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../../crypto/hkdf.h"
#include "../../../crypto/hmac.h"
#include "../../../crypto/secure_zero.h"
#include "../../../crypto/sha256.h"
#include "../../../crypto/sha512.h"
#include "../../../core/aliases.h"
#include "../../../core/slice.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

// The key schedule of TLS 1.3 (RFC 8446 §7.1): HKDF-Expand-Label,
// Derive-Secret, the transcript hash, the chain of secrets from the
// early secret through the handshake secret to the master secret, the
// traffic secrets of both directions and the keys and IVs made from them,
// the Finished keys and their verify_data (§4.4.4), and the next traffic
// secret of a KeyUpdate (§7.2). The hash is the cipher suite's: SHA-256
// for TLS_AES_128_GCM_SHA256 and TLS_CHACHA20_POLY1305_SHA256, SHA-384
// for TLS_AES_256_GCM_SHA384.
//
// Every secret here lives in memory that is not managed: a Secret is a
// fixed array in the object that holds it (the schedule, itself inside
// the connection's unmanaged block of secrets), zeroed when it dies and
// when it is moved from; a secret the schedule no longer needs (the early
// and handshake secrets once the master secret is made, a traffic secret
// replaced by its update) is zeroed at once. Nothing here allocates.
namespace sgcl::net::tls::detail {
    enum class Hash : uint8_t { sha256, sha384 };

    inline constexpr size_t MaxHashSize = 48;

    constexpr size_t hash_size(Hash h) noexcept {
        return h == Hash::sha256 ? 32 : 48;
    }

    inline slice<const byte> bytes_of(const void* p, size_t n) noexcept {
        return slice<const byte>(reinterpret_cast<const byte*>(p), n);
    }

    inline slice<byte> room_of(void* p, size_t n) noexcept {
        return slice<byte>(reinterpret_cast<byte*>(p), n);
    }

    // A secret of the schedule: up to 48 bytes and their count, in the
    // object that holds it, zeroed when it dies and when moved from
    struct Secret {
        uint8_t bytes[MaxHashSize] = {};
        uint8_t size = 0;

        Secret() noexcept = default;
        Secret(const Secret&) = delete;
        Secret& operator=(const Secret&) = delete;

        Secret(Secret&& other) noexcept {
            *this = std::move(other);
        }

        Secret& operator=(Secret&& other) noexcept {
            if (this != &other) {
                std::memcpy(bytes, other.bytes, sizeof bytes);
                size = other.size;
                other.wipe();
            }
            return *this;
        }

        ~Secret() {
            wipe();
        }

        void wipe() noexcept {
            crypto::detail::secure_zero(bytes, sizeof bytes);
            size = 0;
        }

        slice<const byte> view() const noexcept {
            return bytes_of(bytes, size);
        }

        bool empty() const noexcept {
            return size == 0;
        }
    };

    // HKDF-Expand-Label(secret, label, context, out.size()) (§7.1):
    // HKDF-Expand with the info HkdfLabel = uint16 length, the label
    // "tls13 " + label as a vector of one-byte length, the context as one
    template<class H>
    inline void expand_label_with(const slice<byte>& out, const slice<const byte>& secret, const char* label, const slice<const byte>& context) {
        size_t label_size = std::strlen(label);
        assert(out.size() <= 0xFFFF && 6 + label_size <= 255 && context.size() <= 255);
        uint8_t info[2 + 1 + 255 + 1 + 255];
        size_t n = 0;
        info[n++] = uint8_t(out.size() >> 8);
        info[n++] = uint8_t(out.size());
        info[n++] = uint8_t(6 + label_size);
        std::memcpy(info + n, "tls13 ", 6);
        n += 6;
        std::memcpy(info + n, label, label_size);
        n += label_size;
        info[n++] = uint8_t(context.size());
        if (!context.empty()) {
            std::memcpy(info + n, context.data(), context.size());
        }
        n += context.size();
        crypto::hkdf<H>::expand_to(out, secret, bytes_of(info, n));
    }

    inline void expand_label(Hash h, const slice<byte>& out, const slice<const byte>& secret, const char* label, const slice<const byte>& context) {
        if (h == Hash::sha256) {
            expand_label_with<crypto::sha256>(out, secret, label, context);
        } else {
            expand_label_with<crypto::sha384>(out, secret, label, context);
        }
    }

    // HKDF-Extract(salt, ikm) into a secret of the hash's size
    inline void extract(Hash h, Secret& out, const slice<const byte>& salt, const slice<const byte>& ikm) noexcept {
        if (h == Hash::sha256) {
            auto prk = crypto::hkdf<crypto::sha256>::extract(salt, ikm);
            std::memcpy(out.bytes, prk.bytes().data(), 32);
            out.size = 32;
        } else {
            auto prk = crypto::hkdf<crypto::sha384>::extract(salt, ikm);
            std::memcpy(out.bytes, prk.bytes().data(), 48);
            out.size = 48;
        }
    }

    // The transcript hash (§4.4.1): the hash of the handshake messages so
    // far, read at any point without ending it
    class Transcript {
    public:
        explicit Transcript(Hash h) noexcept
        : _hash(h) {
        }

        Hash hash() const noexcept {
            return _hash;
        }

        size_t size() const noexcept {
            return hash_size(_hash);
        }

        void update(const slice<const byte>& message) noexcept {
            if (_hash == Hash::sha256) {
                _sha256.update(message);
            } else {
                _sha384.update(message);
            }
        }

        // The hash of what came so far, into out (size() bytes)
        void value_to(uint8_t* out) const noexcept {
            if (_hash == Hash::sha256) {
                auto v = _sha256.value();
                std::memcpy(out, v.data(), 32);
            } else {
                auto v = _sha384.value();
                std::memcpy(out, v.data(), 48);
            }
        }

    private:
        Hash _hash;
        crypto::sha256 _sha256;
        crypto::sha384 _sha384;
    };

    // Derive-Secret(secret, label, messages) = HKDF-Expand-Label(secret,
    // label, Transcript-Hash(messages), Hash.length), the transcript hash
    // given
    inline void derive_secret(Hash h, Secret& out, const Secret& secret, const char* label, const slice<const byte>& transcript_hash) {
        out.size = uint8_t(hash_size(h));
        expand_label(h, room_of(out.bytes, out.size), secret.view(), label, transcript_hash);
    }

    // The hash of nothing, the context of Derive-Secret(., "derived", "")
    inline void empty_hash(Hash h, uint8_t* out) noexcept {
        Transcript t(h);
        t.value_to(out);
    }

    // The keys of one direction: the AEAD's key and the IV the record
    // numbers are XORed into (§7.3)
    struct TrafficKeys {
        uint8_t key[32] = {};
        uint8_t iv[12] = {};
        uint8_t key_size = 0;

        TrafficKeys() noexcept = default;
        TrafficKeys(const TrafficKeys&) = delete;
        TrafficKeys& operator=(const TrafficKeys&) = delete;

        ~TrafficKeys() {
            crypto::detail::secure_zero(key, sizeof key);
            crypto::detail::secure_zero(iv, sizeof iv);
        }
    };

    // key = HKDF-Expand-Label(secret, "key", "", key_size), iv =
    // HKDF-Expand-Label(secret, "iv", "", 12)
    inline void traffic_keys(Hash h, TrafficKeys& out, const Secret& secret, size_t key_size) {
        assert(key_size <= sizeof out.key);
        out.key_size = uint8_t(key_size);
        expand_label(h, room_of(out.key, key_size), secret.view(), "key", slice<const byte>());
        expand_label(h, room_of(out.iv, 12), secret.view(), "iv", slice<const byte>());
    }

    // application_traffic_secret_N+1 = HKDF-Expand-Label(secret_N, "traffic
    // upd", "", Hash.length), in place (§7.2): the old secret gone
    inline void update_traffic_secret(Hash h, Secret& secret) {
        Secret next;
        next.size = uint8_t(hash_size(h));
        expand_label(h, room_of(next.bytes, next.size), secret.view(), "traffic upd", slice<const byte>());
        secret = std::move(next);
    }

    // finished_key = HKDF-Expand-Label(base, "finished", "", Hash.length);
    // verify_data = HMAC(finished_key, transcript hash) (§4.4.4), into out
    // (Hash.length bytes)
    inline void verify_data(Hash h, uint8_t* out, const Secret& base, const slice<const byte>& transcript_hash) {
        Secret finished_key;
        finished_key.size = uint8_t(hash_size(h));
        expand_label(h, room_of(finished_key.bytes, finished_key.size), base.view(), "finished", slice<const byte>());
        if (h == Hash::sha256) {
            auto tag = crypto::hmac<crypto::sha256>::of(transcript_hash, finished_key.view());
            std::memcpy(out, tag.data(), 32);
            crypto::detail::secure_zero(tag.data(), tag.size());
        } else {
            auto tag = crypto::hmac<crypto::sha384>::of(transcript_hash, finished_key.view());
            std::memcpy(out, tag.data(), 48);
            crypto::detail::secure_zero(tag.data(), tag.size());
        }
    }

    // The chain of §7.1 without a PSK: early secret = HKDF-Extract(0, 0);
    // handshake secret = HKDF-Extract(Derive-Secret(early, "derived", ""),
    // (EC)DHE); master secret = HKDF-Extract(Derive-Secret(handshake,
    // "derived", ""), 0); and what each gives. The steps in their order,
    // each zeroing what the next no longer needs
    class KeySchedule {
    public:
        explicit KeySchedule(Hash h)
        : _hash(h) {
            uint8_t zeros[MaxHashSize] = {};
            extract(_hash, _early, slice<const byte>(), bytes_of(zeros, hash_size(_hash)));
        }

        Hash hash() const noexcept {
            return _hash;
        }

        size_t hash_length() const noexcept {
            return hash_size(_hash);
        }

        // With the shared secret of the key exchange and the transcript
        // hash of ClientHello..ServerHello: the handshake secret and the
        // two handshake traffic secrets. The early secret is zeroed
        void handshake(const slice<const byte>& shared_secret, const slice<const byte>& hello_hash) {
            Secret derived;
            _derived(derived, _early);
            extract(_hash, _handshake, derived.view(), shared_secret);
            derive_secret(_hash, client_handshake_traffic, _handshake, "c hs traffic", hello_hash);
            derive_secret(_hash, server_handshake_traffic, _handshake, "s hs traffic", hello_hash);
            _early.wipe();
        }

        // With the transcript hash of ClientHello..server Finished: the
        // master secret, the application traffic secrets of both
        // directions and the exporter master secret. The handshake secret
        // is zeroed (its traffic secrets stay until the caller is done
        // with the handshake's records: finish_handshake())
        void application(const slice<const byte>& server_finished_hash) {
            Secret derived;
            _derived(derived, _handshake);
            uint8_t zeros[MaxHashSize] = {};
            extract(_hash, _master, derived.view(), bytes_of(zeros, hash_size(_hash)));
            derive_secret(_hash, client_application_traffic, _master, "c ap traffic", server_finished_hash);
            derive_secret(_hash, server_application_traffic, _master, "s ap traffic", server_finished_hash);
            derive_secret(_hash, exporter_master, _master, "exp master", server_finished_hash);
            _handshake.wipe();
        }

        // The handshake's traffic secrets and the master secret gone, once
        // both Finished are through (no resumption in v1: the resumption
        // master secret is never made)
        void finish_handshake() noexcept {
            client_handshake_traffic.wipe();
            server_handshake_traffic.wipe();
            _master.wipe();
        }

        Secret client_handshake_traffic, server_handshake_traffic;
        Secret client_application_traffic, server_application_traffic;
        Secret exporter_master;

        // For the tests: the chain's own secrets while they live
        const Secret& early_secret() const noexcept {
            return _early;
        }

        const Secret& handshake_secret() const noexcept {
            return _handshake;
        }

        const Secret& master_secret() const noexcept {
            return _master;
        }

    private:
        void _derived(Secret& out, const Secret& from) {
            uint8_t empty[MaxHashSize];
            empty_hash(_hash, empty);
            derive_secret(_hash, out, from, "derived", bytes_of(empty, hash_size(_hash)));
        }

        Hash _hash;
        Secret _early, _handshake, _master;
    };
}
