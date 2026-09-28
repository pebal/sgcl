//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The record layer of TLS 1.3 (sgcl/net/tls/detail/record.h) on any bytes.
// The first byte picks the mode, the cipher suite and the size of the reads:
//
//   - the stream (bit 0 clear): the rest is what a peer sent, framed in
//     reads of that size, each record opened (under keys or not, with the
//     compatibility change_cipher_spec accepted or not, 0-RTT skipping or
//     not), each handshake fragment given to the assembler, until the first
//     error, which ends the connection as it would;
//   - the round trip (bit 0 set): the rest is a script of records a writer
//     seals (type, length, padding, content, a key update, a flipped
//     byte); the stream it makes goes through the same reader, and every
//     record not flipped must come back as it was sealed, under the same
//     keys, a flipped one must fail as bad_record_mac.
//
// Every fragment and message must lie within its bounds (ASan checks the
// reads) and within the limits of the RFC and the assembler.
#include "sgcl/net/tls/detail/record.h"

#include <cstring>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    const uint8_t secret_bytes[48] = {
        0xb6, 0x7b, 0x7d, 0x69, 0x0c, 0xc1, 0x6c, 0x4e, 0x75, 0xe5, 0x42, 0x13, 0xcb, 0x2d, 0x37, 0xb4,
        0xe9, 0xc9, 0x12, 0xbc, 0xde, 0xd9, 0x10, 0x5d, 0x42, 0xbe, 0xfd, 0x59, 0xd3, 0x91, 0xad, 0x38,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
    };

    tls::Cipher cipher_of(uint8_t b) {
        switch (b % 3) {
        case 0:
            return tls::Cipher::aes_128_gcm_sha256;
        case 1:
            return tls::Cipher::aes_256_gcm_sha384;
        default:
            return tls::Cipher::chacha20_poly1305_sha256;
        }
    }

    tls::Secret secret_for(tls::Cipher c) {
        tls::Secret s;
        s.size = uint8_t(tls::hash_size(tls::hash_of(c)));
        std::memcpy(s.bytes, secret_bytes, s.size);
        return s;
    }

    struct Expected {
        bool update = false;
        bool flipped = false;
        tls::ContentType type = tls::ContentType::invalid;
        std::vector<uint8_t> fragment;
    };

    // The reader: frames `stream` in reads of `step`, opens every record,
    // assembles the handshake; with `script`, compares each record with it
    void read(const uint8_t* stream, size_t size, size_t step, tls::RecordProtection& reader, const std::vector<Expected>* script) {
        tls::RecordFramer framer;
        tls::HandshakeAssembler assembler;
        std::vector<uint8_t> plain(tls::MaxCiphertext);
        size_t at = 0, index = 0, records = 0;
        bool assembling = true;
        // a key update before any record; the others follow their record
        while (script && index < script->size() && (*script)[index].update) {
            reader.update();
            ++index;
        }
        while (at < size) {
            auto room = framer.room();
            check(!room.empty());
            size_t n = std::min({step, room.size(), size - at});
            std::memcpy(room.data(), stream + at, n);
            framer.commit(n);
            at += n;
            for (;;) {
                auto r = framer.next();
                if (!r) {
                    check(!script);   // a writer's stream always frames
                    return;
                }
                if (r->empty()) {
                    break;
                }
                auto* record = reinterpret_cast<uint8_t*>(r->data());
                check(r->size() >= tls::HeaderSize && r->size() <= tls::MaxRecord);
                const bool out_of_place = (records++ & 1) != 0;
                auto o = out_of_place ? reader.open(record, r->size(), plain.data()) : reader.open(record, r->size());
                if (script) {
                    check(index < script->size() && !(*script)[index].update);
                    const Expected& e = (*script)[index++];
                    if (e.flipped) {
                        check(!o && o.error().description == tls::AlertDescription::bad_record_mac);
                        return;   // the connection is over
                    }
                    check(o.has_value());
                    check(o->type == e.type || (e.type == tls::ContentType::change_cipher_spec && o->type == tls::ContentType::invalid));
                    if (o->type != tls::ContentType::invalid) {
                        check(o->fragment.size() == e.fragment.size());
                        check(o->fragment.empty() || std::memcmp(o->fragment.data(), e.fragment.data(), e.fragment.size()) == 0);
                    }
                    while (index < script->size() && (*script)[index].update) {
                        reader.update();
                        ++index;
                    }
                }
                if (!o) {
                    return;
                }
                check(o->fragment.size() <= tls::MaxPlaintext);
                if (o->type == tls::ContentType::handshake && assembling) {
                    if (!assembler.push(o->fragment, reader.installed() ? tls::Epoch::handshake : tls::Epoch::initial)) {
                        assembling = false;
                    } else {
                        while (auto m = assembler.next()) {
                            const auto* h = reinterpret_cast<const uint8_t*>(m->data());
                            size_t length = size_t(h[1]) << 16 | size_t(h[2]) << 8 | h[3];
                            check(m->size() == 4 + length);
                            check(length <= (h[0] == 11 ? tls::HandshakeAssembler::MaxCertificate : tls::HandshakeAssembler::MaxMessage));
                        }
                    }
                }
                framer.consume();
            }
        }
        if (script) {
            check(index == script->size());
            check(framer.buffered() == 0);
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    const uint8_t mode = data[0];
    const size_t step = 1 + size_t(data[1]) * 67;
    data += 2;
    size -= 2;
    const tls::Cipher cipher = cipher_of(mode >> 1);
    const tls::Secret secret = secret_for(cipher);
    if ((mode & 1) == 0) {
        tls::RecordProtection reader;
        if (mode & 8) {
            reader.install(cipher, secret);
        }
        reader.accept_ccs((mode & 16) != 0);
        if (mode & 32) {
            reader.skip_undecryptable(size_t(mode) * 97);
        }
        read(data, size, step, reader, nullptr);
        return 0;
    }
    // the round trip
    tls::RecordProtection writer, reader;
    writer.install(cipher, secret);
    reader.install(cipher, secret);
    reader.accept_ccs(true);
    std::vector<uint8_t> stream;
    std::vector<Expected> script;
    size_t at = 0;
    while (at + 4 <= size && script.size() < 64) {
        const uint8_t c = data[at];
        size_t length = (size_t(data[at + 1]) << 8 | data[at + 2]) % (tls::MaxPlaintext + 1);
        size_t padding = data[at + 3];
        at += 4;
        if (c & 0x40) {
            writer.update();
            script.push_back({true});
            continue;
        }
        static const tls::ContentType types[] = {tls::ContentType::handshake, tls::ContentType::alert, tls::ContentType::application_data, tls::ContentType::change_cipher_spec};
        Expected e;
        e.type = types[c & 3];
        length = std::min(length, size - at);
        if (e.type == tls::ContentType::change_cipher_spec) {
            e.fragment = {1};
            padding = 0;
        } else {
            if (length == 0 && e.type != tls::ContentType::application_data) {
                length = 1;   // empty handshake and alert fragments are never sent
                if (at >= size) {
                    break;
                }
            }
            e.fragment.assign(data + at, data + at + length);
            at += length;
            if (c & 0x20) {
                padding *= 61;   // up to the RFC's limit, past it clipped
            }
            padding = std::min(padding, tls::MaxPlaintext - e.fragment.size());
        }
        size_t before = stream.size();
        stream.resize(before + writer.sealed_size(e.type, e.fragment.size(), padding));
        size_t n = writer.seal(e.type, tls::bytes_of(e.fragment.data(), e.fragment.size()), stream.data() + before, padding);
        check(n == stream.size() - before);
        if ((c & 0x80) && e.type != tls::ContentType::change_cipher_spec) {
            // a byte past the header flipped
            size_t where = tls::HeaderSize + (size_t(c) * 131 + e.fragment.size()) % (n - tls::HeaderSize);
            stream[before + where] ^= uint8_t(1 + (c & 0x0F));
            e.flipped = true;
            script.push_back(std::move(e));
            break;   // nothing after it would be read
        }
        script.push_back(std::move(e));
    }
    read(stream.data(), stream.size(), step, reader, &script);
    return 0;
}
