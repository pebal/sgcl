//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The record layer of TLS 1.3 (sgcl/net/tls/detail/record.h) against two
// oracles and its own edges:
//
//   - RFC 8448 (~/Programming/oracles/rfc8448/rfc8448.txt or
//     $SGCL_ORACLES), every record of §3–§7: each payload sealed into the
//     trace's complete record byte for byte and each complete record
//     opened back to its payload, the keys of each record found by
//     opening (0-RTT, handshake, application), the compatibility
//     change_cipher_spec records of §7 included;
//   - real sessions of OpenSSL with itself (tls_record_vectors.h,
//     tools/tls_record_oracle.cpp), the three cipher suites, with and
//     without padding: each side's bytes framed in chunks of every size,
//     every record opened, its handshake messages assembled and compared
//     with the ones OpenSSL wrote, the keys changed where the messages say
//     (ServerHello, Finished, KeyUpdate), the application data compared,
//     and every record sealed again from its content into the same bytes;
//   - the limits, the nonce, padding, the errors, the modes, the framer
//     and the assembler at their edges.
#include "tests/types.h"

#include "sgcl/net/tls/detail/record.h"
#include "tls_record_vectors.h"
#include "tls_rfc8448.h"

#include <string>
#include <vector>

namespace tls = sgcl::net::tls::detail;

namespace {
    using bytes_t = std::vector<uint8_t>;
    using tls::AlertDescription;
    using tls::ContentType;
    using tls::Epoch;

    bytes_t unhex(const std::string& s) {
        bytes_t v;
        for (size_t i = 0; i + 1 < s.size(); i += 2) {
            v.push_back(uint8_t(std::stoi(s.substr(i, 2), nullptr, 16)));
        }
        return v;
    }

    sgcl::slice<const sgcl::byte> view(const bytes_t& v) {
        return sgcl::slice<const sgcl::byte>(reinterpret_cast<const sgcl::byte*>(v.data()), v.size());
    }

    bytes_t of(const sgcl::slice<const sgcl::byte>& s) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        return bytes_t(p, p + s.size());
    }

    tls::Secret secret_of(const bytes_t& b) {
        tls::Secret s;
        std::memcpy(s.bytes, b.data(), b.size());
        s.size = uint8_t(b.size());
        return s;
    }

    tls::Cipher cipher_of(const std::string& suite) {
        if (suite == "TLS_AES_256_GCM_SHA384") {
            return tls::Cipher::aes_256_gcm_sha384;
        }
        if (suite == "TLS_CHACHA20_POLY1305_SHA256") {
            return tls::Cipher::chacha20_poly1305_sha256;
        }
        return tls::Cipher::aes_128_gcm_sha256;
    }

    bytes_t seal(tls::RecordProtection& p, ContentType type, const bytes_t& fragment, size_t padding = 0) {
        bytes_t out(p.sealed_size(type, fragment.size(), padding));
        size_t n = p.seal(type, view(fragment), out.data(), padding);
        EXPECT_EQ(n, out.size());
        return out;
    }

    // A protected record built by hand (any header type, any inner bytes)
    // under the keys of a secret at a sequence number: what seal() refuses
    // to make
    bytes_t raw_record(const tls::Secret& secret, uint64_t sequence, uint8_t header_type, const bytes_t& inner) {
        tls::TrafficKeys keys;
        tls::traffic_keys(tls::Hash::sha256, keys, secret, 16);
        uint8_t nonce[12];
        std::memcpy(nonce, keys.iv, 12);
        for (int i = 0; i < 8; ++i) {
            nonce[11 - i] ^= uint8_t(sequence >> (8 * i));
        }
        size_t length = inner.size() + 16;
        bytes_t out(5 + length);
        out[0] = header_type;
        out[1] = 3;
        out[2] = 3;
        out[3] = uint8_t(length >> 8);
        out[4] = uint8_t(length);
        crypto::aes_gcm aead(tls::bytes_of(keys.key, 16));
        aead.seal_to(tls::room_of(out.data() + 5, length), tls::bytes_of(nonce, 12), view(inner), tls::bytes_of(out.data(), 5));
        return out;
    }

    AlertDescription error_of_open(tls::RecordProtection& p, bytes_t record) {
        auto r = p.open(record.data(), record.size());
        EXPECT_FALSE(r.has_value());
        return r.has_value() ? AlertDescription::close_notify : r.error().description;
    }

    ContentType content_type_of(const std::string& title) {
        if (title.find("application_data") != std::string::npos) {
            return ContentType::application_data;
        }
        if (title.find("alert") != std::string::npos) {
            return ContentType::alert;
        }
        if (title.find("change_cipher_spec") != std::string::npos) {
            return ContentType::change_cipher_spec;
        }
        return ContentType::handshake;
    }

    const bytes_t test_secret = unhex("b67b7d690cc16c4e75e54213cb2d37b4e9c912bcded9105d42befd59d391ad38");
}

// --- RFC 8448 ---------------------------------------------------------------

