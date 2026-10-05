//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The DNS codec (sgcl/net/detail/dns_message.h) and the reading of an
// answer (dns_read_answer): names in their text and wire forms with the
// escapes of RFC 1035 §5.1 and the limits of §2.3.4 (63, 255), compression
// pointers (§4.1.4) forward, into themselves and in loops refused, label
// types 0b01 and 0b10 refused, every message cut at every byte, every
// record type the resolver reads, EDNS0's OPT (RFC 6891), and an answer's
// checks: the id, the question echoed (RFC 5452), TC, the RCODEs, the
// chain of CNAMEs and its bound, a lame referral. Messages are written by
// hand from the RFC where the bytes matter.
#include "tests/types.h"
#include "sgcl/net/net.h"

#include <map>
#include <string>
#include <vector>

namespace {
    namespace nd = sgcl::net::detail;
    namespace type = sgcl::net::detail::dns_type;

    nd::DnsName wire(const std::string& text) {
        nd::DnsName n;
        EXPECT_TRUE(nd::dns_name_from_text(text, n)) << text;
        return n;
    }

    std::string text_of(const nd::DnsName& n) {
        char t[nd::DnsMaxNameText];
        return std::string(t, nd::dns_name_to_text(n, t));
    }

    std::string bytes_of(const nd::DnsName& n) {
        return std::string(reinterpret_cast<const char*>(n.bytes), n.size);
    }

    // A message from its bytes, written as the RFC draws them
    struct Bytes {
        std::vector<uint8_t> b;

        Bytes& u8(uint8_t v) {
            b.push_back(v);
            return *this;
        }

        Bytes& u16(uint16_t v) {
            b.push_back(uint8_t(v >> 8));
            b.push_back(uint8_t(v));
            return *this;
        }

        Bytes& u32(uint32_t v) {
            return u16(uint16_t(v >> 16)).u16(uint16_t(v));
        }

        Bytes& label(const std::string& s) {
            b.push_back(uint8_t(s.size()));
            b.insert(b.end(), s.begin(), s.end());
            return *this;
        }

        Bytes& name(std::initializer_list<const char*> labels) {
            for (auto l : labels) {
                label(l);
            }
            return u8(0);
        }

        Bytes& pointer(uint16_t at) {
            return u16(uint16_t(0xC000 | at));
        }

        Bytes& header(uint16_t id, uint16_t flags, uint16_t qd, uint16_t an, uint16_t ns = 0, uint16_t ar = 0) {
            return u16(id).u16(flags).u16(qd).u16(an).u16(ns).u16(ar);
        }
    };

    bool read_name_at(const std::vector<uint8_t>& m, size_t pos, nd::DnsName& out, size_t* after = nullptr) {
        nd::DnsReader r(m.data(), m.size());
        bool ok = r.name_at(pos, out);
        if (after) {
            *after = pos;
        }
        return ok;
    }
}

TEST(NetDnsMessage_Tests, NamesFromText) {
    nd::DnsName n;
    nd::DnsNameText info;
    ASSERT_TRUE(nd::dns_name_from_text("www.Example.COM", n, &info));
    EXPECT_EQ(bytes_of(n), std::string("\3www\7Example\3COM\0", 17));
    EXPECT_FALSE(info.rooted);
    EXPECT_EQ(info.dots, 2);
    ASSERT_TRUE(nd::dns_name_from_text("www.example.com.", n, &info));
    EXPECT_TRUE(info.rooted);
    EXPECT_EQ(info.dots, 2);
    EXPECT_EQ(n.size, 17);
    ASSERT_TRUE(nd::dns_name_from_text(".", n, &info));
    EXPECT_TRUE(n.is_root());
    EXPECT_TRUE(info.rooted);
    ASSERT_TRUE(nd::dns_name_from_text("localhost", n, &info));
    EXPECT_EQ(info.dots, 0);
    EXPECT_EQ(n.labels(), 1u);
    // the escapes of RFC 1035 §5.1
    ASSERT_TRUE(nd::dns_name_from_text("a\\.b.c", n));
    EXPECT_EQ(bytes_of(n), std::string("\3a.b\1c\0", 7));
    ASSERT_TRUE(nd::dns_name_from_text("\\065\\\\\\000.x", n));
    EXPECT_EQ(bytes_of(n), std::string("\3A\\\0\1x\0", 7));
    // refused
    for (const char* bad : {"", "..", ".a", "a..b", "a.b..", "\\", "a\\", "\\25", "\\2a5", "\\256", "x\\999"}) {
        EXPECT_FALSE(nd::dns_name_from_text(bad, n)) << bad;
    }
}

