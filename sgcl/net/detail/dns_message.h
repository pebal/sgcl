//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"
#include "../../core/aliases.h"
#include "../../core/string.h"
#include "../../core/vector.h"
#include "../../io/error.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// DNS messages, RFC 1035 §4: the header, the question, the resource
// records, the names with their compression (§4.1.4), written and read in
// place in a buffer the caller owns, with no allocation. The record types
// the stub resolver asks for (A, NS, CNAME, SOA, MX, TXT, AAAA, SRV of
// RFC 2782, OPT of RFC 6891) are known by number; the reader gives every
// record's place in the message, and the rdata of a known type is read
// from there.
//
// A name read is checked whole: labels of at most 63 bytes (the types
// 0b01 and 0b10 of the first two bits refused, RFC 6891 §5), at most 255
// bytes with the root's zero (§2.3.4), and every compression pointer
// pointing before the place where the run of labels that holds it began,
// which ends every loop: the place a name is read from only goes back.
// A pointer to a prior occurrence, the only kind §4.1.4 makes, always
// does.
namespace sgcl::net::detail {
    using namespace sgcl::detail;

    namespace dns_type {
        inline constexpr uint16_t a = 1;
        inline constexpr uint16_t ns = 2;
        inline constexpr uint16_t cname = 5;
        inline constexpr uint16_t soa = 6;
        inline constexpr uint16_t ptr = 12;
        inline constexpr uint16_t mx = 15;
        inline constexpr uint16_t txt = 16;
        inline constexpr uint16_t aaaa = 28;
        inline constexpr uint16_t srv = 33;
        inline constexpr uint16_t opt = 41;
        inline constexpr uint16_t nsec = 47;
        inline constexpr uint16_t any = 255;
    }

    namespace dns_rcode {
        inline constexpr int no_error = 0;
        inline constexpr int format_error = 1;
        inline constexpr int server_failure = 2;
        inline constexpr int name_error = 3;   // NXDOMAIN
        inline constexpr int not_implemented = 4;
        inline constexpr int refused = 5;
    }

    inline constexpr uint16_t DnsClassIn = 1;
    inline constexpr size_t DnsMaxName = 255;          // wire bytes, the root's zero counted
    inline constexpr size_t DnsMaxLabel = 63;
    inline constexpr size_t DnsMaxNameText = 1024;     // presentation form: every byte \DDD, the dots, the root
    inline constexpr uint16_t DnsUdpPayload = 1232;    // RFC 6891 §6.2.5, the size of DNS Flag Day 2020
    inline constexpr size_t DnsHeaderSize = 12;
    inline constexpr uint16_t DnsOptionPadding = 12;  // RFC 7830
    inline constexpr size_t DnsQueryPadBlock = 128;    // RFC 8467 §4.1: a query padded to a multiple of 128 bytes

    // The flags of the header's second word
    inline constexpr uint16_t DnsFlagResponse = 0x8000;
    inline constexpr uint16_t DnsFlagAuthoritative = 0x0400;
    inline constexpr uint16_t DnsFlagTruncated = 0x0200;
    inline constexpr uint16_t DnsFlagRecursionDesired = 0x0100;
    inline constexpr uint16_t DnsFlagRecursionAvailable = 0x0080;

    struct DnsHeader {
        uint16_t id = 0;
        uint16_t flags = 0;
        uint16_t questions = 0;
        uint16_t answers = 0;
        uint16_t authorities = 0;
        uint16_t additionals = 0;

        SGCL_INLINE_HOT int opcode() const noexcept {
            return (flags >> 11) & 0xF;
        }

        SGCL_INLINE_HOT int rcode() const noexcept {
            return flags & 0xF;
        }
    };

    // A name in wire form: its labels, each after its length, and the
    // root's zero; at most 255 bytes. Compared without regard to the case
    // of ASCII letters (RFC 4343): a length byte is below 64, never a
    // letter, so the bytes fold as one run
    struct DnsName {
        uint8_t bytes[DnsMaxName];   // the first `size` meaningful, the rest never read
        uint16_t size = 1;           // the root

