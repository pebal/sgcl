//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// HPACK (sgcl/net/http/detail/h2/hpack.h) against RFC 7541 read from its
// text (rfc7541.h): the static table and the Huffman code of the header
// against Appendices A and B; the integers of C.1; the Huffman code both
// ways over every octet and the strings of C.4 and C.6; the faults of the
// primitives; then the decoder and the encoder over every example of
// Appendix C, byte for byte both ways, with the dynamic table after each.
#include "tests/types.h"
#include "sgcl/net/http/detail/h2/hpack.h"
#include "rfc7541.h"

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace h2 = sgcl::net::http::detail::h2;

namespace {
    const std::vector<std::string>& rfc() {
        static const auto ls = rfc7541::lines();
        return ls;
    }

    std::string bytes(std::initializer_list<int> b) {
        std::string s;
        for (int x : b) {
            s.push_back(char(uint8_t(x)));
        }
        return s;
    }

    std::string hex(const std::string& s) {
        static const char* d = "0123456789abcdef";
        std::string out;
        for (unsigned char c : s) {
            out += d[c >> 4];
            out += d[c & 15];
        }
        return out;
    }
}

TEST(Hpack_Tests, TheTablesAreTheRfcs) {
    ASSERT_FALSE(rfc().empty()) << "RFC 7541 not found at " << rfc7541::path();
    auto codes = rfc7541::huffman(rfc());
    ASSERT_EQ(codes.size(), 257u);
    for (auto& c : codes) {
        ASSERT_GE(c.symbol, 0);
        ASSERT_LE(c.symbol, 256);
        EXPECT_EQ(h2::huffman_codes[c.symbol].code, c.code) << c.symbol;
        EXPECT_EQ(h2::huffman_codes[c.symbol].bits, c.bits) << c.symbol;
    }
    // Appendix A: "| i | name | value |" rows
    bool in = false;
    int rows = 0;
    for (auto& l : rfc()) {
        if (l.rfind("Appendix A.", 0) == 0) {
            in = true;
            continue;
        }
        if (l.rfind("Appendix B.", 0) == 0) {
            break;
        }
        if (!in) {
            continue;
        }
        auto t = rfc7541::trim(l);
        if (t.size() < 3 || t[0] != '|' || !std::isdigit((unsigned char)rfc7541::trim(t.substr(1))[0])) {
            continue;
        }
        std::vector<std::string> cells;
        size_t at = 1;
        for (;;) {
            auto bar = t.find('|', at);
            if (bar == std::string::npos) {
                break;
            }
            cells.push_back(rfc7541::trim(t.substr(at, bar - at)));
            at = bar + 1;
        }
        ASSERT_EQ(cells.size(), 3u) << l;
        int i = std::atoi(cells[0].c_str());
        ASSERT_GE(i, 1);
        ASSERT_LE(i, 61);
        EXPECT_EQ(h2::static_table[i - 1].name, cells[1]) << i;
        EXPECT_EQ(h2::static_table[i - 1].value, cells[2]) << i;
        ++rows;
    }
    EXPECT_EQ(rows, 61);
}

// C.1: 10 on 5 bits, 1337 on 5 bits, 42 on 8 bits; both ways
TEST(Hpack_Tests, TheIntegersOfC1) {
    struct Case {
        uint32_t value;
        int prefix;
        std::string encoded;
    };
    for (auto& c : {Case{10, 5, bytes({0x0a})}, Case{1337, 5, bytes({0x1f, 0x9a, 0x0a})}, Case{42, 8, bytes({0x2a})}}) {
        std::string out;
        h2::put_integer(out, 0, c.prefix, c.value);
        EXPECT_EQ(hex(out), hex(c.encoded)) << c.value;
        auto p = reinterpret_cast<const uint8_t*>(c.encoded.data());
        uint32_t v = 0;
        ASSERT_EQ(h2::get_integer(p, p + c.encoded.size(), c.prefix, v), h2::HpackFault::none);
        EXPECT_EQ(v, c.value);
        EXPECT_EQ(p, reinterpret_cast<const uint8_t*>(c.encoded.data()) + c.encoded.size());
    }
    // the other bits of the first octet kept; every prefix and edges round trip
    for (int prefix = 1; prefix <= 8; ++prefix) {
        const uint8_t first = prefix == 8 ? 0 : uint8_t(0xff << prefix);
        for (uint64_t v : {0ull, 1ull, (1ull << prefix) - 2, (1ull << prefix) - 1, (1ull << prefix), 127ull, 128ull, 16383ull, 16384ull, 0xFFFFFFFFull}) {
            std::string out;
            h2::put_integer(out, first, prefix, v);
            EXPECT_EQ(uint8_t(out[0]) & first, first);
            auto p = reinterpret_cast<const uint8_t*>(out.data());
            uint32_t got = 0;
            ASSERT_EQ(h2::get_integer(p, p + out.size(), prefix, got), h2::HpackFault::none) << prefix << " " << v;
            EXPECT_EQ(got, v) << prefix;
        }
    }
}