// Every record of every trace: sealed into its bytes, opened back
TEST(TlsRecord, Rfc8448EveryRecord) {
    auto steps = rfc8448::read();
    if (steps.empty()) {
        GTEST_SKIP() << "rfc8448.txt not on disk";
    }
    size_t total = 0;
    for (int section = 3; section <= 7; ++section) {
        // the traffic secrets of the section in the order each side uses them
        // (the step that computes it: the other side's says "same as")
        std::vector<bytes_t> keys[2];   // [0] the client's, [1] the server's
        auto derived = [&](int side, const char* label) {
            std::string title = std::string("derive secret \"tls13 ") + label + "\"";
            for (auto& s : steps) {
                if (s.section == section && s.title.rfind(title, 0) == 0 && s.fields.count("expanded")) {
                    keys[side].push_back(s.fields.at("expanded"));
                    return;
                }
            }
        };
        for (const char* label : {"c e traffic", "c hs traffic", "c ap traffic"}) {
            derived(0, label);
        }
        for (const char* label : {"s hs traffic", "s ap traffic"}) {
            derived(1, label);
        }
        ASSERT_GE(keys[0].size(), 2u) << section;
        ASSERT_EQ(keys[1].size(), 2u) << section;
        tls::RecordProtection writer[2], reader[2];
        int epoch[2] = {-1, -1};
        reader[0].accept_ccs(true);
        reader[1].accept_ccs(true);
        size_t records = 0;
        for (auto& s : steps) {
            if (s.section != section || s.title.rfind("send ", 0) != 0 || !s.fields.count("complete record")) {
                continue;
            }
            const int side = s.who == "client" ? 0 : 1;
            const ContentType type = content_type_of(s.title);
            const bytes_t& payload = s.fields.at("payload");
            const bytes_t& complete = s.fields.at("complete record");
            if (complete[0] == 0x17) {
                // the keys: the current ones, or the next that open it
                bool opened = false;
                for (int e = epoch[side] < 0 ? 0 : epoch[side]; e < int(keys[side].size()) && !opened; ++e) {
                    if (e != epoch[side]) {
                        reader[side].install(tls::Cipher::aes_128_gcm_sha256, secret_of(keys[side][e]));
                    }
                    bytes_t copy = complete;
                    auto r = reader[side].open(copy.data(), copy.size());
                    if (r) {
                        EXPECT_EQ(r->type, type) << section << " " << s.who << " " << s.title;
                        EXPECT_EQ(of(r->fragment), payload) << section << " " << s.who << " " << s.title;
                        if (e != epoch[side]) {
                            writer[side].install(tls::Cipher::aes_128_gcm_sha256, secret_of(keys[side][e]));
                            epoch[side] = e;
                        }
                        opened = true;
                    } else {
                        EXPECT_EQ(r.error().description, AlertDescription::bad_record_mac);
                    }
                }
                ASSERT_TRUE(opened) << section << " " << s.who << " " << s.title;
            } else {
                bytes_t copy = complete;
                auto r = reader[side].open(copy.data(), copy.size());
                ASSERT_TRUE(r.has_value()) << section << " " << s.who << " " << s.title;
                if (type == ContentType::change_cipher_spec) {
                    EXPECT_EQ(r->type, ContentType::invalid);   // dropped
                } else {
                    EXPECT_EQ(r->type, type);
                    EXPECT_EQ(of(r->fragment), payload);
                }
            }
            EXPECT_EQ(seal(writer[side], type, payload), complete) << section << " " << s.who << " " << s.title;
            ++records;
        }
        EXPECT_GT(records, 0u) << section;
        total += records;
    }
    EXPECT_EQ(total, 41u);   // §3 9, §4 10, §5 8, §6 6, §7 8
}

// --- OpenSSL sessions -------------------------------------------------------

namespace {
    // One direction of a session as its reader sees it, with a writer that
    // mirrors every change of keys and seals each record again
    struct Direction {
        tls::Cipher cipher;
        const tls::Secret* handshake_secret;
        const tls::Secret* application_secret;
        tls::RecordFramer framer;
        tls::RecordProtection reader, writer;
        tls::HandshakeAssembler assembler;
        Epoch epoch = Epoch::initial;
        std::vector<bytes_t> messages;
        bytes_t data;
        std::vector<bytes_t> alerts;
        size_t records = 0, resealed = 0, key_updates = 0;

        void install(Epoch e, const tls::Secret& secret) {
            reader.install(cipher, secret);
            writer.install(cipher, secret);
            epoch = e;
            auto k = assembler.on_key_change();
            EXPECT_TRUE(k.has_value());
        }

        // A handshake message whole: the keys it changes (§7: the reader
        // of the client's side installs them after the ClientHello, the
        // one of the server's after the ServerHello; the Finished moves to
        // the application's; a KeyUpdate to the next)
        void on_message(const bytes_t& m) {
            messages.push_back(m);
            switch (tls::HandshakeType(m[0])) {
            case tls::HandshakeType::client_hello:
            case tls::HandshakeType::server_hello:
                install(Epoch::handshake, *handshake_secret);
                break;
            case tls::HandshakeType::finished:
                install(Epoch::application, *application_secret);
                break;
            case tls::HandshakeType::key_update:
                reader.update();
                writer.update();
                ++key_updates;
                {
                    auto k = assembler.on_key_change();
                    EXPECT_TRUE(k.has_value());
                }
                break;
            default:
                break;
            }
        }

