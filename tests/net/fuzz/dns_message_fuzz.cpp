//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The DNS codec (sgcl/net/detail/dns_message.h) on any bytes, without an
// oracle: the input is a message, read as the resolver reads one. What
// must hold:
//   - nothing is read past the message (ASan), no pointer loop runs on;
//   - every name read is at most 255 bytes, ends with the root's zero, has
//     labels of at most 63, and its presentation text reads back to the
//     same bytes;
//   - a message whose every section reads, written again by the writer
//     (names compressed, and not) and read again, gives the same records:
//     owners, types, classes, TTLs and rdata, the names in it decompressed;
//   - the reading of an answer (dns_read_answer, as UDP and as TCP) for the
//     message's own id and question: ok only with records, none otherwise.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/dns_message_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/detail/dns_message.h"

#include <cstdint>
#include <string>
#include <vector>

namespace {
    namespace nd = sgcl::net::detail;
    namespace type = sgcl::net::detail::dns_type;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    std::string bytes_of(const nd::DnsName& n) {
        return std::string(reinterpret_cast<const char*>(n.bytes), n.size);
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
        char text[nd::DnsMaxNameText];
        size_t len = nd::dns_name_to_text(n, text);
        check(len <= nd::DnsMaxNameText);
        nd::DnsName back;
        nd::DnsNameText info;
        check(nd::dns_name_from_text(std::string_view(text, len), back, &info));
        check(info.rooted);
        check(bytes_of(back) == bytes_of(n));
        check(back == n);
    }

    // A record as read: its fixed part, and its rdata with the names in it
    // decompressed, so that two writings of it compare equal
    struct Read {
        std::string owner;
        uint16_t type = 0, klass = 0;
        uint32_t ttl = 0;
        std::string rdata;
        bool names = false;   // a type whose rdata has names
        std::vector<nd::DnsName> parts;
        std::vector<uint32_t> numbers;
        std::string raw;
    };

    bool read_record(const nd::DnsReader& r, const nd::DnsReader::Record& rec, Read& out) {
        sound(rec.name);
        out.owner = bytes_of(rec.name);
        out.type = rec.type;
        out.klass = rec.klass;
        out.ttl = rec.ttl;
        switch (rec.type) {
            case type::mx: {
                nd::DnsMx mx;
                if (!nd::dns_read_mx(r, rec, mx)) {
                    return false;
                }
                sound(mx.host);
                out.names = true;
                out.numbers = {mx.preference};
                out.parts = {mx.host};
                break;
            }
            case type::srv: {
                nd::DnsSrv s;
                if (!nd::dns_read_srv(r, rec, s)) {
                    return false;
                }
                sound(s.target);
                out.names = true;
                out.numbers = {s.priority, s.weight, s.port};
                out.parts = {s.target};
                break;
            }
            case type::ns:
            case type::cname:
            case type::ptr: {
                nd::DnsName n;
                if (!nd::dns_read_name_rdata(r, rec, n)) {
                    return false;
                }
                sound(n);
                out.names = true;
                out.parts = {n};
                break;
            }
            case type::soa: {
                nd::DnsSoa s;
                if (!nd::dns_read_soa(r, rec, s)) {
                    return false;
                }
                sound(s.mname);
                sound(s.rname);
                out.names = true;
                out.parts = {s.mname, s.rname};
                out.numbers = {s.serial, s.refresh, s.retry, s.expire, s.minimum};
                break;
            }
            case type::txt: {
                sgcl::string text;
                size_t total = 0;
                bool ok = nd::dns_txt_size(r, rec, total);
                check(ok == nd::dns_read_txt(r, rec, text));
                if (!ok) {
                    return false;
                }
                check(text.size() == total && total < rec.length + 1);
                out.raw.assign(reinterpret_cast<const char*>(r.data() + rec.rdata), rec.length);
                break;
            }
            default:
                out.raw.assign(reinterpret_cast<const char*>(r.data() + rec.rdata), rec.length);
        }
        return true;
    }

    bool same(const Read& a, const Read& b) {
        if (a.owner != b.owner || a.type != b.type || a.klass != b.klass || a.ttl != b.ttl || a.names != b.names || a.numbers != b.numbers || a.raw != b.raw || a.parts.size() != b.parts.size()) {
            return false;
        }
        for (size_t i = 0; i < a.parts.size(); ++i) {
            if (bytes_of(a.parts[i]) != bytes_of(b.parts[i])) {
                return false;
            }
        }
        return true;
    }