TEST(Hpack_Tests, TheIntegersThatFail) {
    auto fault = [](const std::string& s, int prefix) {
        auto p = reinterpret_cast<const uint8_t*>(s.data());
        uint32_t v = 0;
        return h2::get_integer(p, p + s.size(), prefix, v);
    };
    EXPECT_EQ(fault("", 5), h2::HpackFault::truncated);
    EXPECT_EQ(fault(bytes({0x1f}), 5), h2::HpackFault::truncated);               // the continuation missing
    EXPECT_EQ(fault(bytes({0x1f, 0x9a}), 5), h2::HpackFault::truncated);         // its last octet missing
    EXPECT_EQ(fault(bytes({0x1f, 0xff, 0xff, 0xff, 0xff, 0x0f}), 5), h2::HpackFault::integer_overflow);   // past 2^32 - 1
    EXPECT_EQ(fault(bytes({0x1f, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}), 5), h2::HpackFault::integer_overflow);   // zeros padding it past 5 octets
    // 2^32 - 1 itself reads
    std::string max;
    h2::put_integer(max, 0, 5, 0xFFFFFFFFu);
    EXPECT_EQ(fault(max, 5), h2::HpackFault::none);
}

// Every octet alone and all of them together, both ways; and the strings of
// C.4.1 ("www.example.com") and C.6.1 as the RFC codes them
TEST(Hpack_Tests, TheHuffmanCodeBothWays) {
    auto round = [](const std::string& s) {
        std::string enc;
        h2::huffman_encode(enc, s);
        EXPECT_EQ(enc.size(), h2::huffman_length(s));
        std::string dec;
        EXPECT_EQ(h2::huffman_decode(dec, reinterpret_cast<const uint8_t*>(enc.data()), enc.size()), h2::HpackFault::none) << hex(s);
        EXPECT_EQ(dec, s);
        return enc;
    };
    for (int c = 0; c < 256; ++c) {
        round(std::string(1, char(c)));
        round(std::string(5, char(c)));
    }
    std::string all;
    for (int c = 0; c < 256; ++c) {
        all.push_back(char(c));
    }
    round(all);
    round("");
    EXPECT_EQ(hex(round("www.example.com")), "f1e3c2e5f23a6ba0ab90f4ff");
    EXPECT_EQ(hex(round("no-cache")), "a8eb10649cbf");
    EXPECT_EQ(hex(round("custom-key")), "25a849e95ba97d7f");
    EXPECT_EQ(hex(round("custom-value")), "25a849e95bb8e8b4bf");
    EXPECT_EQ(hex(round("302")), "6402");
    EXPECT_EQ(hex(round("private")), "aec3771a4b");
    EXPECT_EQ(hex(round("Mon, 21 Oct 2013 20:13:21 GMT")), "d07abe941054d444a8200595040b8166e082a62d1bff");
    EXPECT_EQ(hex(round("https://www.example.com")), "9d29ad171863c78f0b97c8e9ae82ae43d3");
}