TEST(NetDnsMessage_Tests, TheLimitsOfANameFromText) {
    nd::DnsName n;
    std::string l63(63, 'a'), l64(64, 'a');
    EXPECT_TRUE(nd::dns_name_from_text(l63 + ".com", n));
    EXPECT_FALSE(nd::dns_name_from_text(l64 + ".com", n));
    EXPECT_FALSE(nd::dns_name_from_text("com." + l64, n));
    EXPECT_FALSE(nd::dns_name_from_text("com." + l64 + ".", n));
    // 255 bytes on the wire: four labels of 63 (256 bytes) is too long,
    // three of 63 and one of 61 (254 + the root) is the longest
    std::string longest = l63 + "." + l63 + "." + l63 + "." + std::string(61, 'b');
    ASSERT_TRUE(nd::dns_name_from_text(longest, n));
    EXPECT_EQ(n.size, 255);
    ASSERT_TRUE(nd::dns_name_from_text(longest + ".", n));
    EXPECT_EQ(n.size, 255);
    EXPECT_FALSE(nd::dns_name_from_text(longest + "b", n));
    EXPECT_FALSE(nd::dns_name_from_text(longest + ".c", n));
    // an escape counts as its one byte
    std::string escaped;
    for (int i = 0; i < 63; ++i) {
        escaped += "\\046";
    }
    ASSERT_TRUE(nd::dns_name_from_text(escaped + ".x", n));
    EXPECT_EQ(n.size, 64 + 2 + 1);
    EXPECT_FALSE(nd::dns_name_from_text(escaped + "\\046.x", n));
    // 127 labels of one byte: 254 bytes and the root
    std::string many;
    for (int i = 0; i < 127; ++i) {
        many += i ? ".a" : "a";
    }
    ASSERT_TRUE(nd::dns_name_from_text(many, n));
    EXPECT_EQ(n.size, 255);
    EXPECT_FALSE(nd::dns_name_from_text(many + ".a", n));
}

TEST(NetDnsMessage_Tests, NamesToText) {
    EXPECT_EQ(text_of(wire("www.example.com")), "www.example.com.");
    EXPECT_EQ(text_of(wire(".")), ".");
    EXPECT_EQ(text_of(wire("a\\.b.c")), "a\\.b.c.");
    EXPECT_EQ(text_of(wire("\\000\\032\\127\\255\\\\.x")), "\\000\\032\\127\\255\\\\.x.");
    EXPECT_EQ(text_of(wire("~!.x")), "~!.x.");
    // the text read back is the same name, whatever its bytes
    nd::DnsName any;
    any.size = 0;
    any.bytes[any.size++] = 63;
    for (int i = 0; i < 63; ++i) {
        any.bytes[any.size++] = uint8_t(i * 4 + 1);
    }
    any.bytes[any.size++] = 0;
    nd::DnsName back;
    std::string t = text_of(any);
    ASSERT_TRUE(nd::dns_name_from_text(t, back));
    EXPECT_EQ(bytes_of(back), bytes_of(any));
    // the longest text: every byte \DDD
    nd::DnsName worst;
    worst.size = 0;
    for (int l = 0; l < 4; ++l) {
        int len = l < 3 ? 63 : 61;
        worst.bytes[worst.size++] = uint8_t(len);
        for (int i = 0; i < len; ++i) {
            worst.bytes[worst.size++] = 0;
        }
    }
    worst.bytes[worst.size++] = 0;
    EXPECT_EQ(worst.size, 255);
    EXPECT_EQ(text_of(worst).size(), size_t(250 * 4 + 4));
    EXPECT_LE(text_of(worst).size(), nd::DnsMaxNameText);
    EXPECT_EQ(std::string(nd::dns_name_string(wire("a.b")).view()), "a.b.");
}

TEST(NetDnsMessage_Tests, NamesCompareWithoutCase) {
    EXPECT_TRUE(wire("WWW.Example.COM") == wire("www.example.com."));
    EXPECT_FALSE(wire("www.example.com") == wire("www.example.org"));
    EXPECT_FALSE(wire("www.example.com") == wire("www.example.co"));
    EXPECT_FALSE(wire("a[") == wire("a{"));   // '[' is not 'Z' + 1 folded
    EXPECT_FALSE(wire("a@") == wire("a`"));
    EXPECT_TRUE(wire(".") == wire("."));
    nd::DnsName joined;
    ASSERT_TRUE(nd::dns_name_join(wire("mail"), wire("corp.example."), joined));
    EXPECT_EQ(text_of(joined), "mail.corp.example.");
    ASSERT_TRUE(nd::dns_name_join(wire("mail"), wire("."), joined));
    EXPECT_EQ(text_of(joined), "mail.");
    std::string l63(63, 'a');
    EXPECT_FALSE(nd::dns_name_join(wire(l63 + "." + l63), wire(l63 + "." + l63), joined));
}