        SGCL_INLINE_HOT DnsName() noexcept {
            bytes[0] = 0;
        }

        SGCL_INLINE_HOT DnsName(const DnsName& o) noexcept
        : size(o.size) {
            copy_bytes(bytes, o.bytes, o.size);
        }

        SGCL_INLINE_HOT DnsName& operator=(const DnsName& o) noexcept {
            if (this != &o) {
                size = o.size;
                copy_bytes(bytes, o.bytes, o.size);
            }
            return *this;
        }

        SGCL_INLINE_HOT bool is_root() const noexcept {
            return size == 1;
        }

        // The labels before the root's zero
        SGCL_INLINE_HOT size_t labels() const noexcept {
            size_t n = 0;
            for (size_t i = 0; bytes[i] != 0; i += size_t(bytes[i]) + 1) {
                ++n;
            }
            return n;
        }

        friend bool operator==(const DnsName& a, const DnsName& b) noexcept {
            if (a.size != b.size) {
                return false;
            }
            uint8_t diff = 0;
            for (size_t i = 0; i < a.size; ++i) {
                uint8_t x = a.bytes[i], y = b.bytes[i];
                x = uint8_t(x | (uint8_t(x - 'A') < 26 ? 0x20 : 0));
                y = uint8_t(y | (uint8_t(y - 'A') < 26 ? 0x20 : 0));
                diff = uint8_t(diff | (x ^ y));
            }
            return diff == 0;
        }
    };

    // What a name's text says beside its labels: whether it ended with a
    // dot (absolute: no search domain is tried), and its dots before that
    // (for ndots)
    struct DnsNameText {
        bool rooted = false;
        int dots = 0;
    };

    // A name's text in its wire form, the escapes of RFC 1035 §5.1 read:
    // "\." a dot inside a label, "\\" a backslash, "\DDD" the byte of
    // that decimal value (at most 255), "\X" any other X as itself. "."
    // alone is the root. False for the empty text, an empty label (two
    // dots, a leading dot), a label past 63 bytes, a name past 255, an
    // escape cut short or past 255
    inline bool dns_name_from_text(std::string_view text, DnsName& out, DnsNameText* info = nullptr) noexcept {
        out.size = 1;
        out.bytes[0] = 0;
        if (text.empty()) {
            return false;
        }
        DnsNameText t;
        if (text == ".") {
            t.rooted = true;
            if (info) {
                *info = t;
            }
            return true;
        }
        size_t at = 0;            // the length byte of the label being written
        size_t w = 1;             // the next byte to write
        size_t i = 0, n = text.size();
        while (i < n) {
            char c = text[i];
            if (c == '.') {
                size_t len = w - at - 1;
                if (len == 0) {
                    return false;
                }
                out.bytes[at] = uint8_t(len);
                ++i;
                if (i == n) {
                    t.rooted = true;
                    break;
                }
                ++t.dots;
                at = w;
                if (w >= DnsMaxName - 1) {
                    return false;
                }
                ++w;
                continue;
            }
            uint8_t b = uint8_t(c);
            ++i;
            if (c == '\\') {
                if (i == n) {
                    return false;
                }
                char d = text[i];
                if (d >= '0' && d <= '9') {
                    if (n - i < 3 || text[i + 1] < '0' || text[i + 1] > '9' || text[i + 2] < '0' || text[i + 2] > '9') {
                        return false;
                    }
                    int v = (d - '0') * 100 + (text[i + 1] - '0') * 10 + (text[i + 2] - '0');
                    if (v > 255) {
                        return false;
                    }
                    b = uint8_t(v);
                    i += 3;
                } else {
                    b = uint8_t(d);
                    ++i;
                }
            }
            if (w - at - 1 >= DnsMaxLabel || w >= DnsMaxName - 1) {   // the label, or the name with its root's zero, full
                return false;
            }
            out.bytes[w++] = b;
        }
        if (!t.rooted) {
            size_t len = w - at - 1;
            if (len == 0) {
                return false;
            }
            out.bytes[at] = uint8_t(len);
        }
        out.bytes[w++] = 0;
        out.size = uint16_t(w);
        if (info) {
            *info = t;
        }
        return true;
    }