TEST(Hpack_Tests, TheHuffmanStringsThatFail) {
    auto fault = [](const std::string& s) {
        std::string out;
        return h2::huffman_decode(out, reinterpret_cast<const uint8_t*>(s.data()), s.size());
    };
    std::string a;
    h2::huffman_encode(a, "a");            // 00011 + 111 padding
    EXPECT_EQ(fault(a), h2::HpackFault::none);
    EXPECT_EQ(fault(a + bytes({0xff})), h2::HpackFault::huffman_padding);   // 11 bits of padding
    EXPECT_EQ(fault(bytes({0x18})), h2::HpackFault::huffman_padding);       // 00011 000: padding of zeros
    EXPECT_EQ(fault(bytes({0x1a})), h2::HpackFault::huffman_padding);       // 00011 010: padding not all ones
    EXPECT_EQ(fault(bytes({0xff})), h2::HpackFault::huffman_padding);       // 8 ones: padding of 8 bits
    EXPECT_EQ(fault(bytes({0xff, 0xff, 0xff, 0xff})), h2::HpackFault::huffman_eos);   // EOS (30 ones) inside
    EXPECT_EQ(fault(bytes({0xfe})), h2::HpackFault::huffman_padding);       // 7 ones then a zero: a code's prefix cut off
    EXPECT_EQ(fault(""), h2::HpackFault::none);
}