        void on_record(uint8_t* record, size_t size) {
            ++records;
            bytes_t wire(record, record + size);
            // application data out of place, the rest in place
            bytes_t plain(tls::MaxCiphertext);
            auto r = record[0] == 0x17 && epoch == Epoch::application ? reader.open(record, size, plain.data()) : reader.open(record, size);
            ASSERT_TRUE(r.has_value()) << "record " << records << ": " << (r ? "" : r.error().what);
            bytes_t fragment = of(r->fragment);
            if (r->type == ContentType::invalid) {
                EXPECT_EQ(wire, (bytes_t{0x14, 0x03, 0x03, 0x00, 0x01, 0x01}));
                EXPECT_EQ(seal(writer, ContentType::change_cipher_spec, {1}), wire);
                ++resealed;
                return;
            }
            // the same record again from its content and its padding
            size_t padding = wire[0] == 0x17 ? size - 5 - 16 - 1 - fragment.size() : 0;
            EXPECT_EQ(seal(writer, r->type, fragment, padding), wire) << "record " << records;
            ++resealed;
            if (r->type == ContentType::handshake) {
                auto p = assembler.push(r->fragment, epoch);
                ASSERT_TRUE(p.has_value());
                while (auto m = assembler.next()) {
                    on_message(of(*m));
                }
            } else if (r->type == ContentType::application_data) {
                data.insert(data.end(), fragment.begin(), fragment.end());
            } else {
                alerts.push_back(fragment);
            }
        }

        // The bytes of the wire through the framer in chunks of the sizes given
        void run(const bytes_t& wire, size_t chunk) {
            size_t at = 0;
            while (at < wire.size()) {
                auto room = framer.room();
                ASSERT_FALSE(room.empty());
                size_t n = std::min({chunk, room.size(), wire.size() - at});
                std::memcpy(room.data(), wire.data() + at, n);
                framer.commit(n);
                at += n;
                for (;;) {
                    auto r = framer.next();
                    ASSERT_TRUE(r.has_value());
                    if (r->empty()) {
                        break;
                    }
                    on_record(reinterpret_cast<uint8_t*>(r->data()), r->size());
                    framer.consume();
                }
                chunk = chunk * 5 % 7919 + 1;   // every size in turn
            }
            EXPECT_EQ(framer.buffered(), 0u);
            EXPECT_TRUE(assembler.empty());
        }
    };
}

TEST(TlsRecord, OpenSslSessions) {
    ASSERT_EQ(tls_record_vectors::sessions.size(), 5u);
    for (auto& s : tls_record_vectors::sessions) {
        SCOPED_TRACE(std::string(s.suite) + " padding " + std::to_string(s.padding));
        const tls::Cipher cipher = cipher_of(s.suite);
        tls::Secret chs = secret_of(unhex(s.client_hs)), shs = secret_of(unhex(s.server_hs));
        tls::Secret cap = secret_of(unhex(s.client_ap)), sap = secret_of(unhex(s.server_ap));
        bytes_t expected_messages[2];
        std::vector<bytes_t> sent[2];
        for (auto& m : s.messages) {
            sent[m.client ? 0 : 1].push_back(unhex(m.bytes));
        }
        for (size_t chunk : {size_t(1), size_t(5), size_t(16389), size_t(3000)}) {
            Direction client{cipher, &chs, &cap}, server{cipher, &shs, &sap};
            client.reader.accept_ccs(true);
            server.reader.accept_ccs(true);
            client.run(unhex(s.client_wire), chunk);
            server.run(unhex(s.server_wire), chunk);
            EXPECT_EQ(client.messages, sent[0]);
            EXPECT_EQ(server.messages, sent[1]);
            EXPECT_EQ(client.resealed, client.records);
            EXPECT_EQ(server.resealed, server.records);
            EXPECT_EQ(server.key_updates, 1u);
            EXPECT_EQ(client.key_updates, 1u);
            std::string cdata(client.data.begin(), client.data.end());
            EXPECT_EQ(cdata, "helloclient after update");
            bytes_t sdata;
            for (size_t i = 0; i < 17000; ++i) {
                sdata.push_back(uint8_t(i * 7 + 3));
            }
            for (char c : std::string("after update")) {
                sdata.push_back(uint8_t(c));
            }
            EXPECT_EQ(server.data, sdata);
            ASSERT_EQ(client.alerts.size(), 1u);
            EXPECT_EQ(client.alerts[0], (bytes_t{1, 0}));   // close_notify
            EXPECT_TRUE(server.alerts.empty());
        }
    }
}

// --- the edges --------------------------------------------------------------

