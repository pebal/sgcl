//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The multicast DNS message codec (sgcl/net/detail/mdns_message.h) on any
// bytes, without an oracle: the input is a message as a socket of the
// engine receives one. What must hold:
//   - nothing is read past the message (ASan); a message past 9000 bytes,
//     of another opcode, or with counts its bytes cannot hold is refused;
//   - every name read is sound (at most 255 bytes, labels of at most 63,
//     the root's zero at its end), the names inside a PTR's, SRV's, NS's
//     and CNAME's rdata among them, which come out uncompressed;
//   - a message read whole, written again by the writer (its names
//     compressed) and read again, gives the same questions (QU bit too;
//     the class IN, the only one the writer asks in) and the same records
//     (the class, the cache-flush bit, TTL, rdata byte for byte);
//   - the TXT strings of every TXT record split and their entries kept by
//     RFC 6763 §6.4; a record's data as the resolver takes it never faults.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/mdns_message_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/detail/mdns_engine.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void sound(const nd::DnsName& n) {
        check(n.size >= 1 && n.size <= nd::DnsMaxName);
        size_t i = 0;
        while (n.bytes[i] != 0) {
            check(n.bytes[i] <= nd::DnsMaxLabel);
            i += size_t(n.bytes[i]) + 1;
            check(i < n.size);
        }
        check(i + 1 == n.size);
    }

    void record_sound(const nd::MdnsRecord& r) {
        sound(r.name);
        nd::DnsName inner;
        if (r.type == nd::dns_type::ptr || r.type == nd::dns_type::ns || r.type == nd::dns_type::cname) {
            check(nd::mdns_rdata_name(r.rdata, 0, inner));
            sound(inner);
            check(inner.size == r.rdata.size());
        } else if (r.type == nd::dns_type::srv) {
            check(r.rdata.size() > 6 && nd::mdns_rdata_name(r.rdata, 6, inner));
            check(size_t(inner.size) + 6 == r.rdata.size());
        } else if (r.type == nd::dns_type::a) {
            check(r.rdata.size() == 4);
        } else if (r.type == nd::dns_type::aaaa) {
            check(r.rdata.size() == 16);
        } else if (r.type == nd::dns_type::txt) {
            std::vector<std::string> strings;
            if (nd::mdns_txt_strings(r.rdata, strings)) {
                auto entries = nd::mdns_txt_entries(strings);
                check(entries.size() <= strings.size());
                for (size_t i = 0; i < entries.size(); ++i) {
                    check(entries[i].size() <= 255);
                    check(nd::mdns_txt_key_valid(nd::mdns_txt_key(entries[i])));
                    for (size_t j = 0; j < i; ++j) {
                        check(!nd::mdns_txt_key_equal(nd::mdns_txt_key(entries[i]), nd::mdns_txt_key(entries[j])));
                    }
                }
            }
        }
        nd::DnsRecordData d;
        (void)nd::mdns_record_data(r, d);
        (void)nd::mdns_is_local(r.name);
    }

    bool same(const nd::MdnsRecord& a, const nd::MdnsRecord& b) {
        return a.same(b) && a.ttl == b.ttl && a.unique == b.unique;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    nd::MdnsMessage m;
    if (!nd::mdns_read(data, size, m)) {
        return 0;
    }
    check(size >= nd::DnsHeaderSize && size <= nd::MdnsMaxMessage);
    check(m.questions.size() == m.header.questions);
    check(m.answers.size() == m.header.answers);
    for (auto& q : m.questions) {
        sound(q.name);
        check((q.klass & nd::MdnsClassTopBit) == 0);
    }
    for (auto* section : {&m.answers, &m.authorities, &m.additionals}) {
        for (auto& r : *section) {
            record_sound(r);
        }
    }
    // written again and read again: the same message
    std::vector<uint8_t> buf(size_t(1) << 20);
    nd::MdnsWriter w(buf.data(), buf.size(), m.header.id, m.header.flags);
    for (auto& q : m.questions) {
        check(w.question(q.name, q.type, q.unicast));
    }
    int s = 0;
    for (auto* section : {&m.answers, &m.authorities, &m.additionals}) {
        for (auto& r : *section) {
            check(w.record(s, r, r.ttl, r.unique));
        }
        ++s;
    }
    size_t n = w.finish();
    check(n >= nd::DnsHeaderSize);
    nd::MdnsMessage again;
    if (n <= nd::MdnsMaxMessage) {
        check(nd::mdns_read(buf.data(), n, again));
        check(again.questions.size() == m.questions.size());
        for (size_t i = 0; i < m.questions.size(); ++i) {
            check(again.questions[i].name == m.questions[i].name && again.questions[i].type == m.questions[i].type);
            check(again.questions[i].unicast == m.questions[i].unicast);
            check(again.questions[i].klass == nd::DnsClassIn);   // the writer asks in class IN, whatever the message read had
        }
        const std::vector<nd::MdnsRecord>* a[3] = {&m.answers, &m.authorities, &m.additionals};
        const std::vector<nd::MdnsRecord>* b[3] = {&again.answers, &again.authorities, &again.additionals};
        for (int k = 0; k < 3; ++k) {
            check(a[k]->size() == b[k]->size());
            for (size_t i = 0; i < a[k]->size(); ++i) {
                check(same((*a[k])[i], (*b[k])[i]));
            }
        }
    }
    return 0;
}
