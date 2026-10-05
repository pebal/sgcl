//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "schedule.h"
#include "types.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// The key schedule of the client's TLS 1.2 (RFC 5246 §5, §6.3, §7.4.9;
// RFC 7627): the PRF, P_SHA256 or P_SHA384 by the suite; the master secret
// of the extended master secret (the only one this client makes: a server
// without it is refused); the key block, each direction's AEAD key and its
// fixed IV; the Finished's verify_data. As schedule.h: every secret lies in
// a Secret (a fixed array zeroed when it goes), nothing allocates, and
// every intermediate value is zeroed before it leaves the stack.
namespace sgcl::net::tls::detail {
    // P_hash(secret, label || seed1 || seed2) into out (§5)
    template<class H>
    void p_hash(const slice<byte>& out, const slice<const byte>& secret, const char* label, const slice<const byte>& seed1, const slice<const byte>& seed2) noexcept {
        const auto l = bytes_of(label, std::strlen(label));
        crypto::hmac<H> keyed(secret);
        auto seeded = [&](crypto::hmac<H>& m) noexcept {
            m.update(l);
            m.update(seed1);
            m.update(seed2);
        };
        // A(1) = HMAC(secret, A(0)), A(0) the seed
        crypto::hmac<H> first = keyed.clone();
        seeded(first);
        auto a = first.value();
        size_t at = 0;
        while (at < out.size()) {
            crypto::hmac<H> block = keyed.clone();
            block.update(bytes_of(a.data(), a.size()));
            seeded(block);
            auto b = block.value();
            const size_t n = std::min(b.size(), out.size() - at);
            std::memcpy(out.data() + at, b.data(), n);
            crypto::detail::secure_zero(b.data(), b.size());
            at += n;
            crypto::hmac<H> next = keyed.clone();
            next.update(bytes_of(a.data(), a.size()));
            a = next.value();
        }
        crypto::detail::secure_zero(a.data(), a.size());
    }

    // PRF(secret, label, seed1 || seed2) of the suite's hash (§5)
    SGCL_INLINE_HOT void prf12(Hash h, const slice<byte>& out, const slice<const byte>& secret, const char* label, const slice<const byte>& seed1,
                               const slice<const byte>& seed2 = slice<const byte>()) noexcept {
        if (h == Hash::sha256) {
            p_hash<crypto::sha256>(out, secret, label, seed1, seed2);
        } else {
            p_hash<crypto::sha384>(out, secret, label, seed1, seed2);
        }
    }

    // master_secret = PRF(pre_master_secret, "extended master secret",
    // session_hash)[0..47] (RFC 7627 §4), session_hash the transcript's hash
    // through ClientKeyExchange
    SGCL_INLINE_HOT void extended_master_secret(Hash h, Secret& out, const slice<const byte>& pre_master, const slice<const byte>& session_hash) noexcept {
        out.size = 48;
        prf12(h, room_of(out.bytes, 48), pre_master, "extended master secret", session_hash);
    }

    // The key block (§6.3): client_write_key, server_write_key,
    // client_write_IV, server_write_IV (no MAC keys: an AEAD), each
    // direction's key and IV given as one Secret, the key first
    inline void key_block12(Cipher c, Secret& client, Secret& server, const Secret& master, const slice<const byte>& client_random,
                            const slice<const byte>& server_random) noexcept {
        const size_t k = key_size(c), iv = iv_size12(c);
        uint8_t block[2 * 32 + 2 * 12];
        const size_t n = 2 * k + 2 * iv;
        prf12(hash_of(c), room_of(block, n), master.view(), "key expansion", server_random, client_random);
        std::memcpy(client.bytes, block, k);
        std::memcpy(client.bytes + k, block + 2 * k, iv);
        client.size = uint8_t(k + iv);
        std::memcpy(server.bytes, block + k, k);
        std::memcpy(server.bytes + k, block + 2 * k + iv, iv);
        server.size = uint8_t(k + iv);
        crypto::detail::secure_zero(block, sizeof block);
    }

    // verify_data = PRF(master_secret, "client finished" or "server
    // finished", Hash(handshake_messages))[0..11] (§7.4.9)
    SGCL_INLINE_HOT void finished12(Hash h, uint8_t* out, const Secret& master, bool client, const slice<const byte>& transcript_hash) noexcept {
        prf12(h, room_of(out, 12), master.view(), client ? "client finished" : "server finished", transcript_hash);
    }
}