// §5.3: the nonce is the IV XOR the sequence number; every install and
// update starts it at zero
TEST(TlsRecord, NonceAndSequence) {
    tls::Secret secret = secret_of(test_secret);
    tls::RecordProtection w, r;
    w.install(tls::Cipher::aes_128_gcm_sha256, secret);
    r.install(tls::Cipher::aes_128_gcm_sha256, secret);
    bytes_t data = {1, 2, 3};
    for (uint64_t i = 0; i < 300; ++i) {
        bytes_t record = seal(w, ContentType::application_data, data);
        bytes_t inner = data;
        inner.push_back(23);
        EXPECT_EQ(record, raw_record(secret, i, 23, inner)) << i;
        auto o = r.open(record.data(), record.size());
        ASSERT_TRUE(o.has_value());
        EXPECT_EQ(of(o->fragment), data);
    }
    EXPECT_EQ(w.sequence(), 300u);
    EXPECT_EQ(r.sequence(), 300u);
    EXPECT_FALSE(w.needs_update());
    // out of order: the sequence number is part of the nonce
    bytes_t first = seal(w, ContentType::application_data, data);
    bytes_t second = seal(w, ContentType::application_data, data);
    EXPECT_EQ(error_of_open(r, second), AlertDescription::bad_record_mac);
    EXPECT_EQ(r.sequence(), 300u);   // a failed open takes no number
    EXPECT_TRUE(r.open(first.data(), first.size()).has_value());
    EXPECT_TRUE(r.open(second.data(), second.size()).has_value());
    // a new install starts again at zero
    w.install(tls::Cipher::aes_128_gcm_sha256, secret);
    EXPECT_EQ(w.sequence(), 0u);
    bytes_t inner = data;
    inner.push_back(23);
    EXPECT_EQ(seal(w, ContentType::application_data, data), raw_record(secret, 0, 23, inner));
}

// §7.2: after update() both sides use the next traffic secret from zero
TEST(TlsRecord, KeyUpdate) {
    for (auto cipher : {tls::Cipher::aes_128_gcm_sha256, tls::Cipher::aes_256_gcm_sha384, tls::Cipher::chacha20_poly1305_sha256}) {
        bytes_t raw = test_secret;
        if (tls::hash_of(cipher) == tls::Hash::sha384) {
            raw.insert(raw.end(), test_secret.begin(), test_secret.begin() + 16);
        }
        tls::Secret secret = secret_of(raw);
        tls::RecordProtection w, r, stale;
        w.install(cipher, secret);
        r.install(cipher, secret);
        stale.install(cipher, secret);
        bytes_t data = {9, 8, 7, 6};
        for (int i = 0; i < 3; ++i) {
            bytes_t rec = seal(w, ContentType::application_data, data);
            bytes_t rec2 = rec;
            ASSERT_TRUE(r.open(rec.data(), rec.size()).has_value());
            ASSERT_TRUE(stale.open(rec2.data(), rec2.size()).has_value());
        }
        w.update();
        r.update();
        EXPECT_EQ(w.sequence(), 0u);
        bytes_t rec = seal(w, ContentType::application_data, data);
        EXPECT_EQ(error_of_open(stale, rec), AlertDescription::bad_record_mac);
        // the same as installing the updated secret
        tls::Secret next = secret_of(raw);
        tls::update_traffic_secret(tls::hash_of(cipher), next);
        tls::RecordProtection fresh;
        fresh.install(cipher, next);
        bytes_t copy = rec;
        auto o = fresh.open(copy.data(), copy.size());
        ASSERT_TRUE(o.has_value());
        EXPECT_EQ(of(o->fragment), data);
        auto o2 = r.open(rec.data(), rec.size());
        ASSERT_TRUE(o2.has_value());
        EXPECT_EQ(of(o2->fragment), data);
    }
    tls::RecordProtection none;
    EXPECT_THROW(none.update(), std::logic_error);
}

// §5.4: padding is zeros after the inner type; content and padding at most
// 2^14 + 1
TEST(TlsRecord, Padding) {
    tls::Secret secret = secret_of(test_secret);
    tls::RecordProtection w, r;
    w.install(tls::Cipher::aes_128_gcm_sha256, secret);
    r.install(tls::Cipher::aes_128_gcm_sha256, secret);
    bytes_t data(100, 0);   // zeros in the content itself are content
    for (size_t padding : {size_t(0), size_t(1), size_t(255), size_t(16284)}) {
        bytes_t rec = seal(w, ContentType::handshake, data, padding);
        EXPECT_EQ(rec.size(), 5 + 100 + 1 + padding + 16);
        auto o = r.open(rec.data(), rec.size());
        ASSERT_TRUE(o.has_value()) << padding;
        EXPECT_EQ(o->type, ContentType::handshake);
        EXPECT_EQ(of(o->fragment), data);
    }
    bytes_t out(tls::MaxRecord + 1);
    EXPECT_THROW(w.seal(ContentType::handshake, view(data), out.data(), 16285), std::length_error);
    bytes_t full(tls::MaxPlaintext, 5);
    EXPECT_EQ(w.seal(ContentType::application_data, view(full), out.data()), 5 + tls::MaxPlaintext + 1 + 16);
    EXPECT_THROW(w.seal(ContentType::application_data, view(full), out.data(), 1), std::length_error);
    bytes_t over(tls::MaxPlaintext + 1, 5);
    EXPECT_THROW(w.seal(ContentType::application_data, view(over), out.data()), std::length_error);
    // plaintext records take no padding
    tls::RecordProtection plain;
    EXPECT_THROW(plain.seal(ContentType::handshake, view(data), out.data(), 1), std::logic_error);
}