// RFC 1035 §4.1.4's example: F.ISI.ARPA at 20, FOO.F.ISI.ARPA as FOO and
// a pointer to 20, ARPA as a pointer to 26, the root at 40
TEST(NetDnsMessage_Tests, CompressionPointers) {
    std::vector<uint8_t> m(20, 0);
    Bytes b;
    b.b = m;
    b.name({"F", "ISI", "ARPA"});       // 20..31
    b.label("FOO").pointer(20);         // 32..37
    b.pointer(26);                      // 38..39
    b.u8(0);                            // 40
    nd::DnsName n;
    size_t after = 0;
    ASSERT_TRUE(read_name_at(b.b, 20, n, &after));
    EXPECT_EQ(text_of(n), "F.ISI.ARPA.");
    EXPECT_EQ(after, 32u);
    ASSERT_TRUE(read_name_at(b.b, 32, n, &after));
    EXPECT_EQ(text_of(n), "FOO.F.ISI.ARPA.");
    EXPECT_EQ(after, 38u);
    ASSERT_TRUE(read_name_at(b.b, 38, n, &after));
    EXPECT_EQ(text_of(n), "ARPA.");
    EXPECT_EQ(after, 40u);
    ASSERT_TRUE(read_name_at(b.b, 40, n, &after));
    EXPECT_TRUE(n.is_root());
    EXPECT_EQ(after, 41u);
    // a pointer to a pointer to a name
    b.pointer(38);                      // 41
    ASSERT_TRUE(read_name_at(b.b, 41, n, &after));
    EXPECT_EQ(text_of(n), "ARPA.");
    EXPECT_EQ(after, 43u);
}

TEST(NetDnsMessage_Tests, PointerLoopsAndForwardPointersAreRefused) {
    nd::DnsName n;
    {
        Bytes b;
        b.b.assign(12, 0);
        b.pointer(12);                  // to itself
        EXPECT_FALSE(read_name_at(b.b, 12, n));
    }
    {
        Bytes b;
        b.b.assign(12, 0);
        b.pointer(14).label("a").u8(0); // forward
        EXPECT_FALSE(read_name_at(b.b, 12, n));
    }
    {
        Bytes b;
        b.b.assign(12, 0);
        b.label("a").pointer(12);       // a label, then back to itself: a loop
        EXPECT_FALSE(read_name_at(b.b, 12, n));
    }
    {
        Bytes b;
        b.b.assign(12, 0);
        b.label("a").pointer(16);       // 12: "a" -> 16
        b.label("b").pointer(12);       // 16: "b" -> 12: a loop of two
        EXPECT_FALSE(read_name_at(b.b, 16, n));
        EXPECT_FALSE(read_name_at(b.b, 12, n));
    }
    {
        Bytes b;
        b.b.assign(12, 0);
        b.u8(0x40).u8(0);               // label type 0b01 (RFC 6891 §5: none)
        EXPECT_FALSE(read_name_at(b.b, 12, n));
        Bytes c;
        c.b.assign(12, 0);
        c.u8(0x80).u8(0);               // 0b10
        EXPECT_FALSE(read_name_at(c.b, 12, n));
    }
    {
        Bytes b;
        b.b.assign(12, 0);
        b.u8(0xC0);                     // a pointer cut short
        EXPECT_FALSE(read_name_at(b.b, 12, n));
    }
    {
        Bytes b;
        b.b.assign(12, 0);
        b.label("abc");                 // no root
        EXPECT_FALSE(read_name_at(b.b, 12, n));
    }
}

// A name of more than 255 bytes made of pointers to labels each within
// the limit: refused, as one written whole would be
TEST(NetDnsMessage_Tests, ANameThatPointersMakeTooLong) {
    Bytes b;
    b.b.assign(12, 0);
    std::string l63(63, 'x');
    b.label(l63).u8(0);                       // 12: x63
    b.label(l63).pointer(12);                 // 77: x63.x63
    b.label(l63).pointer(77);                 // 143: x63.x63.x63 (192 bytes)
    b.label(std::string(61, 'y')).pointer(143);   // 209: 254 + root = 255
    b.label("z").pointer(143);                // 274: 194 + ...
    nd::DnsName n;
    ASSERT_TRUE(read_name_at(b.b, 209, n));
    EXPECT_EQ(n.size, 255);
    EXPECT_TRUE(read_name_at(b.b, 273, n));
    Bytes c = b;
    c.label(std::string(62, 'y')).pointer(143);   // 256: one byte too many
    EXPECT_FALSE(read_name_at(c.b, b.b.size(), n));
    Bytes d = b;
    d.label(std::string(64, 'q')).u8(0);          // a label of 64 is a label type 0b01
    EXPECT_FALSE(read_name_at(d.b, b.b.size(), n));
}

