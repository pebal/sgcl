//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "dns_message.h"

#include <string>
#include <vector>

// Multicast DNS messages (RFC 6762 §18) over the codec of dns_message.h:
// a whole message read into its questions and records, every record's
// rdata in its canonical form (the names inside it uncompressed, RFC 6762
// §18.14, so that two records compare by their bytes, §8.2), and messages
// written from records. The top bit of a question's class asks for a
// unicast response (QU, §5.4), of a record's class it is the cache-flush
// bit (§10.2); both are taken apart from the class here.
//
// And the TXT record of DNS-SD (RFC 6763 §6): strings of at most 255
// bytes, each "key=value" or "key" alone; read into its strings, the
// empty TXT being one string of nothing (§6.1).
namespace sgcl::net::detail {
    inline constexpr uint16_t MdnsPort = 5353;
    inline constexpr uint16_t MdnsClassTopBit = 0x8000;
    inline constexpr size_t MdnsMaxMessage = 9000;      // §17: a message on the wire, at most
    inline constexpr size_t MdnsSendLimit = 1440;       // §17: what one sent message is kept under (an Ethernet MTU)
    inline constexpr uint32_t MdnsHostTtl = 120;        // §10: A, AAAA, SRV and the records of the host
    inline constexpr uint32_t MdnsOtherTtl = 4500;      // §10: the rest, 75 minutes
    inline constexpr uint32_t MdnsLegacyTtl = 10;       // §6.7: the most a legacy unicast answer says

    struct MdnsQuestion {
        DnsName name;
        uint16_t type = 0;
        uint16_t klass = DnsClassIn;
        bool unicast = false;   // QU
    };

    struct MdnsRecord {
        DnsName name;
        uint16_t type = 0;
        uint16_t klass = DnsClassIn;
        bool unique = false;    // the cache-flush bit
        uint32_t ttl = 0;
        std::string rdata;      // canonical: names uncompressed, in the case they came in

        // The same record: the name without regard to case, the type,
        // the class, the rdata byte for byte
        bool same(const MdnsRecord& o) const noexcept {
            return type == o.type && klass == o.klass && rdata == o.rdata && name == o.name;
        }
    };

    struct MdnsMessage {
        DnsHeader header;
        std::vector<MdnsQuestion> questions;
        std::vector<MdnsRecord> answers;
        std::vector<MdnsRecord> authorities;
        std::vector<MdnsRecord> additionals;

        SGCL_INLINE_HOT bool is_response() const noexcept {
            return (header.flags & DnsFlagResponse) != 0;
        }
    };

    // A name in wire form as the bytes of a string, for rdata
    SGCL_INLINE_HOT void mdns_append_name(std::string& out, const DnsName& n) {
        out.append(reinterpret_cast<const char*>(n.bytes), n.size);
    }

    SGCL_INLINE_HOT void mdns_append_u16(std::string& out, uint16_t v) {
        out += char(v >> 8);
        out += char(v & 0xFF);
    }

    // The first name of an rdata in canonical form (PTR, CNAME, NS: the
    // whole of it; SRV: after its six bytes); false when it holds none
    inline bool mdns_rdata_name(const std::string& rdata, size_t at, DnsName& out) noexcept {
        DnsReader r(reinterpret_cast<const uint8_t*>(rdata.data()), rdata.size());
        size_t p = at;
        return at < rdata.size() && r.name_at(p, out);
    }

    // A record's rdata at its place in a message, made canonical: the
    // names of the types that hold them read whole (pointers followed),
    // anything else as it is. False when it does not read
    inline bool mdns_canonical_rdata(const DnsReader& r, const DnsReader::Record& rec, std::string& out) {
        out.clear();
        const uint8_t* m = r.data();
        switch (rec.type) {
            case dns_type::ptr:
            case dns_type::cname:
            case dns_type::ns: {
                DnsName n;
                if (!dns_read_name_rdata(r, rec, n)) {
                    return false;
                }
                mdns_append_name(out, n);
                return true;
            }
            case dns_type::srv: {
                DnsSrv s;
                if (!dns_read_srv(r, rec, s)) {
                    return false;
                }
                out.append(reinterpret_cast<const char*>(m + rec.rdata), 6);
                mdns_append_name(out, s.target);
                return true;
            }
            case dns_type::mx: {
                DnsMx x;
                if (!dns_read_mx(r, rec, x)) {
                    return false;
                }
                out.append(reinterpret_cast<const char*>(m + rec.rdata), 2);
                mdns_append_name(out, x.host);
                return true;
            }
            case dns_type::nsec: {
                // the next name (RFC 4034 §4.1; RFC 6762 §6.1: never
                // compressed by an mDNS sender, read all the same) and the
                // type bitmaps as they are
                size_t p = rec.rdata;
                DnsName next;
                if (!r.name_at(p, next) || p > rec.rdata + rec.length) {
                    return false;
                }
                mdns_append_name(out, next);
                out.append(reinterpret_cast<const char*>(m + p), rec.rdata + rec.length - p);
                return true;
            }
            case dns_type::a:
                if (rec.length != 4) {
                    return false;
                }
                break;
            case dns_type::aaaa:
                if (rec.length != 16) {
                    return false;
                }
                break;
            default:
                break;
        }
        out.append(reinterpret_cast<const char*>(m + rec.rdata), rec.length);
        return true;
    }