// A fragment already in place at out + HeaderSize is sealed where it is;
// open into a separate buffer leaves the record's text alone
TEST(TlsRecord, InPlaceAndOutOfPlace) {
    tls::Secret secret = secret_of(test_secret);
    tls::RecordProtection w, r, w2;
    w.install(tls::Cipher::chacha20_poly1305_sha256, secret);
    r.install(tls::Cipher::chacha20_poly1305_sha256, secret);
    w2.install(tls::Cipher::chacha20_poly1305_sha256, secret);
    bytes_t data = {10, 20, 30, 40, 50};
    bytes_t buf(64);
    std::memcpy(buf.data() + 5, data.data(), data.size());
    size_t n = w.seal(ContentType::application_data, tls::bytes_of(buf.data() + 5, data.size()), buf.data());
    buf.resize(n);
    EXPECT_EQ(buf, seal(w2, ContentType::application_data, data));
    bytes_t out(n);
    bytes_t before = buf;
    auto o = r.open(buf.data(), buf.size(), out.data());
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(o->fragment.data()), out.data());
    EXPECT_EQ(of(o->fragment), data);
    EXPECT_EQ(buf, before);
}

// The errors of a protected record
TEST(TlsRecord, ProtectedRecordErrors) {
    tls::Secret secret = secret_of(test_secret);
    auto fresh = [&] {
        auto p = std::make_unique<tls::RecordProtection>();
        p->install(tls::Cipher::aes_128_gcm_sha256, secret);
        return p;
    };
    bytes_t data = {1, 2, 3};
    bytes_t good = raw_record(secret, 0, 23, {1, 2, 3, 22});
    // a byte of the ciphertext, of the tag, of the header (the AAD)
    for (size_t at : {size_t(5), good.size() - 1, size_t(1), size_t(2)}) {
        bytes_t bad = good;
        bad[at] ^= 1;
        EXPECT_EQ(error_of_open(*fresh(), bad), AlertDescription::bad_record_mac) << at;
    }
    // under other keys
    tls::Secret other = secret_of(unhex(std::string(64, '1')));
    EXPECT_EQ(error_of_open(*fresh(), raw_record(other, 0, 23, {1, 22})), AlertDescription::bad_record_mac);
    // shorter than the tag and the type
    bytes_t tiny = {23, 3, 3, 0, 16};
    tiny.resize(5 + 16, 0);
    EXPECT_EQ(error_of_open(*fresh(), tiny), AlertDescription::bad_record_mac);
    // no inner type: all zeros
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, bytes_t(20, 0))), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, {})), AlertDescription::bad_record_mac);   // 16 bytes: < tag + 1
    // inner types that may not be protected, or are unknown
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, {1, 20})), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, {1, 99})), AlertDescription::unexpected_message);
    // empty handshake and alert fragments; empty application data is fine
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, {22, 0, 0})), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, {21})), AlertDescription::unexpected_message);
    {
        auto p = fresh();
        bytes_t rec = raw_record(secret, 0, 23, {23, 0, 0, 0});
        auto o = p->open(rec.data(), rec.size());
        ASSERT_TRUE(o.has_value());
        EXPECT_EQ(o->type, ContentType::application_data);
        EXPECT_TRUE(o->fragment.empty());
    }
    // content over 2^14 + 1 with a good tag
    bytes_t big(tls::MaxPlaintext + 2, 0);
    big[0] = 1;
    big[tls::MaxPlaintext] = 23;   // the type, then one byte of padding too many
    EXPECT_EQ(error_of_open(*fresh(), raw_record(secret, 0, 23, big)), AlertDescription::record_overflow);
    big.pop_back();
    EXPECT_TRUE(fresh()->open(raw_record(secret, 0, 23, big).data(), 5 + big.size() + 16).has_value());
    // a ciphertext over 2^14 + 256
    bytes_t huge = {23, 3, 3, uint8_t((tls::MaxCiphertext + 1) >> 8), uint8_t(tls::MaxCiphertext + 1)};
    huge.resize(5 + tls::MaxCiphertext + 1);
    EXPECT_EQ(error_of_open(*fresh(), huge), AlertDescription::record_overflow);
    // a plaintext record after the keys
    bytes_t plain = {22, 3, 3, 0, 4, 1, 0, 0, 0};
    EXPECT_EQ(error_of_open(*fresh(), plain), AlertDescription::unexpected_message);
    // the header's length not the record's
    bytes_t mismatch = good;
    mismatch.push_back(0);
    EXPECT_EQ(error_of_open(*fresh(), mismatch), AlertDescription::decode_error);
}

// The errors of a plaintext record before the keys
TEST(TlsRecord, PlaintextRecordErrors) {
    tls::RecordProtection p;
    EXPECT_EQ(error_of_open(p, {23, 3, 3, 0, 1, 7}), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(p, {22, 3, 3, 0, 0}), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(p, {21, 3, 3, 0, 0}), AlertDescription::unexpected_message);
    bytes_t over = {22, 3, 1, uint8_t((tls::MaxPlaintext + 1) >> 8), uint8_t(tls::MaxPlaintext + 1)};
    over.resize(5 + tls::MaxPlaintext + 1, 1);
    EXPECT_EQ(error_of_open(p, over), AlertDescription::record_overflow);
    bytes_t max = over;
    max[3] = uint8_t(tls::MaxPlaintext >> 8);
    max[4] = uint8_t(tls::MaxPlaintext);
    max.pop_back();
    auto o = p.open(max.data(), max.size());
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->fragment.size(), tls::MaxPlaintext);
    // the version of a record is ignored (§5.1)
    bytes_t odd = {22, 9, 9, 0, 1, 2};
    EXPECT_TRUE(p.open(odd.data(), odd.size()).has_value());
    // out of place
    bytes_t rec = {21, 3, 3, 0, 2, 2, 40};
    bytes_t out(2);
    auto a = p.open(rec.data(), rec.size(), out.data());
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->type, ContentType::alert);
    EXPECT_EQ(out, (bytes_t{2, 40}));
}