namespace {
    // A whole answer: the question, an MX, a TXT of two strings, an SRV, an
    // NS, an A and an AAAA, an SOA in the authority section, the OPT; the
    // names compressed
    std::vector<uint8_t> sample(uint16_t id, uint16_t qtype, const std::string& qname = "example.test", uint16_t flags = 0x8180) {
        std::vector<uint8_t> m(1024);
        nd::DnsWriter w(m.data(), m.size());
        nd::DnsHeader h;
        h.id = id;
        h.flags = flags;
        h.questions = 1;
        h.answers = 7;
        h.authorities = 1;
        h.additionals = 1;
        w.header(h);
        w.question(wire(qname), qtype, nd::DnsClassIn, true);
        w.record_begin(wire("example.test"), type::mx, nd::DnsClassIn, 300, true);
        w.u16(10);
        w.name(wire("mx1.example.test"), true);
        w.record_end();
        w.record_begin(wire("example.test"), type::txt, nd::DnsClassIn, 300, true);
        w.txt("v=spf1 ");
        w.txt("-all");
        w.record_end();
        w.record_begin(wire("_sip._tcp.example.test"), type::srv, nd::DnsClassIn, 300, true);
        w.u16(1);
        w.u16(5);
        w.u16(5060);
        w.name(wire("sip.example.test"), false);
        w.record_end();
        w.record_begin(wire("example.test"), type::ns, nd::DnsClassIn, 300, true);
        w.name(wire("ns.example.test"), true);
        w.record_end();
        w.record_begin(wire("mx1.example.test"), type::a, nd::DnsClassIn, 300, true);
        w.u32(0xC0000201);
        w.record_end();
        w.record_begin(wire("mx1.example.test"), type::aaaa, nd::DnsClassIn, 300, true);
        for (int i = 0; i < 4; ++i) {
            w.u32(i == 0 ? 0x20010db8 : uint32_t(i));
        }
        w.record_end();
        w.record_begin(wire("www.example.test"), type::cname, nd::DnsClassIn, 300, true);
        w.name(wire("example.test"), true);
        w.record_end();
        w.record_begin(wire("example.test"), type::soa, nd::DnsClassIn, 60, true);
        w.name(wire("ns.example.test"), true);
        w.name(wire("hostmaster.example.test"), true);
        for (uint32_t v : {2026u, 3600u, 600u, 86400u, 60u}) {
            w.u32(v);
        }
        w.record_end();
        w.opt(1232);
        EXPECT_TRUE(w.ok());
        m.resize(w.size());
        return m;
    }
}

TEST(NetDnsMessage_Tests, EveryRecordTypeRead) {
    auto m = sample(0x1234, type::mx);
    nd::DnsReader r(m.data(), m.size());
    nd::DnsHeader h;
    ASSERT_TRUE(r.header(h));
    EXPECT_EQ(h.id, 0x1234);
    EXPECT_EQ(h.rcode(), 0);
    EXPECT_EQ(h.opcode(), 0);
    nd::DnsName q;
    uint16_t qt = 0, qc = 0;
    ASSERT_TRUE(r.question(q, qt, qc));
    EXPECT_EQ(text_of(q), "example.test.");
    EXPECT_EQ(qt, type::mx);
    std::vector<std::string> seen;
    nd::DnsReader::Record rec;
    for (int i = 0; i < 9; ++i) {
        ASSERT_TRUE(r.record(rec)) << i;
        switch (rec.type) {
            case type::mx: {
                nd::DnsMx mx;
                ASSERT_TRUE(nd::dns_read_mx(r, rec, mx));
                seen.push_back("MX " + std::to_string(mx.preference) + " " + text_of(mx.host));
                break;
            }
            case type::txt: {
                sgcl::string t;
                ASSERT_TRUE(nd::dns_read_txt(r, rec, t));
                seen.push_back("TXT " + std::string(t.view()));
                break;
            }
            case type::srv: {
                nd::DnsSrv s;
                ASSERT_TRUE(nd::dns_read_srv(r, rec, s));
                seen.push_back("SRV " + std::to_string(s.priority) + " " + std::to_string(s.weight) + " " + std::to_string(s.port) + " " + text_of(s.target));
                break;
            }
            case type::ns:
            case type::cname: {
                nd::DnsName n;
                ASSERT_TRUE(nd::dns_read_name_rdata(r, rec, n));
                seen.push_back((rec.type == type::ns ? "NS " : "CNAME ") + text_of(n));
                break;
            }
            case type::soa: {
                nd::DnsSoa s;
                ASSERT_TRUE(nd::dns_read_soa(r, rec, s));
                seen.push_back("SOA " + text_of(s.mname) + " " + text_of(s.rname) + " " + std::to_string(s.serial) + " " + std::to_string(s.minimum));
                break;
            }
            case type::opt:
                seen.push_back("OPT " + std::to_string(rec.klass) + " " + std::to_string(rec.ttl) + " " + std::to_string(rec.length));
                EXPECT_TRUE(rec.name.is_root());
                break;
            default:
                seen.push_back("type " + std::to_string(rec.type) + " " + std::to_string(rec.length));
        }
    }
    EXPECT_EQ(r.position(), m.size());
    std::vector<std::string> want = {"MX 10 mx1.example.test.", "TXT v=spf1 -all", "SRV 1 5 5060 sip.example.test.", "NS ns.example.test.",
                                     "type 1 4", "type 28 16", "CNAME example.test.", "SOA ns.example.test. hostmaster.example.test. 2026 60", "OPT 1232 0 0"};
    EXPECT_EQ(seen, want);
    EXPECT_FALSE(r.record(rec));   // no more
}

