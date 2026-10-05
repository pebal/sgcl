//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The multicast DNS responder and querier (sgcl/net/detail/mdns_engine.h)
// on any message, without sockets and without an oracle: an engine of one
// link (the loopback's addresses) holding a host's entry and a service's,
// each probing or announced as the first byte says, and a cache; the rest
// is a message as it came on the link, from port 5353 or (the first
// byte's bit 2) another, a legacy querier's. What must hold:
//   - a response's records go into the cache only with a TTL, a goodbye
//     makes one expire within a second; the cache's expiry never faults;
//   - a conflict or a lost tie-break is seen only by an entry that has a
//     unique record of the name; a rename after it keeps the name a name,
//     its label at most 63 bytes, however many times it is renamed;
//   - a query's reply holds only the announced entries' records, of the
//     questions' names, none twice, none the query knows with at least
//     half its TTL; its messages read back, each under 1440 bytes, and a
//     legacy reply carries the query's id, its questions, TTLs of 10 s at
//     most and no cache-flush bit.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/mdns_responder_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/detail/mdns_engine.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    nd::DnsName name_of(const char* text) {
        nd::DnsName n;
        nd::dns_name_from_text(text, n);
        return n;
    }

    tracked_ptr<nd::MdnsEngine> engine(uint8_t mode) {
        tracked_ptr<nd::MdnsEngine> e = make_tracked<nd::MdnsEngine>();
        nd::MdnsLink link;
        link.ifi.name = "lo0";
        link.ifi.index = 1;
        link.ifi.addresses.push_back(net::ip_network(net::ip_address::loopback_v4(), 8));
        link.ifi.addresses.push_back(net::ip_network(net::ip_address::loopback_v6(), 128));
        e->links.push_back(link);
        e->own.push_back(net::ip_address::loopback_v4());
        e->own.push_back(net::ip_address::loopback_v6());
        tracked_ptr<nd::MdnsEntry> host = make_tracked<nd::MdnsEntry>();
        host->host = true;
        host->base = host->label = "fuzzhost";
        host->domain = name_of("local.");
        host->state = mode & 1 ? nd::MdnsEntry::State::announced : nd::MdnsEntry::State::probing;
        e->entries.push_back(host);
        tracked_ptr<nd::MdnsEntry> svc = make_tracked<nd::MdnsEntry>();
        svc->base = svc->label = "Fuzz Service";
        svc->type = name_of("_http._tcp.local.");
        svc->domain = name_of("local.");
        svc->target = name_of("fuzzhost.local.");
        svc->port = 8080;
        svc->txt = nd::mdns_txt_rdata({"path=/", "flag"});
        svc->subtypes.push_back(name_of("_printer._sub._http._tcp.local."));
        svc->state = mode & 2 ? nd::MdnsEntry::State::announced : nd::MdnsEntry::State::probing;
        e->entries.push_back(svc);
        return e;
    }

    void name_sound(const nd::DnsName& n) {
        check(!n.is_root());
        check(n.size <= nd::DnsMaxName);
        check(n.bytes[0] >= 1 && n.bytes[0] <= nd::DnsMaxLabel);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    uint8_t mode = data[0];
    bool legacy = mode & 4;
    nd::MdnsMessage m;
    if (!nd::mdns_read(data + 1, size - 1, m)) {
        return 0;
    }
    tracked_ptr<nd::MdnsEngine> e = engine(mode);
    std::lock_guard lock(e->m);
    if (m.is_response()) {
        nd::mdns_take(*e, m, 1);
        for (auto& c : e->cache) {
            check(c.r.ttl > 0 || c.expires <= sgcl::clock::now() + std::chrono::seconds(2));
        }
        std::vector<nd::MdnsQuestion> refresh;
        (void)nd::mdns_expire(*e, refresh);
    }
    nd::mdns_check_entries(*e, m, 0);
    for (auto& en : e->entries) {
        nd::DnsName own = en->name();
        if (en->conflict || en->lost) {
            bool named = false;
            for (auto* section : {&m.answers, &m.additionals, &m.authorities}) {
                for (auto& r : *section) {
                    named = named || r.name == own;
                }
            }
            check(named);
        }
        if (en->conflict) {
            for (int i = 0; i < 120; ++i) {
                en->rename();
                check(en->label.size() <= nd::DnsMaxLabel);
                name_sound(en->name());
            }
        }
    }
    if (m.is_response()) {
        return 0;
    }
    // the reply to the query, as the entries stand
    for (auto& en : e->entries) {
        en->conflict = false;
        en->lost = false;
    }
    nd::MdnsReply reply = nd::mdns_reply(*e, 0, m, legacy);
    for (size_t i = 0; i < reply.answers.size(); ++i) {
        auto& a = reply.answers[i];
        bool asked = false;
        for (auto& q : m.questions) {
            asked = asked || (q.name == a.name && (q.type == a.type || q.type == nd::dns_type::any));
        }
        check(asked);
        for (size_t j = 0; j < i; ++j) {
            check(!reply.answers[j].same(a));
        }
        for (auto& k : m.answers) {
            check(!(k.same(a) && k.ttl >= a.ttl / 2));
        }
        bool ours = false;
        for (auto& en : e->entries) {
            if (en->state != nd::MdnsEntry::State::announced) {
                continue;
            }
            for (auto& r : nd::mdns_entry_records(*en, e->links[0], false)) {
                ours = ours || r.same(a);
            }
        }
        check(ours);
    }
    for (auto& bytes : nd::mdns_reply_messages(m, reply, legacy)) {
        check(bytes.size() <= nd::MdnsSendLimit);
        nd::MdnsMessage back;
        check(nd::mdns_read(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), back));
        check(back.is_response());
        if (legacy) {
            check(back.header.id == m.header.id);
            check(back.questions.size() <= m.questions.size());
            for (auto* section : {&back.answers, &back.additionals}) {
                for (auto& r : *section) {
                    check(r.ttl <= nd::MdnsLegacyTtl);
                    check(!r.unique);
                }
            }
        } else {
            check(back.header.id == 0 && back.questions.empty());
        }
    }
    return 0;
}