// §5, §D.4: the compatibility change_cipher_spec
TEST(TlsRecord, ChangeCipherSpec) {
    tls::Secret secret = secret_of(test_secret);
    tls::RecordProtection r;
    bytes_t ccs = {20, 3, 3, 0, 1, 1};
    EXPECT_EQ(error_of_open(r, ccs), AlertDescription::unexpected_message);   // before the ClientHello
    r.accept_ccs(true);
    auto o = r.open(bytes_t(ccs).data(), ccs.size());
    ASSERT_TRUE(o.has_value());
    EXPECT_EQ(o->type, ContentType::invalid);
    EXPECT_EQ(error_of_open(r, {20, 3, 3, 0, 1, 2}), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(r, {20, 3, 3, 0, 2, 1, 1}), AlertDescription::unexpected_message);
    EXPECT_EQ(error_of_open(r, {20, 3, 3, 0, 0}), AlertDescription::unexpected_message);
    r.install(tls::Cipher::aes_128_gcm_sha256, secret);
    bytes_t again = ccs;
    EXPECT_TRUE(r.open(again.data(), again.size()).has_value());   // under the keys too
    EXPECT_EQ(r.sequence(), 0u);
    r.accept_ccs(false);                                           // after the peer's Finished
    EXPECT_EQ(error_of_open(r, ccs), AlertDescription::unexpected_message);
    // sealed as the plaintext byte 1 whatever the keys
    tls::RecordProtection w;
    w.install(tls::Cipher::aes_128_gcm_sha256, secret);
    EXPECT_EQ(seal(w, ContentType::change_cipher_spec, {1}), ccs);
    EXPECT_EQ(w.sequence(), 0u);
}

// §5.1: the records of the initial ClientHello carry 0x0301, a second
// ClientHello and everything else 0x0303
TEST(TlsRecord, LegacyRecordVersion) {
    tls::RecordProtection w;
    bytes_t hello(4 + 20000, 7);
    hello[0] = 1;
    hello[1] = 0;
    hello[2] = uint8_t(20000 >> 8);
    hello[3] = uint8_t(20000 & 0xFF);
    bytes_t first(hello.begin(), hello.begin() + tls::MaxPlaintext), rest(hello.begin() + tls::MaxPlaintext, hello.end());
    EXPECT_EQ(seal(w, ContentType::handshake, first)[2], 0x01);
    EXPECT_EQ(seal(w, ContentType::handshake, rest)[2], 0x01);
    bytes_t second = {1, 0, 0, 1, 9};
    EXPECT_EQ(seal(w, ContentType::handshake, second)[2], 0x03);
    tls::RecordProtection server;
    EXPECT_EQ(seal(server, ContentType::handshake, {2, 0, 0, 1, 9})[2], 0x03);
    EXPECT_EQ(seal(server, ContentType::alert, {2, 40})[2], 0x03);
}

// §4.2.10: after a refused 0-RTT, records that do not open are dropped up
// to the limit; the first record that opens ends the mode
TEST(TlsRecord, SkipUndecryptable) {
    tls::Secret secret = secret_of(test_secret);
    tls::Secret early = secret_of(unhex(std::string(64, '2')));
    bytes_t e1 = raw_record(early, 0, 23, bytes_t(30, 1)), e2 = raw_record(early, 1, 23, bytes_t(30, 1));
    {
        tls::RecordProtection r;
        r.install(tls::Cipher::aes_128_gcm_sha256, secret);
        r.skip_undecryptable(2 * (30 + 16));
        auto a = r.open(bytes_t(e1).data(), e1.size());
        ASSERT_TRUE(a.has_value());
        EXPECT_EQ(a->type, ContentType::invalid);
        auto b = r.open(bytes_t(e2).data(), e2.size());
        ASSERT_TRUE(b.has_value());
        EXPECT_EQ(b->type, ContentType::invalid);
        EXPECT_EQ(error_of_open(r, e1), AlertDescription::bad_record_mac);   // over the limit
    }
    {
        tls::RecordProtection r;
        r.install(tls::Cipher::aes_128_gcm_sha256, secret);
        r.skip_undecryptable(1000);
        EXPECT_TRUE(r.open(bytes_t(e1).data(), e1.size()).has_value());
        bytes_t good = raw_record(secret, 0, 23, {1, 22});
        auto g = r.open(good.data(), good.size());
        ASSERT_TRUE(g.has_value());
        EXPECT_EQ(g->type, ContentType::handshake);
        EXPECT_EQ(error_of_open(r, e2), AlertDescription::bad_record_mac);   // the mode is over
    }
}

