//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// A handshake message of TLS 1.3 read and written back (messages.h), and
// every extension of it v1 reads, its body read and written back: what the
// tests of tls_messages.cpp hold the RFC 8448 messages to, and what the
// fuzzer holds any message that reads to (tests/net/fuzz/tls_messages_fuzz.cpp).
// No gtest: a reason in a string, empty when everything came back the same.
#pragma once

#include "sgcl/net/tls/detail/messages.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tls_roundtrip {
    namespace tls = sgcl::net::tls::detail;
    using bytes_t = std::vector<uint8_t>;

    inline tls::Bytes view(const bytes_t& v) {
        return tls::bytes_of(v.data(), v.size());
    }

    inline bytes_t of(const std::vector<sgcl::byte>& v) {
        return bytes_t(reinterpret_cast<const uint8_t*>(v.data()), reinterpret_cast<const uint8_t*>(v.data()) + v.size());
    }

    inline bytes_t of(const tls::Bytes& b) {
        return bytes_t(reinterpret_cast<const uint8_t*>(b.data()), reinterpret_cast<const uint8_t*>(b.data()) + b.size());
    }

    inline size_t extensions_compared = 0;   // the bodies written back and compared, over the whole run

    // An extension's body read by its typed reader and written back; an
    // empty string when it came back the same (or is one v1 only passes over)
    inline std::string extension_back(tls::HandshakeType in, bool retry, const tls::Extension& e) {
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        bool client = in == tls::HandshakeType::client_hello;
        bool fail = false;
        auto take = [&](const auto& r, auto&& write) {
            if (!r) {
                fail = true;
            } else {
                write(*r);
            }
        };
        switch (tls::ExtensionType(e.type)) {
            case tls::ExtensionType::server_name:
                if (e.body.empty()) {
                    return tls::read_empty(e.body) ? "" : "empty server_name";
                }
                take(tls::read_server_name(e.body), [&](auto& h) { tls::write_server_name(w, h); });
                if (!fail && of(out) != of(e.body)) {
                    // names of other types than host_name are passed over:
                    // what comes back is the host name alone, the same one
                    auto again = tls::read_server_name(tls::bytes_of(out.data(), out.size()));
                    auto first = tls::read_server_name(e.body);
                    ++extensions_compared;
                    return again && of(*again) == of(*first) ? "" : "server_name came back otherwise";
                }
                break;
            case tls::ExtensionType::supported_groups:
                take(tls::read_groups(e.body), [&](auto& l) { tls::write_groups(w, l); });
                break;
            case tls::ExtensionType::signature_algorithms:
            case tls::ExtensionType::signature_algorithms_cert:
                take(tls::read_signature_schemes(e.body), [&](auto& l) { tls::write_signature_schemes(w, l); });
                break;
            case tls::ExtensionType::supported_versions:
                if (client) {
                    take(tls::read_versions_offered(e.body), [&](auto& l) { tls::write_versions_offered(w, l); });
                } else {
                    take(tls::read_version_selected(e.body), [&](auto v) { tls::write_version_selected(w, v); });
                }
                break;
            case tls::ExtensionType::key_share:
                if (client) {
                    take(tls::read_key_shares(e.body), [&](auto& l) { tls::write_key_shares(w, l); });
                } else if (retry) {
                    take(tls::read_key_share_retry(e.body), [&](auto g) { tls::write_key_share_retry(w, g); });
                } else {
                    take(tls::read_key_share_selected(e.body), [&](auto& s) { tls::write_key_share_selected(w, s); });
                }
                break;
            case tls::ExtensionType::application_layer_protocol_negotiation:
                take(tls::read_protocols(e.body), [&](auto& l) {
                    std::vector<tls::Bytes> names(l.begin(), l.end());   // lint-handles: ok slices over unmanaged bytes, no owner
                    tls::write_protocols(w, names);
                });
                break;
            case tls::ExtensionType::cookie:
                take(tls::read_cookie(e.body), [&](auto& c) { tls::write_cookie(w, c); });
                break;
            case tls::ExtensionType::psk_key_exchange_modes:
                take(tls::read_psk_modes(e.body), [&](auto& m) { tls::write_psk_modes(w, m); });
                break;
            case tls::ExtensionType::early_data:
                if (in == tls::HandshakeType::new_session_ticket) {
                    take(tls::read_early_data_limit(e.body), [&](auto n) { w.u32(n); });
                    break;
                }
                return tls::read_empty(e.body) ? "" : "early_data not empty";
            case tls::ExtensionType::pre_shared_key:
                if (client) {
                    return tls::read_pre_shared_keys(e.body) ? "" : "pre_shared_key";
                }
                take(tls::read_pre_shared_key_selected(e.body), [&](auto v) { w.u16(v); });
                break;
            case tls::ExtensionType::record_size_limit:
                take(tls::read_record_size_limit(e.body), [&](auto v) { w.u16(v); });
                break;
            default:
                return "";
        }
        if (fail) {
            return "extension " + std::to_string(e.type) + " refused";
        }
        ++extensions_compared;
        return of(out) == of(e.body) ? "" : "extension " + std::to_string(e.type) + " came back otherwise";
    }

    // A message read and written back, and each of its extensions; an empty
    // string when all came back the same
    inline std::string message_back(const bytes_t& message, size_t hash_size = 32) {
        auto h = tls::read_handshake(view(message));
        if (!h) {
            return "header refused";
        }
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        std::string why;
        auto each = [&](tls::HandshakeType t, bool retry, const tls::Extensions& x) {
            for (auto e : x) {
                auto r = extension_back(t, retry, e);
                if (!r.empty() && why.empty()) {
                    why = r;
                }
            }
        };
        auto raw = [](const tls::Extensions& x) {
            return [&x](tls::Builder& b) { tls::write_raw(b, x); };
        };
        switch (tls::HandshakeType(h->type)) {
            case tls::HandshakeType::client_hello: {
                auto m = tls::read_client_hello(h->body);
                if (!m) {
                    return std::string("ClientHello refused: ") + m.error().what;
                }
                tls::write_client_hello(w, m->random, m->session_id, m->cipher_suites, raw(m->extensions));
                each(tls::HandshakeType::client_hello, false, m->extensions);
                break;
            }
            case tls::HandshakeType::server_hello: {
                auto m = tls::read_server_hello(h->body);
                if (!m) {
                    return std::string("ServerHello refused: ") + m.error().what;
                }
                tls::write_server_hello(w, m->random, m->session_id, m->cipher_suite, raw(m->extensions));
                each(tls::HandshakeType::server_hello, m->is_retry(), m->extensions);
                break;
            }
            case tls::HandshakeType::encrypted_extensions: {
                auto m = tls::read_encrypted_extensions(h->body);
                if (!m) {
                    return std::string("EncryptedExtensions refused: ") + m.error().what;
                }
                tls::write_encrypted_extensions(w, raw(*m));
                each(tls::HandshakeType::encrypted_extensions, false, *m);
                break;
            }
            case tls::HandshakeType::certificate_request: {
                auto m = tls::read_certificate_request(h->body);
                if (!m) {
                    return std::string("CertificateRequest refused: ") + m.error().what;
                }
                tls::write_certificate_request(w, m->context, raw(m->extensions));
                each(tls::HandshakeType::certificate_request, false, m->extensions);
                break;
            }
            case tls::HandshakeType::certificate: {
                auto m = tls::read_certificate(h->body);
                if (!m) {
                    return std::string("Certificate refused: ") + m.error().what;
                }
                std::vector<tls::CertificateEntry> entries(m->begin(), m->end());   // lint-handles: ok slices over unmanaged bytes, no owner
                tls::write_certificate(w, m->context, entries);
                break;
            }
            case tls::HandshakeType::certificate_verify: {
                auto m = tls::read_certificate_verify(h->body);
                if (!m) {
                    return std::string("CertificateVerify refused: ") + m.error().what;
                }
                tls::write_certificate_verify(w, m->scheme, m->signature);
                break;
            }
            case tls::HandshakeType::finished: {
                auto m = tls::read_finished(h->body, hash_size);
                if (!m) {
                    return std::string("Finished refused: ") + m.error().what;
                }
                tls::write_finished(w, *m);
                break;
            }
            case tls::HandshakeType::new_session_ticket: {
                auto m = tls::read_new_session_ticket(h->body);
                if (!m) {
                    return std::string("NewSessionTicket refused: ") + m.error().what;
                }
                tls::write_new_session_ticket(w, *m);
                each(tls::HandshakeType::new_session_ticket, false, m->extensions);
                break;
            }
            case tls::HandshakeType::key_update: {
                auto m = tls::read_key_update(h->body);
                if (!m) {
                    return std::string("KeyUpdate refused: ") + m.error().what;
                }
                tls::write_key_update(w, *m);
                break;
            }
            case tls::HandshakeType::end_of_early_data: {
                if (!tls::read_end_of_early_data(h->body)) {
                    return "EndOfEarlyData refused";
                }
                tls::write_end_of_early_data(w);
                break;
            }
            default:
                return "an unknown message";
        }
        if (!why.empty()) {
            return why;
        }
        return of(out) == message ? "" : "the message came back otherwise";
    }
}
