//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The client's handshake machine (sgcl/net/tls/detail/handshake.h) fed any
// server messages. The first byte picks the client: bit 0 clear, the
// client of RFC 8448 §3 (its settings and its randomness, so that the
// trace's server messages, the seeds, take it through the whole handshake
// and the fuzzer works on every state up to the messages after it); bit 0
// set, a client of the default settings (X25519MLKEM768 and X25519 shares,
// compatibility mode, ALPN offered) on fixed randomness. The rest is a
// list of messages, each a type, a 16-bit length and the body, fed one by
// one until the machine sends an alert. What must hold: no fault, a step
// of actions in the order the machine may give them, one alert at most,
// ending the machine, and nothing after it.
#include "sgcl/net/tls/detail/handshake.h"

#include <cstring>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // RFC 8448 §3: the ClientHello's random, then the client's X25519 key
    const uint8_t rfc_entropy[64] = {
        0xcb, 0x34, 0xec, 0xb1, 0xe7, 0x81, 0x63, 0xba, 0x1c, 0x38, 0xc6, 0xda, 0xcb, 0x19, 0x6a, 0x6d,
        0xff, 0xa2, 0x1a, 0x8d, 0x99, 0x12, 0xec, 0x18, 0xa2, 0xef, 0x62, 0x83, 0x02, 0x4d, 0xec, 0xe7,
        0x49, 0xaf, 0x42, 0xba, 0x7f, 0x79, 0x94, 0x85, 0x2d, 0x71, 0x3e, 0xf2, 0x78, 0x4b, 0xcb, 0xca,
        0xa7, 0x91, 0x1d, 0xe2, 0x6a, 0xdc, 0x56, 0x42, 0xcb, 0x63, 0x45, 0x40, 0xe7, 0xea, 0x50, 0x05,
    };

    struct Fixed {
        const uint8_t* data;
        size_t size;
        size_t at = 0;

        static void fill(void* self, uint8_t* out, size_t n) {
            auto& f = *static_cast<Fixed*>(self);
            for (size_t i = 0; i < n; ++i) {
                out[i] = f.at < f.size ? f.data[f.at++] : uint8_t(i * 29 + 7);
            }
        }
    };

    tls::ClientSettings rfc_settings() {
        tls::ClientSettings s;
        s.server_name = sgcl::string("server");
        s.insecure_skip_verify = true;
        s.compatibility_mode = false;
        s.session_ticket_extension = true;
        s.record_size_limit = 0x4001;
        s.ciphers = {0x1301, 0x1303, 0x1302};
        s.groups = {0x001d, 0x0017, 0x0018, 0x0019, 0x0100, 0x0101, 0x0102, 0x0103, 0x0104};
        s.key_shares = {0x001d};
        s.schemes = {0x0403, 0x0503, 0x0603, 0x0203, 0x0804, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601, 0x0201, 0x0402, 0x0502, 0x0602, 0x0202};
        return s;
    }

    void run(tls::ClientHandshake& c, const uint8_t* data, size_t size) {
        auto check_step = [&](const tls::Step& step) {
            size_t alerts = 0;
            for (auto& a : step.actions) {
                if (a.kind == tls::Action::Kind::alert) {
                    ++alerts;
                    check(&a == &step.actions.back());   // the last action
                }
                if (a.kind == tls::Action::Kind::send) {
                    check(a.offset + a.size <= step.out.size());
                }
            }
            check(alerts <= 1);
            check(alerts == 0 || c.failed());
            return alerts == 0;
        };
        if (!check_step(c.start())) {
            return;
        }
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
            if (!check_step(c.feed(tls::bytes_of(m.data(), m.size())))) {
                check(c.feed(tls::bytes_of(m.data(), m.size())).actions.empty());   // nothing after an alert
                return;
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const uint8_t mode = data[0];
    ++data;
    --size;
    if ((mode & 1) == 0) {
        Fixed f{rfc_entropy, sizeof rfc_entropy};
        tls::ClientHandshake c(rfc_settings(), tls::Entropy{&Fixed::fill, &f});
        run(c, data, size);
    } else {
        Fixed f{nullptr, 0};
        tls::ClientSettings s;
        s.server_name = sgcl::string("example.com");
        s.insecure_skip_verify = true;
        s.alpn = {sgcl::string("h2"), sgcl::string("http/1.1")};
        tls::ClientHandshake c(s, tls::Entropy{&Fixed::fill, &f});
        run(c, data, size);
    }
    return 0;
}