TEST(TlsRecord, Framer) {
    tls::RecordFramer f;
    auto feed = [&](const bytes_t& b) {
        auto room = f.room();
        ASSERT_GE(room.size(), b.size());
        std::memcpy(room.data(), b.data(), b.size());
        f.commit(b.size());
    };
    // byte by byte: nothing until the whole record
    bytes_t rec = {22, 3, 3, 0, 3, 1, 2, 3};
    for (size_t i = 0; i < rec.size(); ++i) {
        auto r = f.next();
        ASSERT_TRUE(r.has_value());
        EXPECT_TRUE(r->empty()) << i;
        feed({rec[i]});
    }
    auto r = f.next();
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(of(*r), rec);
    f.consume();
    EXPECT_EQ(f.buffered(), 0u);
    // two records and a half in one read
    bytes_t two = {23, 3, 3, 0, 1, 9, 21, 3, 3, 0, 2, 1, 0, 22, 3};
    feed(two);
    r = f.next();
    EXPECT_EQ(of(*r), (bytes_t{23, 3, 3, 0, 1, 9}));
    f.consume();
    r = f.next();
    EXPECT_EQ(of(*r), (bytes_t{21, 3, 3, 0, 2, 1, 0}));
    f.consume();
    r = f.next();
    EXPECT_TRUE(r->empty());
    EXPECT_EQ(f.buffered(), 2u);
    // the header is judged as soon as it is here
    tls::RecordFramer g;
    auto room = g.room();
    bytes_t unknown = {24, 3, 3, 0, 1};
    std::memcpy(room.data(), unknown.data(), 5);
    g.commit(5);
    auto u = g.next();
    ASSERT_FALSE(u.has_value());
    EXPECT_EQ(u.error().description, AlertDescription::unexpected_message);
    tls::RecordFramer h;
    room = h.room();
    bytes_t over = {23, 3, 3, uint8_t((tls::MaxCiphertext + 1) >> 8), uint8_t(tls::MaxCiphertext + 1)};
    std::memcpy(room.data(), over.data(), 5);
    h.commit(5);
    auto o = h.next();
    ASSERT_FALSE(o.has_value());
    EXPECT_EQ(o.error().description, AlertDescription::record_overflow);
    // many records of the largest size through reads of every size: the
    // room never runs out and every record comes out whole
    tls::RecordFramer big;
    bytes_t stream;
    for (int i = 0; i < 9; ++i) {
        stream.insert(stream.end(), {23, 3, 3, uint8_t(tls::MaxCiphertext >> 8), uint8_t(tls::MaxCiphertext & 0xFF)});
        stream.insert(stream.end(), tls::MaxCiphertext, uint8_t(i));
    }
    size_t at = 0, got = 0, chunk = 1;
    while (at < stream.size()) {
        auto space = big.room();
        ASSERT_FALSE(space.empty());
        size_t n = std::min({chunk, space.size(), stream.size() - at});
        std::memcpy(space.data(), stream.data() + at, n);
        big.commit(n);
        at += n;
        chunk = chunk * 31 % 40009 + 1;
        for (;;) {
            auto x = big.next();
            ASSERT_TRUE(x.has_value());
            if (x->empty()) {
                break;
            }
            EXPECT_EQ(x->size(), tls::MaxRecord);
            EXPECT_EQ(uint8_t(x->data()[5]), uint8_t(got));
            ++got;
            big.consume();
        }
    }
    EXPECT_EQ(got, 9u);
}