TEST(NetDnsMessage_Tests, TheWriterCompresses) {
    auto m = sample(1, type::mx);
    // the question's name written whole, the owners after it pointers
    EXPECT_EQ(m[12], 7);
    size_t after_question = 12 + 14 + 4;
    EXPECT_EQ(m[after_question], 0xC0);
    EXPECT_EQ(m[after_question + 1], 12);
    // the same message without compression is longer and reads the same
    std::vector<uint8_t> plain(1024);
    nd::DnsWriter w(plain.data(), plain.size());
    nd::DnsHeader h;
    h.questions = 1;
    h.answers = 1;
    w.header(h);
    w.question(wire("example.test"), type::mx, 1);
    w.record_begin(wire("example.test"), type::mx, 1, 1);
    w.u16(5);
    w.name(wire("mx.example.test"));
    w.record_end();
    EXPECT_EQ(w.size(), size_t(12 + 18 + 14 + 10 + 2 + 17));
    // the writer refuses what does not fit, and stays refusing
    uint8_t small[20];
    nd::DnsWriter s(small, sizeof(small));
    EXPECT_TRUE(s.header(h));
    EXPECT_FALSE(s.question(wire("example.test"), type::mx, 1));
    EXPECT_FALSE(s.u8(0));
    EXPECT_FALSE(s.ok());
    // a query: the header, the question, the OPT of RFC 6891 §6.1.2
    uint8_t q[512];
    size_t n = nd::dns_write_query(q, sizeof(q), 0xBEEF, wire("example.test"), type::txt);
    ASSERT_EQ(n, size_t(12 + 14 + 4 + 11));
    EXPECT_EQ(q[0], 0xBE);
    EXPECT_EQ(q[1], 0xEF);
    EXPECT_EQ(q[2], 0x01);   // RD
    EXPECT_EQ(q[3], 0x00);
    EXPECT_EQ(q[11], 1);     // ARCOUNT
    const uint8_t opt[] = {0, 0, 41, 0x04, 0xD0, 0, 0, 0, 0, 0, 0};
    EXPECT_EQ(std::vector<uint8_t>(q + n - 11, q + n), std::vector<uint8_t>(opt, opt + 11));
    EXPECT_EQ(nd::dns_write_query(q, 40, 1, wire("example.test"), type::txt), 0u);
    EXPECT_EQ(nd::dns_write_query(q, sizeof(q), 1, wire("example.test"), type::txt, false), size_t(12 + 14 + 4));
}

// Every message cut at every byte: the reader refuses what is not there,
// never reads past it (ASan would say), and the answer is a fault or
// foreign, never a record from beyond the end
TEST(NetDnsMessage_Tests, EveryTruncationRefused) {
    auto m = sample(7, type::mx);
    nd::DnsName qname = wire("example.test");
    for (size_t n = 0; n < m.size(); ++n) {
        std::vector<uint8_t> cut(m.begin(), m.begin() + ptrdiff_t(n));
        nd::DnsAnswer a;
        auto st = nd::dns_read_answer(cut.data(), cut.size(), 7, qname, type::mx, false, a);
        EXPECT_TRUE(st == nd::DnsStatus::foreign || st == nd::DnsStatus::misbehaving) << n;
        EXPECT_TRUE(a.records.empty());
    }
    nd::DnsAnswer a;
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 7, qname, type::mx, false, a), nd::DnsStatus::ok);
    ASSERT_EQ(a.records.size(), 1u);
    EXPECT_EQ(std::string(a.records[0].name.view()), "mx1.example.test.");
    EXPECT_EQ(a.records[0].first, 10);
}

