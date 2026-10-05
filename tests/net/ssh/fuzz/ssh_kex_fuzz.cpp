//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ssh's key exchange without I/O on any bytes: a KEXINIT read and
// negotiated against the module's own both ways, and each method's
// messages (the client's KEX_REPLY after its init, the server's view of a
// KEX_INIT), the method picked by the first byte. What must hold:
//   - a KEXINIT read gives names that are none empty, and a negotiation's
//     result names algorithms of the module's tables, a MAC exactly when the
//     cipher is not an AEAD;
//   - a reply or an init is refused with a message or taken; taken, K and H
//     are made, H of its hash's size, and the host key and signature views
//     lie inside the message.
#include "sgcl/net/ssh/detail/kex.h"

#include <cstdint>

namespace {
    using namespace sgcl::net::ssh::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool inside(const Span& s, const uint8_t* p, size_t n) {
        return s.n == 0 || (s.p >= p && s.p + s.n <= p + n);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) {
        return 0;
    }
    const uint8_t mode = data[0];
    const uint8_t* p = data + 1;
    const size_t n = size - 1;
    if ((mode & 3) == 0) {
        Kexinit peer;
        if (!read_kexinit(p, n, peer)) {
            return 0;
        }
        for (const auto& list : peer.lists) {
            for (const auto& name : list) {
                check(!name.empty());
            }
        }
        auto ours = make_kexinit(default_preferences(), true, true);
        Kexinit mine;
        check(read_kexinit(ours.data(), ours.size(), mine));
        for (bool client : {true, false}) {
            Negotiated neg;
            const char* e = client ? negotiate(mine, peer, true, neg) : negotiate(peer, mine, false, neg);
            if (!e) {
                check(neg.kex && neg.host_key && find_kex(neg.kex->name) == neg.kex);
                for (int d = 0; d < 2; ++d) {
                    check(neg.cipher[d] != nullptr);
                    check((neg.mac[d] == nullptr) == neg.cipher[d]->aead);
                }
            }
        }
        return 0;
    }
    const KexInfo& info = kex_table[(mode >> 2) % std::size(kex_table)];
    Bytes ic = {MsgKexinit, 1}, is = {MsgKexinit, 2};
    HashInputs in{"SSH-2.0-client", "SSH-2.0-server", &ic, &is};
    if ((mode & 3) == 1) {
        KexExchange x(info);
        (void)x.client_init();
        Span hk, sig;
        if (!x.client_reply(p, n, in, hk, sig)) {
            check(x.h().size() == hash_size(info.hash) && !x.k().empty());
            check(inside(hk, p, n) && inside(sig, p, n) && hk.n > 0);
        }
        return 0;
    }
    KexExchange x(info);
    Bytes blob = {0, 0, 0, 1, 'x'}, reply;
    if (!x.server_init(p, n, in, blob, reply)) {
        check(x.h().size() == hash_size(info.hash) && !x.k().empty());
        check(reply.size() > 1 && reply[0] == MsgKexReply);
    }
    return 0;
}