    // A whole message read; false when it does not (anything left after
    // the counts is ignored, as §18 lets a receiver). A message reads in
    // one pass over its bytes, its counts bounded by them
    inline bool mdns_read(const uint8_t* message, size_t size, MdnsMessage& out) {
        out.questions.clear();
        out.answers.clear();
        out.authorities.clear();
        out.additionals.clear();
        if (size > MdnsMaxMessage) {
            return false;
        }
        DnsReader r(message, size);
        if (!r.header(out.header)) {
            return false;
        }
        if (out.header.opcode() != 0) {
            return false;   // §18.3: a message of another opcode is silently ignored
        }
        size_t room = size - DnsHeaderSize;
        if (size_t(out.header.questions) * 5 > room || (size_t(out.header.answers) + out.header.authorities + out.header.additionals) * 11 > room) {
            return false;   // more than the bytes can hold: a question is 5 bytes at least, a record 11
        }
        out.questions.reserve(out.header.questions);
        for (size_t i = 0; i < out.header.questions; ++i) {
            MdnsQuestion q;
            uint16_t klass = 0;
            if (!r.question(q.name, q.type, klass)) {
                return false;
            }
            q.unicast = (klass & MdnsClassTopBit) != 0;
            q.klass = uint16_t(klass & ~MdnsClassTopBit);
            out.questions.push_back(q);
        }
        std::vector<MdnsRecord>* sections[3] = {&out.answers, &out.authorities, &out.additionals};
        size_t counts[3] = {out.header.answers, out.header.authorities, out.header.additionals};
        for (int s = 0; s < 3; ++s) {
            sections[s]->reserve(counts[s]);
            for (size_t i = 0; i < counts[s]; ++i) {
                DnsReader::Record rec;
                if (!r.record(rec)) {
                    return false;
                }
                MdnsRecord m;
                m.name = rec.name;
                m.type = rec.type;
                m.ttl = rec.ttl;
                if (rec.type == dns_type::opt) {
                    m.klass = rec.klass;   // OPT's class is a payload size, no bit to take apart
                } else {
                    m.unique = (rec.klass & MdnsClassTopBit) != 0;
                    m.klass = uint16_t(rec.klass & ~MdnsClassTopBit);
                }
                if (!mdns_canonical_rdata(r, rec, m.rdata)) {
                    return false;
                }
                sections[s]->push_back(std::move(m));
            }
        }
        return true;
    }

    // A message written: its header (the id and flags given, the counts
    // as written), the questions, then records; each part says whether it
    // fitted, and the message is cut at the last that did
    class MdnsWriter {
    public:
        SGCL_INLINE_HOT MdnsWriter(uint8_t* buffer, size_t capacity, uint16_t id, uint16_t flags) noexcept
        : _w(buffer, capacity) {
            _h.id = id;
            _h.flags = flags;
            _w.header(_h);
        }

        bool question(const DnsName& name, uint16_t type, bool unicast) noexcept {
            if (_records) {
                return false;   // the questions come first
            }
            size_t before = _w.size();
            if (!_w.question(name, type, uint16_t(DnsClassIn | (unicast ? MdnsClassTopBit : 0)), true)) {
                _cut(before);
                return false;
            }
            ++_h.questions;
            return true;
        }