TEST(NetDnsMessage_Tests, TheRdataMustFillItsRecord) {
    auto make = [](uint16_t t, std::vector<uint8_t> rdata) {
        Bytes b;
        b.header(1, 0x8180, 1, 1).name({"x"}).u16(t).u16(1);
        b.pointer(12).u16(t).u16(1).u32(1).u16(uint16_t(rdata.size()));
        b.b.insert(b.b.end(), rdata.begin(), rdata.end());
        return b.b;
    };
    struct Case {
        uint16_t type;
        std::vector<uint8_t> rdata;
        bool ok;
    } cases[] = {
        {type::mx, {0, 1, 0}, true},                      // preference 1, the root
        {type::mx, {0, 1}, false},                        // no name
        {type::mx, {0, 1, 0, 0}, false},                  // a byte past the name
        {type::mx, {0, 1, 1, 'a'}, false},                // the name past the rdata
        {type::srv, {0, 1, 0, 2, 0, 3, 0}, true},
        {type::srv, {0, 1, 0, 2, 0, 3}, false},
        {type::ns, {1, 'a', 0}, true},
        {type::ns, {1, 'a', 0, 9}, false},
        {type::cname, {0xC0, 12}, true},                  // a pointer to the question's name
        {type::txt, {3, 'a', 'b', 'c'}, true},
        {type::txt, {}, true},                            // no strings: the empty text
        {type::txt, {0}, true},                           // one empty string
        {type::txt, {3, 'a', 'b'}, false},                // a string past the rdata
        {type::txt, {1, 'a', 2, 'b'}, false},
        {type::a, {1, 2, 3, 4}, true},
        {type::a, {1, 2, 3}, false},
        {type::aaaa, std::vector<uint8_t>(16, 1), true},
        {type::aaaa, std::vector<uint8_t>(15, 1), false},
    };
    for (auto& c : cases) {
        auto m = make(c.type, c.rdata);
        nd::DnsAnswer a;
        auto st = nd::dns_read_answer(m.data(), m.size(), 1, wire("x"), c.type, false, a);
        EXPECT_EQ(st, c.ok ? nd::DnsStatus::ok : nd::DnsStatus::misbehaving) << c.type << " " << c.rdata.size();
    }
    // a record whose rdata runs past the message
    Bytes b;
    b.header(1, 0x8180, 1, 1).name({"x"}).u16(type::a).u16(1);
    b.pointer(12).u16(type::a).u16(1).u32(1).u16(10).u32(0);
    nd::DnsAnswer a;
    EXPECT_EQ(nd::dns_read_answer(b.b.data(), b.b.size(), 1, wire("x"), type::a, false, a), nd::DnsStatus::misbehaving);
}

TEST(NetDnsMessage_Tests, TXTStringsJoined) {
    Bytes b;
    b.header(1, 0x8180, 1, 2).name({"t"}).u16(type::txt).u16(1);
    std::string long1(255, 'x'), long2(255, 'y');
    b.pointer(12).u16(type::txt).u16(1).u32(1).u16(uint16_t(1 + 255 + 1 + 255 + 1 + 3));
    b.label(long1).label(long2).label("end");
    b.pointer(12).u16(type::txt).u16(1).u32(1).u16(4).label("one");
    nd::DnsAnswer a;
    ASSERT_EQ(nd::dns_read_answer(b.b.data(), b.b.size(), 1, wire("t"), type::txt, false, a), nd::DnsStatus::ok);
    ASSERT_EQ(a.records.size(), 2u);
    EXPECT_EQ(std::string(a.records[0].text.view()), long1 + long2 + "end");
    EXPECT_EQ(std::string(a.records[1].text.view()), "one");
    // the writer's pieces: 600 bytes in strings of 255, 255, 90
    std::vector<uint8_t> m(1024);
    nd::DnsWriter w(m.data(), m.size());
    nd::DnsHeader h;
    w.header(h);
    size_t at = w.size();
    w.txt(std::string(600, 'z'));
    EXPECT_EQ(w.size() - at, size_t(603));
    EXPECT_EQ(m[at], 255);
    EXPECT_EQ(m[at + 256], 255);
    EXPECT_EQ(m[at + 512], 90);
    at = w.size();
    w.txt("");
    EXPECT_EQ(w.size() - at, 1u);
}

// The checks of RFC 5452 §9.1 and the header's: what is not the answer to
// the query is foreign, ignored by the exchange
TEST(NetDnsMessage_Tests, AnAnswerToAnotherQueryIsForeign) {
    nd::DnsName qname = wire("example.test");
    nd::DnsAnswer a;
    auto m = sample(42, type::mx);
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 42, qname, type::mx, false, a), nd::DnsStatus::ok);
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 43, qname, type::mx, false, a), nd::DnsStatus::foreign);       // the id
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 42, wire("other.test"), type::mx, false, a), nd::DnsStatus::foreign);
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 42, qname, type::txt, false, a), nd::DnsStatus::foreign);     // the type
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 42, wire("EXAMPLE.Test."), type::mx, false, a), nd::DnsStatus::ok);   // the case is not
    auto query = sample(42, type::mx, "example.test", 0x0100);   // QR clear: a query
    EXPECT_EQ(nd::dns_read_answer(query.data(), query.size(), 42, qname, type::mx, false, a), nd::DnsStatus::foreign);
    auto notify = sample(42, type::mx, "example.test", 0x8000 | (4 << 11));   // opcode NOTIFY
    EXPECT_EQ(nd::dns_read_answer(notify.data(), notify.size(), 42, qname, type::mx, false, a), nd::DnsStatus::foreign);
    // no question, two questions, another class
    Bytes none;
    none.header(42, 0x8180, 0, 0);
    EXPECT_EQ(nd::dns_read_answer(none.b.data(), none.b.size(), 42, qname, type::mx, false, a), nd::DnsStatus::foreign);
    Bytes two;
    two.header(42, 0x8180, 2, 0).name({"example", "test"}).u16(type::mx).u16(1).pointer(12).u16(type::mx).u16(1);
    EXPECT_EQ(nd::dns_read_answer(two.b.data(), two.b.size(), 42, qname, type::mx, false, a), nd::DnsStatus::foreign);
    Bytes chaos;
    chaos.header(42, 0x8180, 1, 0).name({"example", "test"}).u16(type::mx).u16(3);
    EXPECT_EQ(nd::dns_read_answer(chaos.b.data(), chaos.b.size(), 42, qname, type::mx, false, a), nd::DnsStatus::foreign);
}

