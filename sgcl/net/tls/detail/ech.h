//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "messages.h"
#include "schedule.h"
#include "../../../crypto/constant_time.h"
#include "../../../crypto/hpke.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// Encrypted Client Hello (RFC 9849): the wire of it. An ECHConfigList read
// and written (§4), the encrypted_client_hello extension of a
// ClientHelloOuter (§5), the EncodedClientHelloInner decoded into the
// ClientHelloInner it stands for — its legacy_session_id and its
// ech_outer_extensions taken from the outer hello (§5.1) — and the
// confirmations of acceptance a server writes in its ServerHello.random
// and its HelloRetryRequest (§7.2). The machines (handshake.h,
// server_handshake.h) do the rest.
namespace sgcl::net::tls::detail {
    inline constexpr uint16_t EchExtension = 0xfe0d;            // encrypted_client_hello, and the version of ECHConfig
    inline constexpr uint16_t EchOuterExtensions = 0xfd00;      // ech_outer_extensions
    inline constexpr uint8_t AlertEchRequired = 121;            // ech_required (§11.2)
    inline constexpr const char* EchAccept = "ech accept confirmation";
    inline constexpr const char* EchHrrAccept = "hrr ech accept confirmation";

    // One ECHConfig of version 0xfe0d (§4): its bytes whole (what HPKE's
    // info takes) and its contents
    struct EchConfig {
        std::vector<byte> raw;
        uint8_t id = 0;
        uint16_t kem = 0;
        std::vector<byte> public_key;
        std::vector<uint32_t> suites;     // kdf << 16 | aead
        uint8_t max_name_length = 0;
        std::string public_name;
    };

    // A public name of §4: a DNS name of LDH labels, not an IPv4 address
    // (its last label not all digits, as WHATWG's host parser would read it)
    inline bool ech_public_name_ok(std::string_view n) noexcept {
        if (n.empty() || n.size() > 255 || n.front() == '.' || n.back() == '.') {
            return false;
        }
        size_t label = 0;
        bool digits = true;
        for (char c : n) {
            if (c == '.') {
                if (label == 0 || label > 63) {
                    return false;
                }
                label = 0;
                digits = true;
                continue;
            }
            const bool ldh = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
            if (!ldh) {
                return false;
            }
            digits &= c >= '0' && c <= '9';
            ++label;
        }
        return label > 0 && label <= 63 && !digits;
    }

    SGCL_INLINE_HOT bool ech_kem_known(uint16_t k) noexcept {
        return k == 0x0010 || k == 0x0011 || k == 0x0012 || k == 0x0020;
    }

    SGCL_INLINE_HOT bool ech_suite_known(uint32_t s) noexcept {
        const uint16_t kdf = uint16_t(s >> 16), aead = uint16_t(s);
        return kdf >= 1 && kdf <= 3 && aead >= 1 && aead <= 3;
    }

    // An ECHConfigList (§4): every config of version 0xfe0d whose contents
    // read and that names no mandatory extension (a type with the high bit
    // set; none is known); configs of other versions passed over. nullopt
    // for a list that does not read
    inline optional<std::vector<EchConfig>> read_ech_configs(const Bytes& list) noexcept {
        Reader r(list);
        Reader configs(Bytes(), 0);
        if (!r.sub16(configs, 1, 0xFFFF) || !r.end()) {
            return nullopt;
        }
        std::vector<EchConfig> out;
        while (!configs.empty()) {
            uint16_t version;
            Bytes contents;
            const size_t before = configs.remaining();
            Reader peek = configs;
            if (!configs.u16(version) || !configs.vec16(contents, 0, 0xFFFF)) {
                return nullopt;
            }
            Bytes raw;
            if (!peek.bytes(before - configs.remaining(), raw)) {
                return nullopt;
            }
            if (version != EchExtension) {
                continue;
            }
            Reader c(contents);
            EchConfig e;
            Bytes pub, suites, name, exts;
            if (!c.u8(e.id) || !c.u16(e.kem) || !c.vec16(pub, 1, 0xFFFF) || !c.vec16(suites, 4, 0xFFFC) || !c.u8(e.max_name_length)
                || !c.vec8(name, 1, 0xFF) || !c.vec16(exts, 0, 0xFFFF) || !c.end()) {
                return nullopt;
            }
            if (suites.size() % 4) {
                return nullopt;
            }
            for (size_t i = 0; i < suites.size(); i += 4) {
                e.suites.push_back(uint32_t(uint8_t(suites[i])) << 24 | uint32_t(uint8_t(suites[i + 1])) << 16 | uint32_t(uint8_t(suites[i + 2])) << 8
                                   | uint32_t(uint8_t(suites[i + 3])));
            }
            bool mandatory = false;
            Reader x(exts);
            while (!x.empty()) {
                uint16_t type;
                Bytes body;
                if (!x.u16(type) || !x.vec16(body, 0, 0xFFFF)) {
                    return nullopt;
                }
                mandatory |= (type & 0x8000) != 0;
            }
            if (mandatory) {
                continue;
            }
            e.public_key.assign(pub.data(), pub.data() + pub.size());
            e.public_name.assign(reinterpret_cast<const char*>(name.data()), name.size());
            e.raw.assign(raw.data(), raw.data() + raw.size());
            out.push_back(std::move(e));
        }
        return out;
    }

