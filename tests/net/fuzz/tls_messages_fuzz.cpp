//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The handshake messages of TLS 1.3 (sgcl/net/tls/detail/messages.h) on any
// bytes, without an oracle. The input is one handshake message, header and
// all. What must hold:
//   - no read goes past the input (ASan) and nothing crashes;
//   - a message that reads is written back byte for byte (a reader refuses
//     what a writer would change: a legacy_version, compression methods, a
//     length), and so is every extension of it whose typed reader takes its
//     body (server_name up to the name types it passes over);
//   - so is a TLS 1.2 message that reads as one (ServerKeyExchange,
//     ServerHelloDone, and the 1.2 forms of Certificate, CertificateRequest
//     and NewSessionTicket), and a ServerHello without extensions;
//   - validate_extensions takes any message that reads, with any offer;
//   - an alert record of the first bytes reads or is refused.
// Seeds: the messages of RFC 8448 (seeds/tls_messages). Built with
// libFuzzer (tests/fuzz/run.sh tests/net/fuzz/tls_messages_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "tests/net/tls_roundtrip.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // A reason of message_back that is a failure of the code, not of the input
    bool broken(const std::string& why) {
        return why.find("came back otherwise") != std::string::npos;
    }

    void validate(const tls::Bytes& body, uint8_t type, uint64_t offered) {
        auto run = [&](tls::HandshakeType t, bool retry, const tls::Extensions& x) {
            (void)tls::validate_extensions(t, retry, x, offered);
        };
        switch (tls::HandshakeType(type)) {
            case tls::HandshakeType::client_hello:
                if (auto m = tls::read_client_hello(body)) {
                    run(tls::HandshakeType::client_hello, false, m->extensions);
                }
                break;
            case tls::HandshakeType::server_hello:
                if (auto m = tls::read_server_hello(body)) {
                    run(tls::HandshakeType::server_hello, m->is_retry(), m->extensions);
                }
                break;
            case tls::HandshakeType::encrypted_extensions:
                if (auto m = tls::read_encrypted_extensions(body)) {
                    run(tls::HandshakeType::encrypted_extensions, false, *m);
                }
                break;
            case tls::HandshakeType::certificate:
                if (auto m = tls::read_certificate(body)) {
                    for (auto e : *m) {
                        run(tls::HandshakeType::certificate, false, e.extensions);
                    }
                }
                break;
            default:
                break;
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::vector<uint8_t> input(data, data + size);
    auto message = tls::bytes_of(input.data(), input.size());
    if (auto h = tls::read_handshake(message)) {
        size_t hash = h->body.size() == 48 ? 48 : 32;
        auto why = tls_roundtrip::message_back(input, hash);
        check(!broken(why));
        auto why12 = tls_roundtrip::message_back12(input);   // TLS 1.2's Certificate, CertificateRequest, NewSessionTicket
        check(why12.empty());
        uint64_t offered = size > 4 ? uint64_t(data[size - 1]) * 0x0101010101010101ull : ~uint64_t(0);
        validate(h->body, h->type, offered);
    }
    if (size >= 2) {
        auto a = tls::read_alert(tls::bytes_of(data, size < 3 ? size : 3));
        check(size == 2 ? bool(a) : !a);
    }
    return 0;
}
