//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The server's session tickets (sgcl/net/tls/detail/session.h) on any bytes,
// without an oracle. The first byte picks what the rest is: even, a ticket
// given to the opener of a fixed key (what a client's pre_shared_key
// carries); odd, a ticket's content, sealed under that key first. What must
// hold:
//   - no read past the input, nothing crashes (ASan, UBSan);
//   - a ticket opens only to as many bytes as it has less the overhead, and
//     what opens is the content that was sealed (the odd mode: the input
//     itself, a byte changed refused);
//   - a content that reads, with a chain whose certificates parse, is
//     written back byte for byte.
// Seeds: tickets of the fixed key and their contents (seeds/tls_ticket,
// tests/net/tls_fuzz_seeds.cpp). Built with libFuzzer (tests/fuzz/run.sh
// tests/net/fuzz/tls_ticket_fuzz.cpp) or replayed by the library's own
// driver (tests/fuzz/driver.cpp).
#include "tests/net/fuzz/tls_fuzz_settings.h"

#include <cstring>
#include <vector>

namespace {
    namespace tls = sgcl::net::tls::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    // A content that reads written back, when its chain parses
    void content_back(const uint8_t* p, size_t n) {
        auto t = tls::read_ticket_content(tls::bytes_of(p, n));
        if (!t) {
            return;
        }
        check(t->psk.size() >= 32 && t->psk.size() <= tls::MaxHashSize);
        sgcl::crypto::x509::chain chain;
        tls::Reader list(t->certificates);
        while (!list.empty()) {
            tls::Bytes der;
            check(list.vec24(der, 1, 0xFFFFFF));
            auto c = sgcl::crypto::x509::certificate::parse(der);
            if (!c || c->raw().size() != der.size()) {
                return;
            }
            chain.push_back(std::move(*c));
        }
        tls::Secret psk;
        std::memcpy(psk.bytes, t->psk.data(), t->psk.size());
        psk.size = uint8_t(t->psk.size());
        std::vector<sgcl::byte> out;
        tls::Builder w(out);
        tls::write_ticket_content(w, t->cipher, t->issued_ms, t->lifetime, t->age_add, psk, chain);
        check(out.size() == n && std::memcmp(out.data(), p, n) == 0);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    const bool seal = data[0] & 1;
    ++data;
    --size;
    tls::TicketKeys& keys = tls_fuzz::ticket_keys();
    if (!seal) {
        std::vector<uint8_t> out(size >= tls::TicketKeys::Overhead ? size - tls::TicketKeys::Overhead : 0);
        auto n = keys.open(tls::bytes_of(data, size), out.data());
        if (n) {
            check(size >= tls::TicketKeys::Overhead && *n == size - tls::TicketKeys::Overhead);
            content_back(out.data(), *n);
        }
        return 0;
    }
    if (size > 0xFFFF) {
        return 0;
    }
    std::vector<sgcl::byte> ticket;
    tls_fuzz::ServerEntropy e;
    keys.seal(ticket, tls::bytes_of(data, size), tls_fuzz::Now, 86'400'000, e.entropy());
    check(ticket.size() == size + tls::TicketKeys::Overhead);
    std::vector<uint8_t> out(size);
    auto n = keys.open(tls::bytes_of(ticket.data(), ticket.size()), out.data());
    check(n && *n == size && std::memcmp(out.data(), data, size) == 0);
    ticket[ticket.size() / 2] ^= sgcl::byte(0x80);
    check(!keys.open(tls::bytes_of(ticket.data(), ticket.size()), out.data()));
    content_back(data, size);
    return 0;
}