    // The presentation form (RFC 1035 §5.1): the labels joined by dots,
    // a dot after the last, "." for the root; a dot or a backslash inside
    // a label escaped with a backslash, a byte outside the printable ASCII
    // (0x21-0x7E) as \DDD. At most DnsMaxNameText characters into `out`;
    // their count
    inline size_t dns_name_to_text(const DnsName& name, char* out) noexcept {
        if (name.is_root()) {
            out[0] = '.';
            return 1;
        }
        size_t w = 0;
        size_t i = 0;
        while (name.bytes[i] != 0) {
            size_t len = name.bytes[i++];
            for (size_t k = 0; k < len; ++k) {
                uint8_t b = name.bytes[i++];
                if (b == '.' || b == '\\') {
                    out[w++] = '\\';
                    out[w++] = char(b);
                } else if (b < 0x21 || b > 0x7E) {
                    out[w++] = '\\';
                    out[w++] = char('0' + b / 100);
                    out[w++] = char('0' + b / 10 % 10);
                    out[w++] = char('0' + b % 10);
                } else {
                    out[w++] = char(b);
                }
            }
            out[w++] = '.';
        }
        return w;
    }

    inline string dns_name_string(const DnsName& name) noexcept {
        char text[DnsMaxNameText];
        size_t n = dns_name_to_text(name, text);
        return string(std::string_view(text, n));
    }

    // `name` with `suffix` after its labels, both in wire form: false
    // when the result would pass 255 bytes
    inline bool dns_name_join(const DnsName& name, const DnsName& suffix, DnsName& out) noexcept {
        size_t head = size_t(name.size) - 1;
        if (head + suffix.size > DnsMaxName) {
            return false;
        }
        copy_bytes(out.bytes, name.bytes, head);
        copy_bytes(out.bytes + head, suffix.bytes, suffix.size);
        out.size = uint16_t(head + suffix.size);
        return true;
    }

    // A message read in place. The reader's position moves through the
    // sections; the rdata of a record is read at its place, where a name
    // in it may point anywhere before
    class DnsReader {
    public:
        // A record's fixed part and where its rdata is
        struct Record {
            DnsName name;
            uint16_t type = 0;
            uint16_t klass = 0;
            uint32_t ttl = 0;
            size_t rdata = 0;     // the rdata's offset in the message
            size_t length = 0;    // its length
        };

        SGCL_INLINE_HOT DnsReader(const uint8_t* message, size_t size) noexcept
        : _m(message)
        , _size(size) {
        }

        SGCL_INLINE_HOT size_t position() const noexcept {
            return _pos;
        }