namespace {
    sgcl::slice<const std::byte> view(const std::string& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    sgcl::slice<const std::byte> view(const std::vector<uint8_t>& s) {
        return sgcl::slice<const std::byte>(reinterpret_cast<const std::byte*>(s.data()), s.size());
    }

    std::vector<std::pair<std::string, std::string>> fields_of(const h2::Block& b) {
        std::vector<std::pair<std::string, std::string>> out;
        for (auto& f : sgcl::net::http::detail::HeadersAccess::fields(b.fields)) {
            out.emplace_back(std::string(f.first.view()), std::string(f.second.view()));
        }
        return out;
    }

    void same_table(const h2::DynamicTable& t, const rfc7541::Example& e, const char* who) {
        EXPECT_EQ(t.size(), e.table_size) << who << " " << e.section;
        ASSERT_EQ(t.count(), e.table.size()) << who << " " << e.section;
        for (size_t i = 0; i < e.table.size(); ++i) {
            auto v = t.entry(i + 1);
            EXPECT_EQ(std::string(v.name), e.table[i].name) << who << " " << e.section << " [" << i + 1 << "]";
            EXPECT_EQ(std::string(v.value), e.table[i].value) << who << " " << e.section << " [" << i + 1 << "]";
            EXPECT_EQ(v.name.size() + v.value.size() + 32, e.table[i].size) << who << " " << e.section;
        }
    }

    // The group of an example: C.2.x each alone; C.3, C.4, C.5, C.6 one
    // connection each, the responses (C.5, C.6) with a table of 256
    std::string group(const std::string& section) {
        return section.substr(0, 3) == "C.2" ? section : section.substr(0, 3);
    }
}

// Every example of Appendix C decoded: the header list and the dynamic
// table after each, as the RFC has them; and encoded (C.2.1, C.2.4, C.3 to
// C.6: the strategy of the examples, every field indexed, Huffman in C.4
// and C.6 and in none of the others) byte for byte, the encoder's table the
// decoder's
TEST(Hpack_Tests, AppendixCBothWays) {
    auto examples = rfc7541::examples(rfc());
    ASSERT_GE(examples.size(), 16u);
    std::string current;
    std::unique_ptr<h2::Decoder> dec;
    std::unique_ptr<h2::Encoder> enc;
    size_t decoded = 0, encoded = 0;
    for (auto& e : examples) {
        auto g = group(e.section);
        if (g != current) {
            current = g;
            const uint32_t size = (g == "C.5" || g == "C.6") ? 256 : 4096;
            const bool huffman = g == "C.4" || g == "C.6";
            dec = std::make_unique<h2::Decoder>(size);
            enc = std::make_unique<h2::Encoder>(size, huffman ? h2::Encoder::Huffman::always : h2::Encoder::Huffman::never);   // C.4 and C.6 code every string, even "307" (as long either way)
        }
        auto r = dec->decode(view(e.encoded), 1 << 20);
        ASSERT_TRUE(r.has_value()) << e.section << ": " << r.error().what;
        EXPECT_FALSE(r->truncated);
        EXPECT_EQ(fields_of(*r), e.fields) << e.section;
        same_table(dec->table(), e, "decoder");
        ++decoded;
        const bool encodable = g.rfind("C.2", 0) != 0 || e.section == "C.2.1" || e.section == "C.2.4";
        if (encodable) {
            std::string out;
            enc->begin_block(out);
            for (auto& [n, v] : e.fields) {
                enc->encode(out, n, v);
            }
            EXPECT_EQ(hex(out), hex(std::string(e.encoded.begin(), e.encoded.end()))) << e.section;
            same_table(enc->table(), e, "encoder");
            ++encoded;
        }
    }
    EXPECT_EQ(decoded, 16u);
    EXPECT_EQ(encoded, 14u);
}

// The faults of a block, each a COMPRESSION_ERROR of the connection
TEST(Hpack_Tests, TheBlocksThatFail) {
    auto fails = [](const std::string& block, const char* expected, uint32_t table = 4096) {
        h2::Decoder d(table);
        auto r = d.decode(view(block), 1 << 20);
        EXPECT_FALSE(r.has_value()) << hex(block);
        if (!r) {
            EXPECT_EQ(r.error().code, h2::ErrorCode::compression_error);
            EXPECT_TRUE(r.error().connection());
            EXPECT_EQ(std::string(r.error().what), std::string(expected)) << hex(block);
        }
    };
    using F = h2::HpackFault;
    fails(bytes({0x80}), h2::hpack_fault_text(F::index_zero));                     // indexed 0
    fails(bytes({0xbe}), h2::hpack_fault_text(F::index_past_table));               // 62, an empty dynamic table
    fails(bytes({0xff, 0x80, 0x80, 0x80, 0x80, 0x10}), h2::hpack_fault_text(F::integer_overflow));
    fails(bytes({0xff}), h2::hpack_fault_text(F::truncated));                      // an index cut off
    fails(bytes({0x40, 0x05, 'a', 'b'}), h2::hpack_fault_text(F::string_past_block));   // a name of 5 bytes, 2 there
    fails(bytes({0x40, 0x81, 0x18, 0x00}), h2::hpack_fault_text(F::huffman_padding));   // name "a" with zeros for padding
    fails(bytes({0x40, 0x84, 0xff, 0xff, 0xff, 0xff, 0x00}), h2::hpack_fault_text(F::huffman_eos));
    fails(bytes({0x40, 0x82, 0x1f, 0xff, 0x00}), h2::hpack_fault_text(F::huffman_padding));   // "a" and 11 ones
    fails(bytes({0x3f, 0xe2, 0x1f}), h2::hpack_fault_text(F::table_size_past_limit));   // 4097 > 4096
    fails(bytes({0x82, 0x20}), h2::hpack_fault_text(F::table_size_misplaced));   // an update after a field
    fails(bytes({0x20, 0x20, 0x20}), h2::hpack_fault_text(F::table_size_misplaced));   // a third one
    fails(bytes({0x40, 0x01}), h2::hpack_fault_text(F::string_past_block));       // a name of 1 byte, none there
    fails(bytes({0x40}), h2::hpack_fault_text(F::truncated));                     // the name's length missing
    fails(bytes({0x40, 0x01, 'a'}), h2::hpack_fault_text(F::truncated));          // the value missing
    // our SETTINGS lowered: the next block must begin with an update
    h2::Decoder d(4096);
    d.set_max_table_size(1024);
    auto r = d.decode(view(bytes({0x82})), 1 << 20);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(std::string(r.error().what), h2::hpack_fault_text(F::table_size_missing));
    h2::Decoder e(4096);
    e.set_max_table_size(1024);
    EXPECT_TRUE(e.decode(view(bytes({0x3f, 0xe1, 0x07, 0x82})), 1 << 20).has_value());   // 1024 then GET
    EXPECT_FALSE(e.decode(view(bytes({0x3f, 0xe2, 0x07})), 1 << 20).has_value());       // 1025 > the new SETTINGS
    // two updates at the start are fine; a table of 0 empties it
    h2::Decoder z(4096);
    ASSERT_TRUE(z.decode(view(bytes({0x40, 0x01, 'a', 0x01, 'b'})), 1 << 20).has_value());
    EXPECT_EQ(z.table().count(), 1u);
    ASSERT_TRUE(z.decode(view(bytes({0x20, 0x3f, 0xe1, 0x1f, 0x82})), 1 << 20).has_value());
    EXPECT_EQ(z.table().count(), 0u);
    EXPECT_EQ(z.table().limit(), 4096u);
}

// The bomb: a list past max_list_size is decoded to its end without its
// fields kept (truncated), the table as the peer's encoder left it, and the
// next block decodes from that table
TEST(Hpack_Tests, AListPastTheLimitIsTruncatedAndTheTableKept) {
    h2::Encoder enc(4096, h2::Encoder::Huffman::shorter);
    h2::Decoder dec(4096);
    std::string first;
    enc.begin_block(first);
    std::vector<std::pair<std::string, std::string>> big;
    for (int i = 0; i < 40; ++i) {
        big.emplace_back("x-field-" + std::to_string(i), std::string(60, char('a' + i % 26)));
    }
    for (auto& [n, v] : big) {
        enc.encode(first, n, v);
    }
    auto r = dec.decode(view(first), 1024);   // 40 fields of about 100 bytes: past 1024
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->truncated);
    EXPECT_TRUE(r->bytes.empty());
    EXPECT_EQ(sgcl::net::http::detail::HeadersAccess::fields(r->fields).size(), 0u);
    // the decoder's table is the encoder's
    ASSERT_EQ(dec.table().count(), enc.table().count());
    EXPECT_EQ(dec.table().size(), enc.table().size());
    for (size_t i = 1; i <= enc.table().count(); ++i) {
        EXPECT_EQ(dec.table().entry(i).name, enc.table().entry(i).name);
        EXPECT_EQ(dec.table().entry(i).value, enc.table().entry(i).value);
    }
    // the next block refers to those entries, and decodes whole
    std::string second;
    enc.begin_block(second);
    std::vector<std::pair<std::string, std::string>> small = {{":method", "GET"}, big[39], big[38], {"x-new", "1"}};
    for (auto& [n, v] : small) {
        enc.encode(second, n, v);
    }
    EXPECT_LT(second.size(), 20u);   // indexes, not literals
    auto s = dec.decode(view(second), 1024);
    ASSERT_TRUE(s.has_value());
    EXPECT_FALSE(s->truncated);
    EXPECT_EQ(fields_of(*s), small);
}