    // The config a client offers (§6.1): the first of a KEM the module has,
    // a valid public name and a public key that reads, with the first of
    // its suites the module has
    struct EchChoice {
        EchConfig config;
        crypto::hpke::suite suite;
    };

    inline optional<EchChoice> choose_ech(const Bytes& list) noexcept {
        auto configs = read_ech_configs(list);
        if (!configs) {
            return nullopt;
        }
        for (auto& c : *configs) {
            if (!ech_kem_known(c.kem) || !ech_public_name_ok(c.public_name)) {
                continue;
            }
            const auto kem = crypto::hpke::kem(c.kem);
            if (!crypto::hpke::public_key::from_bytes(kem, bytes_of(c.public_key.data(), c.public_key.size()))) {
                continue;
            }
            for (uint32_t s : c.suites) {
                if (ech_suite_known(s)) {
                    EchChoice choice;
                    choice.suite = crypto::hpke::suite{crypto::hpke::kdf(s >> 16), crypto::hpke::aead(uint16_t(s))};
                    choice.config = std::move(c);
                    return choice;
                }
            }
        }
        return nullopt;
    }

    // An ECHConfig of version 0xfe0d written whole
    inline std::vector<byte> write_ech_config(uint8_t id, uint16_t kem, const Bytes& public_key, const std::vector<uint32_t>& suites, uint8_t max_name_length,
                                              std::string_view public_name) noexcept {
        std::vector<byte> out;
        Builder w(out);
        w.u16(EchExtension);
        auto contents = w.block16();
        w.u8(id);
        w.u16(kem);
        {
            auto p = w.block16();
            w.bytes(public_key);
        }
        {
            auto s = w.block16();
            for (uint32_t x : suites) {
                w.u16(uint16_t(x >> 16));
                w.u16(uint16_t(x));
            }
        }
        w.u8(max_name_length);
        {
            auto n = w.block8();
            w.bytes(public_name.data(), public_name.size());
        }
        {
            auto x = w.block16();
        }
        return out;
    }

    // HPKE's info (§6.1): "tls ech" || 0x00 || ECHConfig
    inline std::vector<byte> ech_info(const std::vector<byte>& config) noexcept {
        static constexpr char prefix[] = "tls ech";
        std::vector<byte> info(reinterpret_cast<const byte*>(prefix), reinterpret_cast<const byte*>(prefix) + 8);   // the NUL included
        info.insert(info.end(), config.begin(), config.end());
        return info;
    }

    // The encrypted_client_hello extension of a ClientHelloOuter (§5): the
    // suite, the config id, enc (empty in a second hello) and the payload;
    // payload_at is the payload's offset in the extension's body
    struct EchOuter {
        uint16_t kdf = 0;
        uint16_t aead = 0;
        uint8_t config_id = 0;
        Bytes enc;
        Bytes payload;
        size_t payload_at = 0;
    };

    // The type of an encrypted_client_hello body: 0 outer, 1 inner; nullopt
    // for an empty body
    SGCL_INLINE_HOT optional<uint8_t> ech_type(const Bytes& body) noexcept {
        if (body.empty()) {
            return nullopt;
        }
        return uint8_t(body[0]);
    }

    inline expected<EchOuter, Alert> read_ech_outer(const Bytes& body) noexcept {
        Reader r(body);
        uint8_t type;
        EchOuter e;
        if (!r.u8(type) || type != 0 || !r.u16(e.kdf) || !r.u16(e.aead) || !r.u8(e.config_id) || !r.vec16(e.enc, 0, 0xFFFF)) {
            return failed(AlertDescription::decode_error, 0, "an encrypted_client_hello extension that does not read");
        }
        e.payload_at = body.size() - r.remaining() + 2;
        if (!r.vec16(e.payload, 1, 0xFFFF) || !r.end()) {
            return failed(AlertDescription::decode_error, 0, "an encrypted_client_hello extension that does not read");
        }
        return e;
    }