        // Back to a position the reader has passed (a section's start)
        SGCL_INLINE_HOT void seek(size_t pos) noexcept {
            _pos = pos < _size ? pos : _size;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT const uint8_t* data() const noexcept {
            return _m;
        }

        // The twelve bytes at the start
        bool header(DnsHeader& h) noexcept {
            if (_size < DnsHeaderSize) {
                return false;
            }
            h.id = _u16(0);
            h.flags = _u16(2);
            h.questions = _u16(4);
            h.answers = _u16(6);
            h.authorities = _u16(8);
            h.additionals = _u16(10);
            _pos = DnsHeaderSize;
            return true;
        }

        bool question(DnsName& name, uint16_t& type, uint16_t& klass) noexcept {
            if (!name_at(_pos, name) || _size - _pos < 4) {
                return false;
            }
            type = _u16(_pos);
            klass = _u16(_pos + 2);
            _pos += 4;
            return true;
        }

        bool record(Record& r) noexcept {
            if (!name_at(_pos, r.name) || _size - _pos < 10) {
                return false;
            }
            r.type = _u16(_pos);
            r.klass = _u16(_pos + 2);
            r.ttl = uint32_t(_u16(_pos + 4)) << 16 | _u16(_pos + 6);
            r.length = _u16(_pos + 8);
            r.rdata = _pos + 10;
            if (_size - r.rdata < r.length) {
                return false;
            }
            _pos = r.rdata + r.length;
            return true;
        }

        // The name at `pos`, which moves past it (past its first pointer,
        // when it has one)
        bool name_at(size_t& pos, DnsName& out) const noexcept {
            size_t p = pos;
            size_t limit = pos;   // a pointer goes before this, which only goes back
            size_t after = 0;
            bool jumped = false;
            size_t w = 0;
            for (;;) {
                if (p >= _size) {
                    return false;
                }
                uint8_t len = _m[p];
                if (len == 0) {
                    out.bytes[w++] = 0;
                    out.size = uint16_t(w);
                    pos = jumped ? after : p + 1;
                    return true;
                }
                if ((len & 0xC0) == 0xC0) {
                    if (p + 1 >= _size) {
                        return false;
                    }
                    size_t target = size_t(len & 0x3F) << 8 | _m[p + 1];
                    if (target >= limit) {
                        return false;
                    }
                    if (!jumped) {
                        after = p + 2;
                        jumped = true;
                    }
                    limit = target;
                    p = target;
                    continue;
                }
                if (len & 0xC0) {
                    return false;   // 0b01 and 0b10: no such label types
                }
                if (_size - p - 1 < len || w + len + 2 > DnsMaxName) {   // the label and, after it, at least the root's zero
                    return false;
                }
                copy_bytes(out.bytes + w, _m + p, size_t(len) + 1);
                w += size_t(len) + 1;
                p += size_t(len) + 1;
            }
        }

        // The rdata's numbers
        SGCL_INLINE_HOT uint16_t u16_at(size_t pos) const noexcept {
            return _u16(pos);
        }

        SGCL_INLINE_HOT uint32_t u32_at(size_t pos) const noexcept {
            return uint32_t(_u16(pos)) << 16 | _u16(pos + 2);
        }

    private:
        SGCL_INLINE_HOT uint16_t _u16(size_t at) const noexcept {
            return uint16_t(uint16_t(_m[at]) << 8 | _m[at + 1]);
        }

        const uint8_t* _m;
        size_t _size;
        size_t _pos = 0;
    };

    // The rdata of the record types the resolver reads, each checked to
    // fill its record exactly
    struct DnsMx {
        uint16_t preference = 0;
        DnsName host;
    };

    struct DnsSrv {
        uint16_t priority = 0;
        uint16_t weight = 0;
        uint16_t port = 0;
        DnsName target;
    };

    struct DnsSoa {
        DnsName mname;
        DnsName rname;
        uint32_t serial = 0;
        uint32_t refresh = 0;
        uint32_t retry = 0;
        uint32_t expire = 0;
        uint32_t minimum = 0;
    };

    // NS, CNAME, PTR: one name
    inline bool dns_read_name_rdata(const DnsReader& r, const DnsReader::Record& rec, DnsName& out) noexcept {
        size_t p = rec.rdata;
        return r.name_at(p, out) && p == rec.rdata + rec.length;
    }

    inline bool dns_read_mx(const DnsReader& r, const DnsReader::Record& rec, DnsMx& out) noexcept {
        if (rec.length < 3) {
            return false;
        }
        out.preference = r.u16_at(rec.rdata);
        size_t p = rec.rdata + 2;
        return r.name_at(p, out.host) && p == rec.rdata + rec.length;
    }

    // RFC 2782 forbids compression of the target; a pointer is read all
    // the same, as the names of other records are
    inline bool dns_read_srv(const DnsReader& r, const DnsReader::Record& rec, DnsSrv& out) noexcept {
        if (rec.length < 7) {
            return false;
        }
        out.priority = r.u16_at(rec.rdata);
        out.weight = r.u16_at(rec.rdata + 2);
        out.port = r.u16_at(rec.rdata + 4);
        size_t p = rec.rdata + 6;
        return r.name_at(p, out.target) && p == rec.rdata + rec.length;
    }