TEST(NetDnsMessage_Tests, TheCodesOfAnAnswer) {
    nd::DnsName qname = wire("example.test");
    auto status = [&](uint16_t flags, uint16_t qtype = type::mx) {
        auto m = sample(9, qtype, "example.test", flags);
        nd::DnsAnswer a;
        return nd::dns_read_answer(m.data(), m.size(), 9, qname, qtype, false, a);
    };
    EXPECT_EQ(status(0x8180), nd::DnsStatus::ok);
    EXPECT_EQ(status(0x8183), nd::DnsStatus::nxdomain);
    EXPECT_EQ(status(0x8182), nd::DnsStatus::servfail);
    EXPECT_EQ(status(0x8185), nd::DnsStatus::misbehaving);   // REFUSED
    EXPECT_EQ(status(0x8181), nd::DnsStatus::misbehaving);   // FORMERR
    EXPECT_EQ(status(0x8184), nd::DnsStatus::misbehaving);   // NOTIMP
    EXPECT_EQ(status(0x8380), nd::DnsStatus::truncated);     // TC over UDP
    {
        auto m = sample(9, type::mx, "example.test", 0x8380);
        nd::DnsAnswer a;
        EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 9, qname, type::mx, true, a), nd::DnsStatus::ok);   // over TCP the bit means nothing
    }
    // the OPT's extended RCODE: BADVERS (16) is 1 in the OPT, 0 in the header
    {
        Bytes b;
        b.header(9, 0x8180, 1, 0, 0, 1).name({"example", "test"}).u16(type::mx).u16(1);
        b.u8(0).u16(type::opt).u16(1232).u32(0x01000000).u16(0);
        nd::DnsAnswer a;
        EXPECT_EQ(nd::dns_read_answer(b.b.data(), b.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::misbehaving);
    }
    // two OPTs, an OPT not at the root
    {
        Bytes b;
        b.header(9, 0x8180, 1, 0, 0, 2).name({"example", "test"}).u16(type::mx).u16(1);
        b.u8(0).u16(type::opt).u16(1232).u32(0).u16(0);
        b.u8(0).u16(type::opt).u16(1232).u32(0).u16(0);
        nd::DnsAnswer a;
        EXPECT_EQ(nd::dns_read_answer(b.b.data(), b.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::misbehaving);
        Bytes c;
        c.header(9, 0x8180, 1, 0, 0, 1).name({"example", "test"}).u16(type::mx).u16(1);
        c.pointer(12).u16(type::opt).u16(1232).u32(0).u16(0);
        EXPECT_EQ(nd::dns_read_answer(c.b.data(), c.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::misbehaving);
    }
    // NODATA: no record of the type; a lame referral: no answer, neither AA nor RA
    {
        Bytes b;
        b.header(9, 0x8180, 1, 0).name({"example", "test"}).u16(type::mx).u16(1);
        nd::DnsAnswer a;
        EXPECT_EQ(nd::dns_read_answer(b.b.data(), b.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::nodata);
        Bytes aa;
        aa.header(9, 0x8400, 1, 0).name({"example", "test"}).u16(type::mx).u16(1);
        EXPECT_EQ(nd::dns_read_answer(aa.b.data(), aa.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::nodata);
        Bytes lame;
        lame.header(9, 0x8000, 1, 0).name({"example", "test"}).u16(type::mx).u16(1);
        EXPECT_EQ(nd::dns_read_answer(lame.b.data(), lame.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::misbehaving);
    }
    // records of another class or another owner are not the answer
    {
        Bytes b;
        b.header(9, 0x8180, 1, 2).name({"example", "test"}).u16(type::mx).u16(1);
        b.pointer(12).u16(type::mx).u16(3).u32(1).u16(3).u16(1).u8(0);                      // class CH
        b.name({"evil", "test"}).u16(type::mx).u16(1).u32(1).u16(3).u16(1).u8(0);         // another owner
        nd::DnsAnswer a;
        EXPECT_EQ(nd::dns_read_answer(b.b.data(), b.b.size(), 9, qname, type::mx, false, a), nd::DnsStatus::nodata);
    }
}

TEST(NetDnsMessage_Tests, TheChainOfCNAMEs) {
    // www -> a -> b, b's MX; written out of order
    auto make = [](int links, bool loop, bool with_mx) {
        std::vector<uint8_t> m(4096);
        nd::DnsWriter w(m.data(), m.size());
        nd::DnsHeader h;
        h.id = 5;
        h.flags = 0x8180;
        h.questions = 1;
        w.header(h);
        w.question(wire("www.test"), type::mx, 1, true);
        int answers = 0;
        if (with_mx) {
            w.record_begin(wire("n" + std::to_string(links) + ".test"), type::mx, 1, 1, true);
            w.u16(3);
            w.name(wire("mail.test"), true);
            w.record_end();
            ++answers;
        }
        for (int i = links; i >= 1; --i) {
            std::string owner = i == 1 ? "www.test" : "n" + std::to_string(i - 1) + ".test";
            std::string target = (loop && i == links) ? "www.test" : "n" + std::to_string(i) + ".test";
            w.record_begin(wire(owner), type::cname, 1, 1, true);
            w.name(wire(target), true);
            w.record_end();
            ++answers;
        }
        h.answers = uint16_t(answers);
        w.set_header(h);
        m.resize(w.size());
        return m;
    };
    nd::DnsAnswer a;
    auto m = make(2, false, true);
    ASSERT_EQ(nd::dns_read_answer(m.data(), m.size(), 5, wire("www.test"), type::mx, false, a), nd::DnsStatus::ok);
    EXPECT_EQ(a.hops, 2);
    EXPECT_EQ(text_of(a.end), "n2.test.");
    ASSERT_EQ(a.records.size(), 1u);
    EXPECT_EQ(std::string(a.records[0].name.view()), "mail.test.");
    m = make(8, false, true);
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 5, wire("www.test"), type::mx, false, a), nd::DnsStatus::ok);
    m = make(9, false, true);   // past the bound
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 5, wire("www.test"), type::mx, false, a), nd::DnsStatus::misbehaving);
    m = make(3, true, false);   // a loop
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 5, wire("www.test"), type::mx, false, a), nd::DnsStatus::misbehaving);
    m = make(2, false, false);  // the chain's end has no MX here: NODATA at the end, its name for the next query
    EXPECT_EQ(nd::dns_read_answer(m.data(), m.size(), 5, wire("www.test"), type::mx, false, a), nd::DnsStatus::nodata);
    EXPECT_EQ(a.hops, 2);
    EXPECT_EQ(text_of(a.end), "n2.test.");
    // asked for CNAME itself, the CNAME is the answer, not followed
    {
        Bytes c;
        c.header(5, 0x8180, 1, 1).name({"www", "test"}).u16(type::cname).u16(1);
        c.pointer(12).u16(type::cname).u16(1).u32(1).u16(3).label("x").u8(0);
        EXPECT_EQ(nd::dns_read_answer(c.b.data(), c.b.size(), 5, wire("www.test"), type::cname, false, a), nd::DnsStatus::ok);
        ASSERT_EQ(a.records.size(), 1u);
        EXPECT_EQ(std::string(a.records[0].name.view()), "x.");
        EXPECT_EQ(a.hops, 0);
    }
}