    // The ClientHelloInner of an EncodedClientHelloInner (§5.1), as a whole
    // handshake message: legacy_session_id the outer hello's, every type of
    // ech_outer_extensions replaced by the outer hello's extension of that
    // type (each there, in the outer hello's order, never
    // encrypted_client_hello), the padding all zeros. illegal_parameter for
    // anything else, as §7.1 asks
    inline expected<std::vector<byte>, Alert> decode_inner(const Bytes& encoded, const ClientHello& outer) noexcept {
        auto bad = [](const char* what) {
            return unexpected(Alert{AlertDescription::illegal_parameter, 0, what});
        };
        Reader r(encoded);
        uint16_t legacy_version;
        Bytes random, session_id, suites, compression;
        Reader exts(Bytes(), 0);
        if (!r.u16(legacy_version) || !r.bytes(32, random) || !r.vec8(session_id, 0, 32) || !r.vec16(suites, 2, 0xFFFE) || !r.vec8(compression, 1, 0xFF)
            || !r.sub16(exts, 8, 0xFFFF)) {
            return bad("an EncodedClientHelloInner that does not read");
        }
        if (!session_id.empty()) {
            return bad("an EncodedClientHelloInner with a legacy_session_id");
        }
        Bytes padding;
        if (!r.bytes(r.remaining(), padding)) {
            return bad("an EncodedClientHelloInner that does not read");
        }
        uint8_t nonzero = 0;
        for (auto b : padding) {
            nonzero |= uint8_t(b);
        }
        if (nonzero) {
            return bad("an EncodedClientHelloInner padded with other bytes than zeros");
        }
        std::vector<byte> out;
        Builder w(out);
        {
            auto m = w.message(HandshakeType::client_hello);
            w.u16(legacy_version);
            w.bytes(random);
            {
                auto s = w.block8(32);
                w.bytes(outer.session_id);
            }
            {
                auto s = w.block16();
                w.bytes(suites);
            }
            {
                auto c = w.block8();
                w.bytes(compression);
            }
            auto list = w.block16();
            bool inner_seen = false;
            auto next_outer = outer.extensions.begin();
            while (!exts.empty()) {
                uint16_t type;
                Bytes body;
                if (!exts.u16(type) || !exts.vec16(body, 0, 0xFFFF)) {
                    return bad("an EncodedClientHelloInner that does not read");
                }
                if (type == EchExtension) {
                    if (body.size() != 1 || uint8_t(body[0]) != 1) {
                        return bad("a ClientHelloInner whose encrypted_client_hello is not of type inner");
                    }
                    inner_seen = true;
                }
                if (type != EchOuterExtensions) {
                    w.u16(type);
                    auto b = w.block16();
                    w.bytes(body);
                    continue;
                }
                Reader refs(body);
                Reader types(Bytes(), 0);
                if (!refs.sub8(types, 2, 254) || !refs.end() || types.remaining() % 2) {
                    return bad("an ech_outer_extensions that does not read");
                }
                while (!types.empty()) {
                    uint16_t t;
                    types.u16(t);
                    if (t == EchExtension) {
                        return bad("ech_outer_extensions names encrypted_client_hello");
                    }
                    bool found = false;
                    while (next_outer != outer.extensions.end()) {
                        auto e = *next_outer;
                        ++next_outer;
                        if (e.type == t) {
                            w.u16(t);
                            auto b = w.block16();
                            w.bytes(e.body);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        return bad("ech_outer_extensions names an extension the outer hello does not have, or out of its order");
                    }
                }
            }
            if (!inner_seen) {
                return bad("a ClientHelloInner without encrypted_client_hello of type inner");
            }
        }
        return out;
    }

    // The 8 bytes of a confirmation (§7.2, §7.2.1): HKDF-Expand-Label of
    // HKDF-Extract(0, the inner random) over the transcript so far and the
    // message, its confirmation bytes zeroed
    inline void ech_confirmation(Hash h, uint8_t* out, const uint8_t* inner_random, Transcript t, const Bytes& message_zeroed, const char* label) noexcept {
        t.update(message_zeroed);
        uint8_t th[MaxHashSize];
        t.value_to(th);
        static constexpr uint8_t zeros[MaxHashSize] = {};
        Secret prk;
        extract(h, prk, bytes_of(zeros, hash_size(h)), bytes_of(inner_random, 32));
        expand_label(h, room_of(out, 8), prk.view(), label, bytes_of(th, hash_size(h)));
    }

    // The padding of an EncodedClientHelloInner (§6.1.3): the server name
    // padded to max_name_length (or nine bytes more without one), the whole
    // to a multiple of 32
    SGCL_INLINE_HOT size_t ech_padding(size_t encoded, size_t server_name, uint8_t max_name_length) noexcept {
        size_t pad = server_name ? (server_name < max_name_length ? max_name_length - server_name : 0) : size_t(max_name_length) + 9;
        const size_t total = encoded + pad;
        return pad + (31 - (total + 31) % 32);
    }
}