    inline bool dns_read_soa(const DnsReader& r, const DnsReader::Record& rec, DnsSoa& out) noexcept {
        size_t p = rec.rdata;
        size_t end = rec.rdata + rec.length;
        if (!r.name_at(p, out.mname) || !r.name_at(p, out.rname) || p > end || end - p != 20) {
            return false;
        }
        out.serial = r.u32_at(p);
        out.refresh = r.u32_at(p + 4);
        out.retry = r.u32_at(p + 8);
        out.expire = r.u32_at(p + 12);
        out.minimum = r.u32_at(p + 16);
        return true;
    }

    // TXT: one or more <character-string>s, each a length byte and that
    // many bytes, filling the rdata exactly. Their bytes joined (as Go's
    // LookupTXT gives a record), into out; false when the strings do not
    // fill the rdata. An rdata of no strings at all is taken as the empty
    // text, as Go takes it
    inline bool dns_txt_size(const DnsReader& r, const DnsReader::Record& rec, size_t& total) noexcept {
        size_t p = rec.rdata, end = rec.rdata + rec.length;
        total = 0;
        while (p < end) {
            size_t len = r.data()[p];
            if (end - p - 1 < len) {
                return false;
            }
            total += len;
            p += len + 1;
        }
        return true;
    }

    inline void dns_txt_join(const DnsReader& r, const DnsReader::Record& rec, char* out) noexcept {
        size_t p = rec.rdata, end = rec.rdata + rec.length;
        while (p < end) {
            size_t len = r.data()[p];
            copy_bytes(out, r.data() + p + 1, len);
            out += len;
            p += len + 1;
        }
    }

    inline bool dns_read_txt(const DnsReader& r, const DnsReader::Record& rec, string& out) noexcept {
        size_t total = 0;
        if (!dns_txt_size(r, rec, total)) {
            return false;
        }
        if (total <= 512) {
            char text[512];
            dns_txt_join(r, rec, text);
            out = string(std::string_view(text, total));
        } else {
            std::string text(total, '\0');
            dns_txt_join(r, rec, text.data());
            out = string(std::string_view(text));
        }
        return true;
    }

    inline constexpr int DnsMaxHops = 8;

    // What a lookup, a name or one exchange came to
    enum class DnsStatus {
        ok,
        nxdomain,       // RCODE 3: no such name
        nodata,         // the name, with no record of the type (RFC 2308)
        servfail,       // RCODE 2
        misbehaving,    // another RCODE, an answer that does not read, a lame referral, a chain too long
        timeout,
        failed,         // a socket's error, in `error`
        cancelled,
        invalid,        // a name that is no name
        foreign,        // not the answer to the query: ignored
        truncated       // TC: asked again over TCP
    };

    // One record of the type asked, its rdata as the public types take it
    struct DnsRecordData {
        string name;                // MX's host, SRV's target, NS's host, CNAME's target
        string text;                // TXT's strings joined
        uint16_t first = 0;         // MX's preference, SRV's priority
        uint16_t second = 0;        // SRV's weight
        uint16_t third = 0;         // SRV's port
        uint8_t address[16] = {};   // A's four bytes, AAAA's sixteen
        uint8_t address_size = 0;
    };

    struct DnsAnswer {
        DnsStatus status = DnsStatus::foreign;
        DnsName end;                // the records' owner: the question's name or the chain's end
        int hops = 0;               // the CNAMEs followed to it
        vector<DnsRecordData> records;
        optional<io::error> error;  // a socket's, for failed
    };

