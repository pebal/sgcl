//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's handshake machine (sgcl/net/tls/detail/server_handshake.h)
// fed any sequence of messages, without an oracle. The first byte picks
// the server (tls_fuzz_settings.h): bit 0 clear, the defaults (every group,
// an Ed25519 identity), set, P-256 alone (a HelloRetryRequest for most
// clients), a cookie and ALPN; bit 1, tickets issued and taken under a
// fixed key (the PSK paths: the identities opened, the binder, the resumed
// flight, the NewSessionTicket after the client's Finished); bit 2, a
// client certificate required (the CertificateRequest, the client's
// Certificate verified against the test CA, its CertificateVerify). The
// clock stands still.
// The rest is messages, each a type, a length of two bytes and that many
// bytes (cut at the end of the input). What must hold:
//   - no read past a message, nothing crashes (ASan, UBSan);
//   - a step has one alert at most, the last action, and the machine
//     failed then; after it every feed gives nothing;
//   - a send action lies inside the step's buffer, and its bytes are whole
//     handshake messages that read (the server writes what it can read);
//   - established comes once, and only after a Finished.
// The entropy is fixed, so a seed made by our client against this server
// (seeds/tls_server, tests/net/tls_fuzz_seeds.cpp) reaches the end of the
// handshake and KeyUpdate, a resumed one and one with a client certificate.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/tls_server_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "tests/net/fuzz/tls_fuzz_settings.h"

#include <cstring>
#include <string>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool check_step(tls::ServerHandshake& s, const tls::Step& step, int& established) {
        size_t alerts = 0;
        for (auto& a : step.actions) {
            if (a.kind == tls::Action::Kind::alert) {
                ++alerts;
                check(&a == &step.actions.back());
            }
            if (a.kind == tls::Action::Kind::established) {
                ++established;
                check(s.established());   // in the step that says so (a later message may still fail it)
            }
            if (a.kind == tls::Action::Kind::send) {
                check(a.offset + a.size <= step.out.size());
                // whole messages, each of which reads
                auto b = step.bytes(a);
                size_t at = 0;
                while (at < b.size()) {
                    check(at + 4 <= b.size());
                    size_t n = size_t(uint8_t(b[at + 1])) << 16 | size_t(uint8_t(b[at + 2])) << 8 | uint8_t(b[at + 3]);
                    check(at + 4 + n <= b.size());
                    check(bool(tls::read_handshake(b.subslice(at, 4 + n))));
                    at += 4 + n;
                }
            }
        }
        check(alerts <= 1);
        check(alerts == 0 || s.failed());
        check(established <= 1);
        return alerts == 0;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const uint8_t mode = data[0] & 7;
    ++data;
    --size;
    tls_fuzz::ServerEntropy f;
    tls::ServerHandshake server(tls_fuzz::server_settings(mode), f.entropy(), tls_fuzz::clock());
    int established = 0;
    std::vector<uint8_t> m;
    size_t at = 0;
    while (at + 3 <= size) {
        const uint8_t type = data[at];
        size_t n = size_t(data[at + 1]) << 8 | data[at + 2];
        at += 3;
        n = std::min(n, size - at);
        m.assign({type, uint8_t(n >> 16), uint8_t(n >> 8), uint8_t(n)});
        m.insert(m.end(), data + at, data + at + n);
        at += n;
        if (!check_step(server, server.feed(tls::bytes_of(m.data(), m.size())), established)) {
            check(server.feed(tls::bytes_of(m.data(), m.size())).actions.empty());   // nothing after an alert
            return 0;
        }
    }
    return 0;
}