        // A record into a section (0 answers, 1 authorities, 2 additionals,
        // in that order); its ttl as given, the cache-flush bit when asked
        bool record(int section, const MdnsRecord& r, uint32_t ttl, bool cache_flush) noexcept {
            if (section < _section) {
                return false;
            }
            _section = section;
            size_t before = _w.size();
            uint16_t klass = uint16_t(r.klass | (cache_flush ? MdnsClassTopBit : 0));
            bool ok = _w.record_begin(r.name, r.type, klass, ttl, true);
            if (ok) {
                switch (r.type) {
                    case dns_type::ptr:
                    case dns_type::cname:
                    case dns_type::ns: {
                        DnsName n;
                        ok = mdns_rdata_name(r.rdata, 0, n) && _w.name(n, true);
                        break;
                    }
                    case dns_type::srv: {
                        DnsName n;
                        // RFC 2782 forbids compressing SRV's target, RFC 6762 §18.14 allows it; uncompressed, as Go and the RFC 2782 readers take it
                        ok = r.rdata.size() > 6 && _w.bytes(r.rdata.data(), 6) && mdns_rdata_name(r.rdata, 6, n) && _w.name(n, false);
                        break;
                    }
                    default:
                        ok = _w.bytes(r.rdata.data(), r.rdata.size());
                }
            }
            ok = ok && _w.record_end();
            if (!ok) {
                _cut(before);
                return false;
            }
            ++_records;
            if (section == 0) {
                ++_h.answers;
            } else if (section == 1) {
                ++_h.authorities;
            } else {
                ++_h.additionals;
            }
            return true;
        }

        // The message's size with its counts written, 0 when not even its
        // header fitted
        size_t finish() noexcept {
            _w.set_header(_h);
            return _w.size() >= DnsHeaderSize ? _w.size() : 0;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _w.size();
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _h.questions == 0 && _records == 0;
        }

    private:
        // back to `size` after a part that did not fit, so that the parts
        // after it may still go in
        void _cut(size_t size) noexcept {
            _w.truncate(size);
        }

        DnsWriter _w;
        DnsHeader _h;
        size_t _records = 0;
        int _section = 0;
    };

    // The strings of a TXT rdata; false when they do not fill it exactly.
    // An rdata of nothing is taken as the empty TXT (§6.1 asks a sender
    // for one empty string)
    inline bool mdns_txt_strings(const std::string& rdata, std::vector<std::string>& out) {
        out.clear();
        size_t p = 0;
        while (p < rdata.size()) {
            size_t len = uint8_t(rdata[p]);
            if (rdata.size() - p - 1 < len) {
                return false;
            }
            out.emplace_back(rdata, p + 1, len);
            p += len + 1;
        }
        return true;
    }

    // RFC 6763 §6.4: a key is at least one printable US-ASCII character
    // (0x20-0x7E) other than '='
    inline bool mdns_txt_key_valid(std::string_view key) noexcept {
        if (key.empty()) {
            return false;
        }
        for (char c : key) {
            if (uint8_t(c) < 0x20 || uint8_t(c) > 0x7E || c == '=') {
                return false;
            }
        }
        return true;
    }

    // Two keys the same without regard to the case of ASCII letters (§6.4)
    inline bool mdns_txt_key_equal(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i) {
            char x = a[i], y = b[i];
            if (x >= 'A' && x <= 'Z') {
                x = char(x + 32);
            }
            if (y >= 'A' && y <= 'Z') {
                y = char(y + 32);
            }
            if (x != y) {
                return false;
            }
        }
        return true;
    }

    // The key of an entry: what stands before its first '=', all of it
    // without one
    SGCL_INLINE_HOT std::string_view mdns_txt_key(std::string_view entry) noexcept {
        size_t eq = entry.find('=');
        return eq == std::string_view::npos ? entry : entry.substr(0, eq);
    }

    // The entries a TXT record's strings hold by §6.4: an empty string,
    // one that starts with '=' and the later ones of a key seen are
    // dropped; a key that is not printable ASCII drops its string too
    inline std::vector<std::string> mdns_txt_entries(const std::vector<std::string>& strings) {
        std::vector<std::string> out;
        for (auto& s : strings) {
            std::string_view key = mdns_txt_key(s);
            if (!mdns_txt_key_valid(key)) {
                continue;
            }
            bool seen = false;
            for (auto& o : out) {
                if (mdns_txt_key_equal(mdns_txt_key(o), key)) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                out.push_back(s);
            }
        }
        return out;
    }

    // The rdata of entries: each a string after its length; one empty
    // string when there are none (§6.1)
    inline std::string mdns_txt_rdata(const std::vector<std::string>& entries) {
        std::string out;
        if (entries.empty()) {
            out += '\0';
            return out;
        }
        for (auto& e : entries) {
            out += char(uint8_t(e.size()));
            out += e;
        }
        return out;
    }
}