    struct Message {
        nd::DnsHeader h;
        std::vector<nd::DnsName> qnames;
        std::vector<uint16_t> qtypes, qclasses;
        std::vector<Read> records;
    };

    // The whole message, or false at its first fault
    bool read_all(const uint8_t* data, size_t size, Message& m) {
        nd::DnsReader r(data, size);
        if (!r.header(m.h)) {
            return false;
        }
        for (size_t i = 0; i < m.h.questions; ++i) {
            nd::DnsName n;
            uint16_t t = 0, c = 0;
            if (!r.question(n, t, c)) {
                return false;
            }
            sound(n);
            m.qnames.push_back(n);
            m.qtypes.push_back(t);
            m.qclasses.push_back(c);
        }
        size_t total = size_t(m.h.answers) + m.h.authorities + m.h.additionals;
        for (size_t i = 0; i < total; ++i) {
            nd::DnsReader::Record rec;
            if (!r.record(rec)) {
                return false;
            }
            check(rec.rdata + rec.length <= size);
            Read one;
            if (!read_record(r, rec, one)) {
                return false;
            }
            m.records.push_back(std::move(one));
        }
        check(r.position() <= size);
        return true;
    }

    std::vector<uint8_t> write_all(const Message& m, bool compress) {
        // room for every name written whole: a record of 12 bytes may be
        // one of more than 800 decompressed
        std::vector<uint8_t> out(4096 + m.qnames.size() * 260 + m.records.size() * 800);
        nd::DnsWriter w(out.data(), out.size());
        w.header(m.h);
        for (size_t i = 0; i < m.qnames.size(); ++i) {
            w.question(m.qnames[i], m.qtypes[i], m.qclasses[i], compress);
        }
        for (auto& rec : m.records) {
            nd::DnsName owner;
            owner.size = uint16_t(rec.owner.size());
            for (size_t k = 0; k < rec.owner.size(); ++k) {
                owner.bytes[k] = uint8_t(rec.owner[k]);
            }
            w.record_begin(owner, rec.type, rec.klass, rec.ttl, compress);
            if (rec.type == type::mx) {
                w.u16(uint16_t(rec.numbers[0]));
                w.name(rec.parts[0], compress);
            } else if (rec.type == type::srv) {
                w.u16(uint16_t(rec.numbers[0]));
                w.u16(uint16_t(rec.numbers[1]));
                w.u16(uint16_t(rec.numbers[2]));
                w.name(rec.parts[0], compress);
            } else if (rec.type == type::soa) {
                w.name(rec.parts[0], compress);
                w.name(rec.parts[1], compress);
                for (uint32_t v : rec.numbers) {
                    w.u32(v);
                }
            } else if (rec.names) {
                w.name(rec.parts[0], compress);
            } else {
                w.bytes(rec.raw.data(), rec.raw.size());
            }
            w.record_end();
        }
        check(w.ok());
        out.resize(w.size());
        return out;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 65535) {
        return 0;
    }
    Message m;
    bool whole = read_all(data, size, m);
    // the answer to the message's own question, as the resolver reads it
    if (!m.qnames.empty()) {
        for (bool tcp : {false, true}) {
            nd::DnsAnswer a;
            auto st = nd::dns_read_answer(data, size, m.h.id, m.qnames[0], m.qtypes[0], tcp, a);
            check(a.status == st);
            check((st == nd::DnsStatus::ok) == !a.records.empty());
            check(a.hops <= nd::DnsMaxHops);
            sound(a.end);
            if (st == nd::DnsStatus::truncated) {
                check(!tcp);
            }
        }
    }
    if (!whole) {
        return 0;
    }
    for (bool compress : {false, true}) {
        auto again = write_all(m, compress);
        Message n;
        check(read_all(again.data(), again.size(), n));
        check(n.records.size() == m.records.size());
        check(n.qnames.size() == m.qnames.size());
        for (size_t i = 0; i < m.qnames.size(); ++i) {
            check(bytes_of(n.qnames[i]) == bytes_of(m.qnames[i]));
            check(n.qtypes[i] == m.qtypes[i] && n.qclasses[i] == m.qclasses[i]);
        }
        for (size_t i = 0; i < m.records.size(); ++i) {
            check(same(m.records[i], n.records[i]));
        }
        if (!compress) {
            check(again.size() >= nd::DnsHeaderSize);
        }
    }
    return 0;
}
