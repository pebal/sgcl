//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Encrypted Client Hello (sgcl/net/tls/detail/ech.h and the server's
// machine with a key, sgcl/net/tls/detail/server_handshake.h), without an
// oracle. The first byte picks what the rest is:
//   0 (mod 4): an ECHConfigList. Each config that reads, written again of
//     what was read, reads again to the same fields, and choose_ech picks
//     only a config whose KEM and suite the module has;
//   1: a ClientHelloOuter's body (a length of two bytes, then that many
//     bytes) and an EncodedClientHelloInner (the rest). A ClientHelloInner
//     decode_inner gives is one whole handshake message;
//   2, 3: messages for the server of tls_fuzz_settings.h with the fixed ECH
//     key (bits 2..4 of the first byte the server's mode, as in
//     tls_server_fuzz.cpp), each a type, a length of two bytes and that many
//     bytes, with the checks of tls_server_fuzz.cpp.
// What must hold besides: no read past an input, nothing crashes (ASan,
// UBSan); every parse reads libFuzzer's buffer or a heap block of exactly
// the message's size, never a managed copy, whose overreads ASan misses. The seeds of 2 (seeds/tls_ech, tests/net/tls_fuzz_seeds.cpp) are
// our client's hellos sealed to the key: accepted, through a
// HelloRetryRequest, and sealed to another key (rejected).
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/tls_ech_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "tests/net/fuzz/tls_fuzz_settings.h"

#include <cstring>
#include <memory>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void configs(const uint8_t* data, size_t size) {
        auto list = tls::read_ech_configs(tls::bytes_of(data, size));
        if (!list) {
            check(!tls::choose_ech(tls::bytes_of(data, size)));
            return;
        }
        for (const auto& c : *list) {
            auto w = tls::write_ech_config(c.id, c.kem, tls::bytes_of(c.public_key.data(), c.public_key.size()), c.suites, c.max_name_length, c.public_name);
            std::vector<sgcl::byte> l = {sgcl::byte(w.size() >> 8), sgcl::byte(w.size())};
            l.insert(l.end(), w.begin(), w.end());
            auto again = tls::read_ech_configs(tls::bytes_of(l.data(), l.size()));
            check(again && again->size() == 1);
            const auto& d = again->front();
            check(d.id == c.id && d.kem == c.kem && d.public_key == c.public_key && d.suites == c.suites && d.max_name_length == c.max_name_length
                  && d.public_name == c.public_name);
        }
        if (auto choice = tls::choose_ech(tls::bytes_of(data, size))) {
            check(tls::ech_kem_known(choice->config.kem) && tls::ech_public_name_ok(choice->config.public_name));
        }
    }

    void inner(const uint8_t* data, size_t size) {
        if (size < 2) {
            return;
        }
        size_t n = std::min(size_t(data[0]) << 8 | data[1], size - 2);
        auto outer = tls::read_client_hello(tls::bytes_of(data + 2, n));
        if (!outer) {
            return;
        }
        auto m = tls::decode_inner(tls::bytes_of(data + 2 + n, size - 2 - n), *outer);
        if (m) {
            check(m->size() >= 4 && uint8_t(m->front()) == 1);
            check(m->size() - 4 == (size_t(uint8_t((*m)[1])) << 16 | size_t(uint8_t((*m)[2])) << 8 | uint8_t((*m)[3])));
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
                check(s.established());
            }
            if (a.kind == tls::Action::Kind::send) {
                check(a.offset + a.size <= step.out.size());
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

    void server(uint8_t mode, const uint8_t* data, size_t size) {
        tls_fuzz::ServerEntropy f;
        tls::ServerHandshake server(tls_fuzz::ech_server_settings(mode), f.entropy(), tls_fuzz::clock());
        int established = 0;
        size_t at = 0;
        while (at + 3 <= size) {
            const uint8_t type = data[at];
            size_t n = size_t(data[at + 1]) << 8 | data[at + 2];
            at += 3;
            n = std::min(n, size - at);
            // each message in a heap block of exactly its size: ASan sees a read past it
            std::unique_ptr<uint8_t[]> m(new uint8_t[4 + n]);
            m[0] = type;
            m[1] = uint8_t(n >> 16);
            m[2] = uint8_t(n >> 8);
            m[3] = uint8_t(n);
            if (n) {
                std::memcpy(m.get() + 4, data + at, n);
            }
            at += n;
            if (!check_step(server, server.feed(tls::bytes_of(m.get(), 4 + n)), established)) {
                check(server.feed(tls::bytes_of(m.get(), 4 + n)).actions.empty());
                return;
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const uint8_t first = data[0];
    ++data;
    --size;
    switch (first & 3) {
    case 0:
        configs(data, size);
        break;
    case 1:
        inner(data, size);
        break;
    default:
        server(uint8_t((first >> 2) & 7), data, size);
        break;
    }
    return 0;
}