TEST(TlsRecord, Assembler) {
    auto message = [](uint8_t type, size_t n) {
        bytes_t m = {type, uint8_t(n >> 16), uint8_t(n >> 8), uint8_t(n)};
        for (size_t i = 0; i < n; ++i) {
            m.push_back(uint8_t(i + type));
        }
        return m;
    };
    {
        // one message over three fragments, the header split too
        tls::HandshakeAssembler a;
        bytes_t m = message(8, 40);
        ASSERT_TRUE(a.push(view(bytes_t(m.begin(), m.begin() + 2)), Epoch::handshake).has_value());
        EXPECT_FALSE(a.next());
        ASSERT_TRUE(a.push(view(bytes_t(m.begin() + 2, m.begin() + 20)), Epoch::handshake).has_value());
        EXPECT_FALSE(a.next());
        EXPECT_FALSE(a.empty());
        ASSERT_TRUE(a.push(view(bytes_t(m.begin() + 20, m.end())), Epoch::handshake).has_value());
        auto x = a.next();
        ASSERT_TRUE(x);
        EXPECT_EQ(of(*x), m);
        EXPECT_FALSE(a.next());
        EXPECT_TRUE(a.empty());
    }
    {
        // three messages in one fragment, and an empty one
        tls::HandshakeAssembler a;
        bytes_t m1 = message(8, 3), m2 = message(11, 0), m3 = message(15, 300), all;
        for (auto* m : {&m1, &m2, &m3}) {
            all.insert(all.end(), m->begin(), m->end());
        }
        ASSERT_TRUE(a.push(view(all), Epoch::handshake).has_value());
        EXPECT_EQ(of(*a.next()), m1);
        EXPECT_EQ(of(*a.next()), m2);
        EXPECT_EQ(of(*a.next()), m3);
        EXPECT_FALSE(a.next());
        EXPECT_TRUE(a.on_key_change().has_value());
    }
    {
        // the limits, judged from the header alone
        tls::HandshakeAssembler a;
        bytes_t h = {8, 0x01, 0x00, 0x01};   // 65537
        auto r = a.push(view(h), Epoch::handshake);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().description, AlertDescription::illegal_parameter);
        tls::HandshakeAssembler c;
        bytes_t cert = {11, 0x04, 0x00, 0x01};   // 262145
        r = c.push(view(cert), Epoch::handshake);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().description, AlertDescription::illegal_parameter);
        tls::HandshakeAssembler huge;
        bytes_t h16 = {1, 0xFF, 0xFF, 0xFF};   // 16 MiB, nothing buffered for it
        EXPECT_FALSE(huge.push(view(h16), Epoch::initial).has_value());
        // a limit in a later message of the same fragment
        tls::HandshakeAssembler later;
        bytes_t two = message(8, 2);
        two.insert(two.end(), {15, 0x01, 0x00, 0x01});
        EXPECT_FALSE(later.push(view(two), Epoch::handshake).has_value());
    }
    {
        // the largest messages, over records
        for (auto [type, size] : {std::pair{uint8_t(8), size_t(65536)}, std::pair{uint8_t(11), size_t(262144)}}) {
            tls::HandshakeAssembler a;
            bytes_t m = message(type, size);
            for (size_t at = 0; at < m.size(); at += tls::MaxPlaintext) {
                size_t n = std::min(tls::MaxPlaintext, m.size() - at);
                ASSERT_TRUE(a.push(view(bytes_t(m.begin() + at, m.begin() + at + n)), Epoch::handshake).has_value());
            }
            auto x = a.next();
            ASSERT_TRUE(x);
            EXPECT_EQ(of(*x), m);
        }
    }
    {
        // §5.1: no message spans a change of keys
        tls::HandshakeAssembler a;
        bytes_t m = message(2, 10);
        ASSERT_TRUE(a.push(view(bytes_t(m.begin(), m.begin() + 6)), Epoch::initial).has_value());
        auto r = a.push(view(bytes_t(m.begin() + 6, m.end())), Epoch::handshake);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().description, AlertDescription::unexpected_message);
        tls::HandshakeAssembler b;
        ASSERT_TRUE(b.push(view(bytes_t(m.begin(), m.begin() + 6)), Epoch::initial).has_value());
        auto k = b.on_key_change();
        ASSERT_FALSE(k.has_value());
        EXPECT_EQ(k.error().description, AlertDescription::unexpected_message);
        // a whole message not yet taken, with the keys changing: the same
        tls::HandshakeAssembler c;
        bytes_t sh_ee = message(2, 5);
        bytes_t ee = message(8, 2);
        sh_ee.insert(sh_ee.end(), ee.begin(), ee.end());
        ASSERT_TRUE(c.push(view(sh_ee), Epoch::initial).has_value());
        ASSERT_TRUE(c.next());   // the ServerHello, then the keys change
        EXPECT_FALSE(c.on_key_change().has_value());
    }
    {
        tls::HandshakeAssembler a;
        auto r = a.push(sgcl::slice<const sgcl::byte>(), Epoch::initial);
        ASSERT_FALSE(r.has_value());
        EXPECT_EQ(r.error().description, AlertDescription::unexpected_message);
    }
}

// The framer's blocks (RecordBlocks): lent while a record is held, given
// back zeroed as far as they were written, taken again from the thread's list
TEST(TlsRecord, BlocksComeBackZeroed) {
    tls::RecordFramer f;
    auto room = f.room();
    ASSERT_GE(room.size(), 5u);
    const uint8_t* block = reinterpret_cast<const uint8_t*>(room.data());
    bytes_t rec = {23, 3, 3, 0, 4, 9, 9, 9, 9};
    std::memcpy(room.data(), rec.data(), rec.size());
    f.commit(rec.size());
    auto r = f.next();
    ASSERT_TRUE(r.has_value() && !r->empty());
    f.consume();   // nothing held: the block goes back, zeroed
    EXPECT_EQ(f.buffered(), 0u);
    uint8_t* again = tls::RecordBlocks::take(tls::RecordBlocks::Small);
    EXPECT_EQ(again, block);   // the thread's list, last in first out
    for (size_t i = 0; i < rec.size(); ++i) {
        EXPECT_EQ(again[i], 0) << i;
    }
    tls::RecordBlocks::give(again, tls::RecordBlocks::Small, 0);
    // a record longer than a small block: a large one, the bytes moved
    tls::RecordFramer g;
    bytes_t big = {23, 3, 3, 0x20, 0x00};   // 8192 bytes of ciphertext
    big.resize(5 + 8192, 7);
    size_t at = 0;
    while (at < big.size()) {
        auto space = g.room();
        ASSERT_FALSE(space.empty());
        size_t n = std::min(space.size(), big.size() - at);
        std::memcpy(space.data(), big.data() + at, n);
        g.commit(n);
        at += n;
    }
    auto whole = g.next();
    ASSERT_TRUE(whole.has_value());
    EXPECT_EQ(of(*whole), big);
    g.consume();
    // a read that brought nothing: the block given back by release()
    tls::RecordFramer h;
    (void)h.room();
    h.release();
    EXPECT_EQ(h.buffered(), 0u);
}