// The encoder's strategy: never indexed for the fields of secrets, a large
// field not indexed, Huffman only when shorter; and whatever it writes, the
// decoder reads back, the tables the same after each block, across changes
// of the peer's table size
TEST(Hpack_Tests, TheEncodersStrategyAndItsRoundTrip) {
    h2::Encoder enc;
    std::string out;
    enc.encode(out, "authorization", "Bearer abc");
    EXPECT_EQ(uint8_t(out[0]) & 0xf0, 0x10);                  // never indexed, the name an index (23)
    EXPECT_EQ(uint8_t(out[0]) & 0x0f, 0x0f);
    EXPECT_EQ(enc.table().count(), 0u);
    out.clear();
    enc.encode(out, "cookie", "a=1");
    EXPECT_EQ(uint8_t(out[0]) & 0xf0, 0x10);
    out.clear();
    enc.encode(out, "x-large", std::string(3100, 'q'));        // 3139 > 3/4 of 4096
    EXPECT_EQ(uint8_t(out[0]), 0x00);
    EXPECT_EQ(enc.table().count(), 0u);
    out.clear();
    enc.encode(out, "x-bin", std::string("\xff\xfe\xfd", 3));  // Huffman longer: raw
    EXPECT_EQ(uint8_t(out[out.size() - 4]), 0x03);
    // round trips with the peer's table changing between blocks
    h2::Encoder e2;
    h2::Decoder d2(4096);
    std::mt19937 rng(7);
    for (int blockno = 0; blockno < 200; ++blockno) {
        if (blockno % 37 == 36) {
            uint32_t n = uint32_t(rng() % 4097);
            e2.set_peer_max_table_size(n);   // what the peer's SETTINGS would say; its decoder allows up to 4096
        }
        std::string block;
        e2.begin_block(block);
        std::vector<std::pair<std::string, std::string>> fields;
        int n = int(rng() % 12);
        for (int i = 0; i < n; ++i) {
            std::string name = "x-" + std::to_string(rng() % 20);
            std::string value(rng() % 50, char('a' + rng() % 26));
            if (rng() % 7 == 0) {
                name = "cookie";
            }
            fields.emplace_back(name, value);
            e2.encode(block, name, value);
        }
        auto r = d2.decode(view(block), 1 << 20);
        ASSERT_TRUE(r.has_value()) << blockno << ": " << r.error().what;
        EXPECT_EQ(fields_of(*r), fields) << blockno;
        ASSERT_EQ(d2.table().count(), e2.table().count()) << blockno;
        EXPECT_EQ(d2.table().size(), e2.table().size()) << blockno;
    }
}