TEST(NetDnsMessage_Tests, TheOrderOfRFC2782AndRFC5321) {
    struct S {
        sgcl::string target;
        uint16_t port = 0;
        uint16_t priority = 0;
        uint16_t weight = 0;
    };
    struct M {
        sgcl::string host;
        uint16_t preference = 0;
    };
    std::map<std::string, int> first;
    for (int round = 0; round < 2000; ++round) {
        sgcl::vector<S> v;
        v.push_back(S{"c", 1, 20, 0});
        v.push_back(S{"a", 1, 10, 90});
        v.push_back(S{"b", 1, 10, 10});
        v.push_back(S{"z", 1, 10, 0});
        v.push_back(S{"d", 1, 5, 0});
        nd::dns_order_srv(v);
        ASSERT_EQ(v.size(), 5u);
        EXPECT_EQ(std::string(v[0].target.view()), "d");
        EXPECT_EQ(std::string(v[4].target.view()), "c");
        EXPECT_EQ(v[1].priority, 10);
        EXPECT_EQ(v[3].priority, 10);
        ++first[std::string(v[1].target.view())];
    }
    // weight 90 against 10 against 0: a first about 9 times in 10, z (weight 0) only when the pick is 0
    EXPECT_GT(first["a"], 1600);
    EXPECT_GT(first["b"], 100);
    EXPECT_LT(first["z"], 40);
    std::map<std::string, int> mx_first;
    for (int round = 0; round < 1000; ++round) {
        sgcl::vector<M> v;
        v.push_back(M{"far", 50});
        v.push_back(M{"x", 10});
        v.push_back(M{"y", 10});
        v.push_back(M{"near", 0});
        nd::dns_order_mx(v);
        EXPECT_EQ(std::string(v[0].host.view()), "near");
        EXPECT_EQ(std::string(v[3].host.view()), "far");
        ++mx_first[std::string(v[1].host.view())];
    }
    EXPECT_GT(mx_first["x"], 350);
    EXPECT_GT(mx_first["y"], 350);
    sgcl::vector<S> none;
    nd::dns_order_srv(none);
    sgcl::vector<M> one;
    one.push_back(M{"only", 1});
    nd::dns_order_mx(one);
    EXPECT_EQ(one.size(), 1u);
}