    // A record of the type asked into `out`; false when its rdata does
    // not read
    inline bool dns_decode(const DnsReader& r, const DnsReader::Record& rec, uint16_t type, vector<DnsRecordData>& out) noexcept {
        DnsRecordData d;
        switch (type) {
            case dns_type::mx: {
                DnsMx mx;
                if (!dns_read_mx(r, rec, mx)) {
                    return false;
                }
                d.first = mx.preference;
                d.name = dns_name_string(mx.host);
                break;
            }
            case dns_type::srv: {
                DnsSrv srv;
                if (!dns_read_srv(r, rec, srv)) {
                    return false;
                }
                d.first = srv.priority;
                d.second = srv.weight;
                d.third = srv.port;
                d.name = dns_name_string(srv.target);
                break;
            }
            case dns_type::ns:
            case dns_type::cname:
            case dns_type::ptr: {
                DnsName n;
                if (!dns_read_name_rdata(r, rec, n)) {
                    return false;
                }
                d.name = dns_name_string(n);
                break;
            }
            case dns_type::txt:
                if (!dns_read_txt(r, rec, d.text)) {
                    return false;
                }
                break;
            case dns_type::a:
            case dns_type::aaaa:
                if (rec.length != (type == dns_type::a ? 4u : 16u)) {
                    return false;
                }
                copy_bytes(d.address, r.data() + rec.rdata, rec.length);
                d.address_size = uint8_t(rec.length);
                break;
            default:
                break;
        }
        out.push_back(std::move(d));
        return true;
    }

    // A message read as the answer to the query (id, qname, qtype): foreign
    // when it is not one (to be ignored), truncated when its TC bit is set
    // and it came over UDP, else what it says
    inline DnsStatus dns_read_answer(const uint8_t* message, size_t size, uint16_t id, const DnsName& qname, uint16_t qtype, bool tcp, DnsAnswer& out) noexcept {
        out.records.clear();
        out.hops = 0;
        out.end = qname;
        DnsReader r(message, size);
        DnsHeader h;
        if (!r.header(h) || !(h.flags & DnsFlagResponse) || h.id != id || h.opcode() != 0 || h.questions != 1) {
            return out.status = DnsStatus::foreign;
        }
        DnsName q;
        uint16_t qt = 0, qc = 0;
        if (!r.question(q, qt, qc) || qt != qtype || qc != DnsClassIn || !(q == qname)) {
            return out.status = DnsStatus::foreign;
        }
        if (!tcp && (h.flags & DnsFlagTruncated)) {
            return out.status = DnsStatus::truncated;
        }
        size_t answers_at = r.position();
        DnsReader::Record rec;
        for (size_t i = 0; i < size_t(h.answers) + h.authorities; ++i) {
            if (!r.record(rec)) {
                return out.status = DnsStatus::misbehaving;
            }
        }
        int extended = 0;
        bool opt = false;
        for (size_t i = 0; i < h.additionals; ++i) {
            if (!r.record(rec)) {
                return out.status = DnsStatus::misbehaving;
            }
            if (rec.type == dns_type::opt) {
                if (opt || !rec.name.is_root()) {
                    return out.status = DnsStatus::misbehaving;   // RFC 6891 §6.1.1: one OPT, at the root
                }
                opt = true;
                extended = int(rec.ttl >> 24);
            }
        }
        int rcode = extended << 4 | h.rcode();
        if (rcode == dns_rcode::server_failure) {
            return out.status = DnsStatus::servfail;
        }
        if (rcode != dns_rcode::no_error && rcode != dns_rcode::name_error) {
            return out.status = DnsStatus::misbehaving;
        }
        // the chain, from the question's name: a CNAME whose owner is the
        // name reached so far moves it on, in any order the records come
        DnsName current = qname;
        int hops = 0;
        if (qtype != dns_type::cname) {
            for (bool moved = true; moved;) {
                moved = false;
                DnsReader a(message, size);
                a.seek(answers_at);
                for (size_t i = 0; i < h.answers; ++i) {
                    a.record(rec);
                    if (rec.type == dns_type::cname && rec.klass == DnsClassIn && rec.name == current) {
                        DnsName target;
                        if (!dns_read_name_rdata(a, rec, target)) {
                            return out.status = DnsStatus::misbehaving;
                        }
                        if (++hops > DnsMaxHops) {
                            return out.status = DnsStatus::misbehaving;   // a loop, or a chain past the bound
                        }
                        current = target;
                        moved = true;
                        break;
                    }
                }
            }
        }
        out.end = current;
        out.hops = hops;
        if (rcode == dns_rcode::name_error) {
            return out.status = DnsStatus::nxdomain;
        }
        DnsReader a(message, size);
        a.seek(answers_at);
        for (size_t i = 0; i < h.answers; ++i) {
            a.record(rec);
            if (rec.type == qtype && rec.klass == DnsClassIn && rec.name == current) {
                if (!dns_decode(a, rec, qtype, out.records)) {
                    out.records.clear();
                    return out.status = DnsStatus::misbehaving;
                }
            }
        }
        if (!out.records.empty()) {
            return out.status = DnsStatus::ok;
        }
        if (hops == 0 && h.answers == 0 && !(h.flags & (DnsFlagAuthoritative | DnsFlagRecursionAvailable))) {
            return out.status = DnsStatus::misbehaving;   // a lame referral: a server that neither knows nor asks
        }
        return out.status = DnsStatus::nodata;
    }

    // A message written in place into a buffer of `capacity` bytes. Every
    // write says whether it fitted; once one has not, none does. Names may
    // be compressed: each label written in full is remembered (up to 64,
    // at offsets a pointer can reach), and a name's longest suffix already
    // in the message becomes a pointer to it
    class DnsWriter {
    public:
        SGCL_INLINE_HOT DnsWriter(uint8_t* buffer, size_t capacity) noexcept
        : _b(buffer)
        , _cap(capacity) {
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT bool ok() const noexcept {
            return _ok;
        }

        bool header(const DnsHeader& h) noexcept {
            if (_size != 0 || _cap < DnsHeaderSize) {
                return _fail();
            }
            _size = DnsHeaderSize;
            set_header(h);
            return true;
        }

        // The header written again, its counts as they came out
        void set_header(const DnsHeader& h) noexcept {
            if (_size < DnsHeaderSize) {
                return;
            }
            _put16(0, h.id);
            _put16(2, h.flags);
            _put16(4, h.questions);
            _put16(6, h.answers);
            _put16(8, h.authorities);
            _put16(10, h.additionals);
        }

        bool name(const DnsName& n, bool compress = false) noexcept {
            size_t i = 0;
            while (n.bytes[i] != 0) {
                if (compress) {
                    if (size_t at = _find(n, i); at != 0) {
                        return u16(uint16_t(0xC000 | at));
                    }
                }
                size_t len = size_t(n.bytes[i]) + 1;
                size_t here = _size;
                if (!bytes(n.bytes + i, len)) {
                    return false;
                }
                if (here < 0x4000 && _labels < MaxLabels) {
                    _label[_labels++] = uint16_t(here);
                }
                i += len;
            }
            return u8(0);
        }

        bool question(const DnsName& n, uint16_t type, uint16_t klass, bool compress = false) noexcept {
            return name(n, compress) && u16(type) && u16(klass);
        }

        // A record's fixed part; its rdata follows, and record_end writes
        // its length
        bool record_begin(const DnsName& n, uint16_t type, uint16_t klass, uint32_t ttl, bool compress = false) noexcept {
            if (!(name(n, compress) && u16(type) && u16(klass) && u32(ttl) && u16(0))) {
                return false;
            }
            _rdata = _size;
            return true;
        }

        bool record_end() noexcept {
            if (!_ok || _size - _rdata > 0xFFFF) {
                return _fail();
            }
            _put16(_rdata - 2, uint16_t(_size - _rdata));
            return true;
        }

        // TXT's character-strings: the text in pieces of at most 255 bytes,
        // one empty string for the empty text
        bool txt(std::string_view text) noexcept {
            if (text.empty()) {
                return u8(0);
            }
            while (!text.empty()) {
                size_t take = text.size() < 255 ? text.size() : 255;
                if (!u8(uint8_t(take)) || !bytes(text.data(), take)) {
                    return false;
                }
                text.remove_prefix(take);
            }
            return true;
        }

        // The OPT pseudo-record of RFC 6891 §6.1.2: the root's name, the
        // UDP payload this side takes in the class, the extended rcode,
        // version 0 and no flags in the TTL, no options
        bool opt(uint16_t payload) noexcept {
            return u8(0) && u16(dns_type::opt) && u16(payload) && u32(0) && u16(0);
        }

        // The OPT record with the Padding option of RFC 7830, its zeros as
        // many as make the message a multiple of `block` bytes once the
        // record is written (RFC 8467 §4.1); the message's other sections
        // are written before it
        bool opt_padded(uint16_t payload, size_t block) noexcept {
            size_t after = _size + 11 + 4;   // the record's fixed part, the option's code and length
            size_t pad = (block - after % block) % block;
            if (!(u8(0) && u16(dns_type::opt) && u16(payload) && u32(0) && u16(uint16_t(4 + pad)) && u16(DnsOptionPadding) && u16(uint16_t(pad)))) {
                return false;
            }
            if (!_ok || _cap - _size < pad) {
                return _fail();
            }
            fill_bytes(_b + _size, 0, pad);
            _size += pad;
            return true;
        }

        // Back to a size written before: what came after it forgotten,
        // the labels remembered there with it, and the writer good again
        void truncate(size_t size) noexcept {
            if (size > _size) {
                return;
            }
            _size = size;
            _ok = true;
            while (_labels > 0 && _label[_labels - 1] >= size) {
                --_labels;
            }
        }

        bool u8(uint8_t v) noexcept {
            if (!_ok || _cap - _size < 1) {
                return _fail();
            }
            _b[_size++] = v;
            return true;
        }

        bool u16(uint16_t v) noexcept {
            if (!_ok || _cap - _size < 2) {
                return _fail();
            }
            _put16(_size, v);
            _size += 2;
            return true;
        }

        bool u32(uint32_t v) noexcept {
            return u16(uint16_t(v >> 16)) && u16(uint16_t(v));
        }

        bool bytes(const void* p, size_t n) noexcept {
            if (!_ok || _cap - _size < n) {
                return _fail();
            }
            copy_bytes(_b + _size, p, n);
            _size += n;
            return true;
        }

    private:
        static constexpr size_t MaxLabels = 64;

        SGCL_INLINE_HOT void _put16(size_t at, uint16_t v) noexcept {
            _b[at] = uint8_t(v >> 8);
            _b[at + 1] = uint8_t(v);
        }

        SGCL_INLINE_HOT bool _fail() noexcept {
            _ok = false;
            return false;
        }

        // A remembered label whose name from there on is the suffix of n
        // from byte i: its offset, 0 for none (no name starts in the header)
        size_t _find(const DnsName& n, size_t i) const noexcept {
            DnsName suffix;
            size_t len = size_t(n.size) - i;
            copy_bytes(suffix.bytes, n.bytes + i, len);
            suffix.size = uint16_t(len);
            DnsReader r(_b, _size);
            for (size_t k = 0; k < _labels; ++k) {
                size_t p = _label[k];
                DnsName there;
                if (r.name_at(p, there) && there.size == suffix.size && std::string_view(reinterpret_cast<const char*>(there.bytes), there.size) == std::string_view(reinterpret_cast<const char*>(suffix.bytes), suffix.size)) {
                    return _label[k];   // the same bytes, case and all: a name keeps the case it was written in (RFC 4343 §4.1)
                }
            }
            return 0;
        }

        uint8_t* _b;
        size_t _cap;
        size_t _size = 0;
        size_t _rdata = 0;
        bool _ok = true;
        uint16_t _label[MaxLabels] = {0};
        size_t _labels = 0;
    };

    // A query: the header (the id given, recursion desired), the one
    // question, and the OPT record with the UDP payload when `edns`
    // (RFC 6891). Its size, or 0 when the buffer is too small
    inline size_t dns_write_query(uint8_t* buffer, size_t capacity, uint16_t id, const DnsName& name, uint16_t type, bool edns = true, size_t pad_block = 0) noexcept {
        DnsWriter w(buffer, capacity);
        DnsHeader h;
        h.id = id;
        h.flags = DnsFlagRecursionDesired;
        h.questions = 1;
        h.additionals = edns ? 1 : 0;
        w.header(h);
        w.question(name, type, DnsClassIn);
        if (edns && pad_block) {
            w.opt_padded(DnsUdpPayload, pad_block);
        } else if (edns) {
            w.opt(DnsUdpPayload);
        }
        return w.ok() ? w.size() : 0;
    }
}
