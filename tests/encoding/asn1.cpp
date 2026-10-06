//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// asn1: X.690's BER and DER, read and written. Against Go's encoding/asn1
// (tools/asn1_oracle.go writes asn1_tests.h): the DER of every value Go
// writes, the tree of named inputs at the edge of every rule and of 1500
// random structures, every difference named with its reason; against
// OpenSSL: a CMS streamed in BER (indefinite lengths, an OCTET STRING in
// pieces) read and written as DER is byte for byte the DER `openssl cms
// -cmsout` writes of it. Then BER's forms one by one, the boundaries of
// every member, the stream reading, and the OBJECT IDENTIFIER.
#include "common.h"
#include "asn1_tests.h"
#include "tests/source_root.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

using namespace sgcl::encoding;
using namespace enc_test;
using sgcl::encoding::asn1;

namespace {
    std::string hex_of(sgcl::slice<const byte> b) {
        static constexpr char digits[] = "0123456789abcdef";
        std::string s;
        for (auto x : b) {
            s += digits[uint8_t(x) >> 4];
            s += digits[uint8_t(x) & 15];
        }
        return s;
    }

    std::string hex_of(std::string_view t) {
        return hex_of(sgcl::slice<const byte>(reinterpret_cast<const byte*>(t.data()), t.size()));
    }

    sgcl::vector<byte> managed(std::string_view hex) {
        std::string compact;
        for (char c : hex) {
            if (c != ' ') {
                compact += c;
            }
        }
        auto v = from_hex(compact);
        return sgcl::vector<byte>(v.begin(), v.end());
    }

    // The tree in the oracle's form (tools/asn1_oracle.go: walk)
    void tree_of(const asn1& e, int depth, std::string& out) {
        out.append(size_t(depth), ' ');
        out += std::to_string(int(e.cls())) + ":" + std::to_string(e.tag());
        if (e.constructed()) {
            out += "c\n";
            for (auto c : e) {
                tree_of(c, depth + 1, out);
            }
            return;
        }
        out += ' ';
        if (e.cls() != asn1::tag_class::universal) {
            out += hex_of(e.content());
        } else {
            switch (e.tag()) {
                case 1: out += *e.as_bool() ? "true" : "false"; break;
                case 2: out += std::string(e.as_big_integer()->to_string().view()); break;
                case 10: out += std::to_string(*e.as_int()); break;
                case 6: out += std::string(e.as_oid()->to_string().view()); break;
                case 3: {
                    auto b = *e.as_bits();
                    out += std::to_string(b.length) + " " + hex_of(b.bytes);
                    break;
                }
                case 5: break;
                case 12:
                case 18:
                case 19:
                case 22:
                case 30: out += hex_of(e.as_string()->view()); break;
                case 23:
                case 24: {
                    auto t = *e.as_time();
                    char ns[16];
                    std::snprintf(ns, sizeof ns, "%09d", t.nanosecond());
                    out += std::to_string(t.unix()) + "." + ns;
                    break;
                }
                default: out += hex_of(e.content()); break;
            }
        }
        out += '\n';
    }

    std::string tree(std::string_view hex, const asn1::options& o = asn1::der) {
        auto bytes = managed(hex);
        auto e = asn1::parse(bytes, o);
        if (!e) {
            return "error";
        }
        std::string out;
        tree_of(*e, 0, out);
        return out;
    }

    std::string run(const std::string& command) {
        std::string out;
        FILE* f = popen(command.c_str(), "r");
        if (!f) {
            return out;
        }
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
            out.append(buf, n);
        }
        pclose(f);
        return out;
    }

    const char* const openssl = "/opt/homebrew/bin/openssl";
}

// --- against Go's Marshal ---

TEST(Asn1_Tests, IntegersAsGoWritesThem) {
    for (auto& c : asn1_oracle::integers) {
        sgcl::math::big_integer v(sgcl::string(c.decimal));
        EXPECT_EQ(hex_of(asn1::integer(v).bytes()), c.der) << c.decimal;
        auto bytes = managed(c.der);
        auto e = asn1::parse(bytes);
        ASSERT_TRUE(e) << c.decimal;
        EXPECT_EQ(std::string(e->as_big_integer()->to_string().view()), c.decimal);
        if (v >= sgcl::math::big_integer(INT64_MIN) && v <= sgcl::math::big_integer(INT64_MAX)) {
            int64_t small = std::stoll(std::string(c.decimal));
            EXPECT_EQ(hex_of(asn1::integer(small).bytes()), c.der) << c.decimal;
            ASSERT_TRUE(e->as_int());
            EXPECT_EQ(*e->as_int(), small);
        } else {
            EXPECT_FALSE(e->as_int()) << c.decimal;
        }
    }
    // the unsigned 64-bit path and the narrow types
    EXPECT_EQ(hex_of(asn1::integer(uint64_t(-1)).bytes()), "020900ffffffffffffffff");
    EXPECT_EQ(hex_of(asn1::integer(uint8_t(255)).bytes()), "020200ff");
    EXPECT_EQ(hex_of(asn1::integer(int8_t(-128)).bytes()), "020180");
    EXPECT_EQ(hex_of(asn1::integer(short(-1)).bytes()), "0201ff");
    EXPECT_EQ(hex_of(asn1::integer(__int128(1) << 100).bytes()), "020d10000000000000000000000000");
}

TEST(Asn1_Tests, EnumeratedAsGoWritesIt) {
    for (auto& c : asn1_oracle::enumerateds) {
        EXPECT_EQ(hex_of(asn1::enumerated(c.value).bytes()), c.der);
        auto bytes = managed(c.der);
        auto e = asn1::parse(bytes);
        ASSERT_TRUE(e);
        EXPECT_TRUE(e->is(asn1::type::enumerated));
        EXPECT_EQ(*e->as_int(), c.value);
    }
}

TEST(Asn1_Tests, BitStringsAsGoWritesThem) {
    for (auto& c : asn1_oracle::bit_strings) {
        auto bytes = from_hex(c.bytes);
        EXPECT_EQ(hex_of(asn1::bit_string(as_slice(bytes), c.length).bytes()), c.der) << c.length;
        auto der = managed(c.der);
        auto e = asn1::parse(der);
        ASSERT_TRUE(e);
        auto b = e->as_bits();
        ASSERT_TRUE(b);
        EXPECT_EQ(b->length, c.length);
        for (size_t i = 0; i < c.length; ++i) {
            bool want = (uint8_t(bytes[i / 8]) >> (7 - i % 8)) & 1;
            ASSERT_EQ((*b)[i], want) << c.length << " " << i;
        }
        if (c.length % 8 == 0) {
            EXPECT_EQ(hex_of(asn1::bit_string(as_slice(bytes)).bytes()), c.der);
        }
    }
    std::vector<byte> two(2);
    EXPECT_THROW(asn1::bit_string(as_slice(two), 17), std::invalid_argument);
    EXPECT_EQ(hex_of(asn1::bit_string(as_slice(two), 16).bytes()), "0303000000");
}

TEST(Asn1_Tests, OctetStringsAsGoWritesThem) {
    for (auto& c : asn1_oracle::octet_strings) {
        size_t n = std::stoul(std::string(c.bytes));
        std::vector<byte> b(n);
        for (size_t i = 0; i < n; ++i) {
            b[i] = byte(uint8_t(i * 31));
        }
        auto e = asn1::octet_string(as_slice(b));
        EXPECT_EQ(hex_of(e.bytes()).substr(0, c.der.size()), c.der) << n;
        EXPECT_EQ(e.bytes().size(), c.der.size() / 2 + n);
        auto back = asn1::parse(e.bytes());
        ASSERT_TRUE(back);
        ASSERT_TRUE(back->as_bytes());
        EXPECT_EQ(back->as_bytes()->size(), n);
        EXPECT_EQ(std::memcmp(back->as_bytes()->data(), b.data(), n), 0);
    }
}

TEST(Asn1_Tests, ObjectIdentifiersAsGoWritesThem) {
    for (auto& c : asn1_oracle::oids) {
        auto id = asn1::oid::parse(sgcl::string(c.dotted));
        ASSERT_TRUE(id) << c.dotted;
        EXPECT_EQ(hex_of(asn1::object_identifier(*id).bytes()), c.der) << c.dotted;
        auto bytes = managed(c.der);
        auto e = asn1::parse(bytes);
        ASSERT_TRUE(e);
        EXPECT_EQ(std::string(e->as_oid()->to_string().view()), c.dotted);
        EXPECT_EQ(*e->as_oid(), *id);
        // the arcs one by one, and from them
        std::vector<uint64_t> arcs;
        for (auto part : sgcl::string(c.dotted).split('.')) {
            arcs.push_back(std::stoull(std::string(part.view())));
        }
        ASSERT_EQ(id->size(), arcs.size()) << c.dotted;
        for (size_t i = 0; i < arcs.size(); ++i) {
            EXPECT_EQ(id->arc(i), arcs[i]) << c.dotted << " " << i;
        }
        EXPECT_FALSE(id->arc(arcs.size()));
    }
}

TEST(Asn1_Tests, StringsAsGoWritesThem) {
    for (auto& c : asn1_oracle::strings) {
        sgcl::string text(c.text);
        asn1 e = c.kind == 12 ? asn1::utf8_string(text)
               : c.kind == 19 ? asn1::printable_string(text)
               : c.kind == 22 ? asn1::ia5_string(text)
                              : asn1::numeric_string(text);
        EXPECT_EQ(hex_of(e.bytes()), c.der) << c.text;
        auto bytes = managed(c.der);
        auto back = asn1::parse(bytes);
        ASSERT_TRUE(back);
        EXPECT_EQ(back->as_string(), text);
    }
}

TEST(Asn1_Tests, TimesAsGoWritesThem) {
    for (auto& c : asn1_oracle::times) {
        auto t = sgcl::time::datetime::from_unix(c.seconds, sgcl::time::zone::utc());
        auto e = c.kind == 23 ? asn1::utc_time(t) : asn1::generalized_time(t);
        EXPECT_EQ(hex_of(e.bytes()), c.der) << c.seconds;
        auto bytes = managed(c.der);
        auto back = asn1::parse(bytes);
        ASSERT_TRUE(back);
        EXPECT_EQ(back->as_time()->unix(), c.seconds);
        EXPECT_EQ(back->as_time()->zone(), sgcl::time::zone::utc());
    }
}

TEST(Asn1_Tests, TaggedStructureAsGoWritesIt) {
    // Go: struct { A int; B int `explicit,tag:0`; C int `tag:1`;
    // D string `utf8,application,tag:5`; E bool `private,explicit,tag:31`;
    // F []byte `tag:200`; G []int `optional,explicit,tag:3`; H []int `set` }
    std::vector<byte> f{byte(1), byte(2), byte(3)};
    auto s = asn1::sequence({
        asn1::integer(5),
        asn1::explicit_tag(0, asn1::integer(-1)),
        asn1::implicit_tag(1, asn1::integer(300)),
        asn1::implicit_tag(5, asn1::utf8_string("żółw"), asn1::tag_class::application),
        asn1::explicit_tag(31, asn1::boolean(true), asn1::tag_class::private_use),
        asn1::implicit_tag(200, asn1::octet_string(as_slice(f))),
        asn1::explicit_tag(3, asn1()),   // an absent OPTIONAL
        asn1::set({asn1::integer(300), asn1::integer(2), asn1::integer(1), asn1::integer(70000), asn1::integer(-1)}),
    });
    EXPECT_EQ(hex_of(s.bytes()), asn1_oracle::tagged_structure);
    auto back = asn1::parse(s.bytes());
    ASSERT_TRUE(back);
    EXPECT_EQ(*back, s);
    EXPECT_EQ(back->size(), 7u);
    EXPECT_EQ(*(*back)[1][0].as_int(), -1);
    EXPECT_EQ(*(*back)[2].as_int(), 300);
    EXPECT_TRUE((*back)[2].is_context(1));
    EXPECT_EQ(*(*back)[3].as_string(), "żółw");
    EXPECT_EQ((*back)[3].cls(), asn1::tag_class::application);
    EXPECT_EQ(*(*back)[4][0].as_bool(), true);
    EXPECT_EQ((*back)[4].tag(), 31u);
    EXPECT_EQ((*back)[5].tag(), 200u);
    EXPECT_EQ((*back)[5].as_bytes()->size(), 3u);
    // DER's order: 020101, 020102, 0201ff, 0202012c, 0203011170
    EXPECT_EQ(*(*back)[6][0].as_int(), 1);
    EXPECT_EQ(*(*back)[6][2].as_int(), -1);
    EXPECT_EQ(*(*back)[6][4].as_int(), 70000);
}

// --- against Go's Unmarshal ---

TEST(Asn1_Tests, NamedInputsAsGoReadsThem) {
    // where X.690 and Go differ, by name: what the reading here gives
    std::map<std::string_view, std::string> differs = {
        // X.690 §41.4: no '*' or '&' in a PrintableString; Go takes them
        // for the certificates that have them
        {"printable asterisk", "error"},
        {"printable ampersand", "error"},
        // X.690 §11.8: DER's UTCTime is YYMMDDHHMMSSZ; Go reads BER's forms
        {"utctime no seconds", "error"},
        {"utctime offset", "error"},
        // X.690 §11.7: DER's GeneralizedTime ends in Z
        {"generalized offset", "error"},
        // an arc past 31 bits is an arc (2.25.<a UUID> has 128); Go refuses
        {"oid arc past 32 bits", "0:6 1.2.549755813887\n"},
        // a time past 2262 is the last instant a datetime holds
        {"generalized 9999", "0:24 9223372036.854775807\n"},
    };
    for (auto& c : asn1_oracle::parses) {
        auto it = differs.find(c.name);
        std::string want = it == differs.end() ? std::string(c.tree) : it->second;
        EXPECT_EQ(tree(c.der), want) << c.name;
    }
    // what DER refuses of Go's readings, BER reads as Go does
    for (auto name : {"utctime no seconds", "utctime offset", "generalized offset"}) {
        for (auto& c : asn1_oracle::parses) {
            if (c.name == name) {
                auto t = tree(c.der, asn1::ber);
                EXPECT_EQ(t, c.tree) << c.name;
            }
        }
    }
}

TEST(Asn1_Tests, RandomStructuresAsGoReadsThem) {
    for (auto& c : asn1_oracle::randoms) {
        ASSERT_EQ(tree(c.der), c.tree) << c.der;
        // and written back the same: every element remade from its value
        auto bytes = managed(c.der);
        auto e = asn1::parse(bytes);
        ASSERT_TRUE(e);
        EXPECT_EQ(hex_of(e->bytes()), c.der);
    }
}

// --- against OpenSSL: BER in, DER out ---

TEST(Asn1_Tests, StreamedCmsAsOpenSslReencodesIt) {
    if (!std::filesystem::exists(openssl)) {
        GTEST_SKIP() << "no " << openssl;
    }
    auto dir = std::filesystem::temp_directory_path() / ("sgcl_asn1_" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    auto d = dir.string();
    std::string sh = "cd '" + d + "' && " + openssl + " req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -keyout k.pem -out c.pem -subj /CN=t -days 1 2>/dev/null"
                     " && head -c 70000 /dev/urandom > m.bin"
                     " && " + openssl + " cms -sign -stream -binary -in m.bin -signer c.pem -inkey k.pem -outform DER -out ber.der"
                     " && " + openssl + " cms -cmsout -inform DER -in ber.der -outform DER -out der.der"
                     " && " + openssl + " pkcs12 -export -in c.pem -inkey k.pem -passout pass:x -out p.p12 && echo ok";
    ASSERT_EQ(run(sh), "ok\n");
    auto read = [&](const char* name) {
        std::ifstream f(dir / name, std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        return sgcl::vector<byte>(reinterpret_cast<const byte*>(s.data()), reinterpret_cast<const byte*>(s.data() + s.size()));
    };
    auto ber = read("ber.der");
    auto der = read("der.der");
    EXPECT_FALSE(asn1::parse(ber));   // indefinite lengths: not DER
    auto e = asn1::parse(ber, asn1::ber);
    ASSERT_TRUE(e) << e.error().message();
    EXPECT_EQ(hex_of(e->bytes()), hex_of(der.as_slice()));
    auto strict = asn1::parse(der);
    ASSERT_TRUE(strict);
    EXPECT_EQ(*strict, *e);
    // the content signed: [0] EXPLICIT OCTET STRING, 70000 bytes once joined
    auto content = (*e)[1][0][2][1][0];
    ASSERT_TRUE(content.is(asn1::type::octet_string));
    auto m = read("m.bin");
    EXPECT_EQ(content.as_bytes()->size(), m.size());
    EXPECT_EQ(std::memcmp(content.as_bytes()->data(), m.data(), m.size()), 0);
    // a PKCS #12 OpenSSL writes is DER
    auto p12 = read("p.p12");
    EXPECT_TRUE(asn1::parse(p12));
    std::filesystem::remove_all(dir);
}

// --- BER's forms ---

TEST(Asn1_Tests, BerIndefiniteLengths) {
    auto in = managed("3080" "3080" "020105" "0000" "a080" "0101ff" "0000" "0000");
    EXPECT_EQ(asn1::parse(in).error().code(), errc::syntax);
    auto e = asn1::parse(in, asn1::ber);
    ASSERT_TRUE(e) << e.error().message();
    EXPECT_EQ(hex_of(e->bytes()), "300a" "3003020105" "a0030101ff");
    EXPECT_EQ(*(*e)[0][0].as_int(), 5);
    // nested deeply, every level indefinite
    std::string deep;
    for (int i = 0; i < 100; ++i) {
        deep += "3080";
    }
    deep += "0500";
    for (int i = 0; i < 100; ++i) {
        deep += "0000";
    }
    auto d = managed(deep);
    auto de = asn1::parse(d, asn1::ber);
    ASSERT_TRUE(de);
    auto x = *de;
    for (int i = 0; i < 100; ++i) {
        ASSERT_TRUE(x.is(asn1::type::sequence));
        x = x[0];
    }
    EXPECT_TRUE(x.is(asn1::type::null));
}

TEST(Asn1_Tests, BerConstructedStrings) {
    // OCTET STRING in pieces, a piece itself in pieces
    auto o = managed("2480" "04026162" "2480" "040163" "0000" "040164" "0000");
    auto e = asn1::parse(o, asn1::ber);
    ASSERT_TRUE(e) << e.error().message();
    EXPECT_EQ(hex_of(e->bytes()), "040461626364");
    EXPECT_FALSE(e->constructed());
    // definite pieces
    auto d2 = managed("240b" "04026162" "2405040163" "0400");
    auto e2 = asn1::parse(d2, asn1::ber);
    ASSERT_TRUE(e2) << e2.error().message();
    EXPECT_EQ(hex_of(e2->bytes()), "0403616263");
    // BIT STRING in pieces: the unused bits are the last piece's
    auto b = managed("2380" "0303 00 aabb" "0302 04 c0" "0000");
    auto be = asn1::parse(b, asn1::ber);
    ASSERT_TRUE(be) << be.error().message();
    EXPECT_EQ(hex_of(be->bytes()), "030404aabbc0");
    EXPECT_EQ(be->as_bits()->length, 20u);
    // unused bits in a piece before the last
    auto bad = managed("2380" "0302 04 c0" "0302 00 aa" "0000");
    EXPECT_EQ(asn1::parse(bad, asn1::ber).error().code(), errc::syntax);
    // a string type in pieces: the pieces are OCTET STRINGs, the whole
    // checked as the type once joined
    auto u = managed("2c80" "0402c5bc" "0402c3b3" "0000");
    auto ue = asn1::parse(u, asn1::ber);
    ASSERT_TRUE(ue) << ue.error().message();
    EXPECT_EQ(*ue->as_string(), "żó");
    auto split = managed("2c80" "0401c5" "0401bc" "0000");   // a character cut between pieces
    EXPECT_EQ(*asn1::parse(split, asn1::ber)->as_string(), "ż");
    auto invalid = managed("2c80" "0401c5" "0000");
    auto ie = asn1::parse(invalid, asn1::ber);
    ASSERT_FALSE(ie);
    EXPECT_EQ(ie.error().code(), errc::invalid_utf8);
    // a piece of another type
    auto wrong = managed("2480" "0c0161" "0000");
    EXPECT_EQ(asn1::parse(wrong, asn1::ber).error().code(), errc::syntax);
    // CMS's [0] IMPLICIT OCTET STRING in pieces stays constructed (its type
    // is the schema's), and as_bytes joins it
    auto cms = managed("a080" "04026162" "040163" "0000");
    auto ce = asn1::parse(cms, asn1::ber);
    ASSERT_TRUE(ce);
    EXPECT_TRUE(ce->constructed());
    EXPECT_EQ(hex_of(*ce->as_bytes()), "616263");
    EXPECT_EQ(*ce->as_string(), "abc");
    // in DER the same is two elements, not a string
    auto cd = managed("a007" "04026162" "040163");
    EXPECT_FALSE(asn1::parse(cd)->as_bytes());
}

TEST(Asn1_Tests, BerLengthsAndValues) {
    // AD's four-byte lengths: read where they lie, no copy
    auto in = managed("3084000000060201050101ff");
    EXPECT_EQ(asn1::parse(in).error().code(), errc::syntax);
    auto e = asn1::parse(in, asn1::ber);
    ASSERT_TRUE(e);
    EXPECT_EQ(e->bytes().data(), in.data());
    EXPECT_EQ(e->size(), 2u);
    EXPECT_EQ(*(*e)[0].as_int(), 5);
    EXPECT_EQ(*(*e)[1].as_bool(), true);
    // BOOLEAN of any byte, unused bits set
    auto b = managed("010101");
    EXPECT_FALSE(asn1::parse(b));
    EXPECT_EQ(*asn1::parse(b, asn1::ber)->as_bool(), true);
    auto bits = managed("03020781");
    EXPECT_FALSE(asn1::parse(bits));
    EXPECT_EQ(asn1::parse(bits, asn1::ber)->as_bits()->length, 1u);
    // the times of X.680 §46-47
    struct {
        const char* text;
        bool utc;
        int64_t seconds;
        uint32_t ns;
        int offset;
        bool der;
    } times[] = {
        {"2311142213Z", true, 1699999980, 0, 0, false},
        {"231114221320+0100", true, 1699996400, 0, 3600, false},
        {"231114221320-0130", true, 1700005400, 0, -5400, false},
        {"2023111422", false, 1699999200, 0, 0, false},              // local time, read as UTC
        {"2023111422.5Z", false, 1700001000, 0, 0, false},           // half an hour
        {"202311142213.25Z", false, 1699999995, 0, 0, false},        // a quarter of a minute
        {"20231114221320,5Z", false, 1700000000, 500000000, 0, false},
        {"20231114221320.123456789123Z", false, 1700000000, 123456789, 0, true},   // past nanoseconds: cut
        {"20231114221320+01", false, 1699996400, 0, 3600, false},
        {"20231114221320.1-0230", false, 1700009000, 100000000, -9000, false},
    };
    for (auto& t : times) {
        std::string text = t.text;
        std::string der = std::string(1, char(t.utc ? 0x17 : 0x18)) + char(text.size()) + text;
        sgcl::vector<byte> bytes(reinterpret_cast<const byte*>(der.data()), reinterpret_cast<const byte*>(der.data() + der.size()));
        EXPECT_EQ(bool(asn1::parse(bytes)), t.der) << t.text;
        auto r = asn1::parse(bytes, asn1::ber);
        ASSERT_TRUE(r) << t.text << ": " << r.error().message();
        auto when = r->as_time();
        ASSERT_TRUE(when) << t.text;
        EXPECT_EQ(when->unix(), t.seconds) << t.text;
        EXPECT_EQ(uint32_t(when->nanosecond()), t.ns) << t.text;
        EXPECT_EQ(when->zone() == sgcl::time::zone::utc(), t.offset == 0) << t.text;
        EXPECT_EQ(*r->as_string(), sgcl::string(text));
    }
    // what BER does not take either
    for (const char* bad : {"2311142213", "23111422Z", "2311142213+01", "2023111422.Z", "20231114226000Z", "20231114221320+2400", "2023111422135Z"}) {
        std::string text = bad;
        for (int tag : {0x17, 0x18}) {
            std::string der = std::string(1, char(tag)) + char(text.size()) + text;
            sgcl::vector<byte> bytes(reinterpret_cast<const byte*>(der.data()), reinterpret_cast<const byte*>(der.data() + der.size()));
            EXPECT_FALSE(asn1::parse(bytes, asn1::ber)) << bad << " " << tag;
        }
    }
}

TEST(Asn1_Tests, BerErrors) {
    struct {
        const char* hex;
        errc code;
    } cases[] = {
        {"0000", errc::syntax},                 // an end-of-contents at the top
        {"3080020105", errc::unexpected_end},   // no end
        {"30800000" "00", errc::syntax},         // a byte after
        {"0480", errc::syntax},                 // indefinite and primitive
        {"300400000000", errc::syntax},         // an end-of-contents in a definite element
        {"2280020105" "0000", errc::syntax},    // INTEGER constructed
        {"04890100000000000000000000", errc::out_of_range},   // a length of nine bytes
    };
    for (auto& c : cases) {
        auto bytes = managed(c.hex);
        auto r = asn1::parse(bytes, asn1::ber);
        ASSERT_FALSE(r) << c.hex;
        EXPECT_EQ(r.error().code(), c.code) << c.hex << ": " << r.error().message();
    }
}

// An element of indefinite length inside a definite one ends inside it: its
// elements and its end-of-contents never read past the holder's end (found
// by an LDAP fuzzer: the read ran past the input). From a buffer of exactly
// the input's size outside the managed heap, where ASan sees an overread
TEST(Asn1_Tests, IndefiniteInsideDefinite) {
    struct {
        std::vector<uint8_t> bytes;
        bool ok;
    } cases[] = {
        {{0x30, 0x08, 0x30, 0x02, 0x30, 0x80, 0x04, 0x00, 0x00, 0x00}, false},   // the EOC past the inner holder's end
        {{0x30, 0x04, 0x30, 0x80, 0x00, 0x00}, true},                           // ends exactly at the holder's end
        {{0x30, 0x06, 0x30, 0x80, 0x04, 0x00, 0x00, 0x00}, true},
        {{0x30, 0x05, 0x30, 0x80, 0x04, 0x00, 0x00}, false},                     // the EOC cut by the holder's end
        {{0x30, 0x04, 0x30, 0x80, 0x04, 0x02, 0x00, 0x00}, false},               // an element past the holder's end
        {{0x30, 0x80, 0x30, 0x03, 0x30, 0x80, 0x00, 0x00, 0x00}, false},
        {{0x30, 0x80, 0x30, 0x04, 0x30, 0x80, 0x00, 0x00, 0x00, 0x00}, true},
    };
    for (auto& c : cases) {
        auto* exact = static_cast<std::byte*>(std::malloc(c.bytes.size()));
        std::copy(c.bytes.begin(), c.bytes.end(), reinterpret_cast<uint8_t*>(exact));
        auto r = asn1::parse(slice<const std::byte>(exact, c.bytes.size()), asn1::ber);
        EXPECT_EQ(bool(r), c.ok) << (r ? std::string() : std::string(r.error().message().view()));
        if (!r) {
            EXPECT_LE(r.error().offset(), c.bytes.size());
        }
        std::free(exact);
    }
}

// --- the limits ---

TEST(Asn1_Tests, DepthLimit) {
    for (bool ber : {false, true}) {
        std::string hex;
        size_t depth = 20;
        // twenty SEQUENCEs one inside another around a NULL
        std::vector<std::string> levels;
        std::string inner = "0500";
        for (size_t i = 0; i < depth; ++i) {
            char head[8];
            std::snprintf(head, sizeof head, "30%02x", unsigned(inner.size() / 2));
            inner = head + inner;
        }
        auto bytes = managed(inner);
        asn1::options o;
        o.ber = ber;
        o.max_depth = 20;
        EXPECT_TRUE(asn1::parse(bytes, o));
        o.max_depth = 19;
        auto r = asn1::parse(bytes, o);
        ASSERT_FALSE(r);
        EXPECT_EQ(r.error().code(), errc::depth_limit);
        o.max_depth = 0;
        EXPECT_EQ(asn1::parse(bytes, o).error().code(), errc::depth_limit);
        auto null = managed("0500");
        EXPECT_TRUE(asn1::parse(null, o));   // a primitive needs no depth
    }
    // the default bound against a deep input, without recursion
    std::string deep;
    for (int i = 0; i < 100000; ++i) {
        deep += "3080";
    }
    auto d = managed(deep);
    auto r = asn1::parse(d, asn1::ber);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::depth_limit);
    // a structure made deeper than any bound is dumped without recursion
    asn1 e = asn1::null();
    size_t levels = 3000;
    for (size_t i = 0; i < levels; ++i) {
        e = asn1::sequence({e});
    }
    // a line a level, "SEQUENCE" indented by two a level, and the NULL
    EXPECT_EQ(e.to_string().size(), levels * 9 + levels * (levels - 1) + 2 * levels + 5);
}

TEST(Asn1_Tests, CutEverywhere) {
    // every prefix of a structure is refused, and where
    auto s = asn1::sequence({asn1::integer(300), asn1::utf8_string("abc"), asn1::explicit_tag(1, asn1::octet_string(sgcl::vector<byte>(200, byte(7))))});
    auto whole = s.bytes();
    for (size_t n = 0; n < whole.size(); ++n) {
        auto r = asn1::parse(whole.first(n));
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), errc::unexpected_end) << n;
        EXPECT_LE(r.error().offset(), n);
    }
    EXPECT_TRUE(asn1::parse(whole));
}

// --- the members at their boundaries ---

TEST(Asn1_Tests, DefaultElement) {
    asn1 none;
    EXPECT_FALSE(none);
    EXPECT_EQ(none, asn1());
    EXPECT_FALSE(none.constructed());
    EXPECT_EQ(none.size(), 0u);
    EXPECT_TRUE(none.empty());
    EXPECT_FALSE(none[0]);
    EXPECT_EQ(none.begin(), none.end());
    EXPECT_TRUE(none.bytes().empty());
    EXPECT_TRUE(none.content().empty());
    EXPECT_FALSE(none.as_bool());
    EXPECT_FALSE(none.as_int());
    EXPECT_FALSE(none.as_big_integer());
    EXPECT_FALSE(none.as_bytes());
    EXPECT_FALSE(none.as_bits());
    EXPECT_FALSE(none.as_oid());
    EXPECT_FALSE(none.as_string());
    EXPECT_FALSE(none.as_time());
    EXPECT_FALSE(none.is(asn1::type::null));
    EXPECT_FALSE(none.is_context(0));
    EXPECT_EQ(none.to_string(), "");
    EXPECT_EQ(none.hash(), asn1().hash());
    EXPECT_FALSE(asn1::explicit_tag(0, none));
    EXPECT_FALSE(asn1::implicit_tag(0, none));
    EXPECT_EQ(hex_of(asn1::sequence({none, asn1::null(), none}).bytes()), "30020500");
    EXPECT_EQ(hex_of(asn1::sequence({}).bytes()), "3000");
    EXPECT_EQ(hex_of(asn1::set(sgcl::vector<asn1>()).bytes()), "3100");
    // moved from: a view, so a copy
    asn1 a = asn1::integer(1);
    asn1 b = std::move(a);
    EXPECT_EQ(*b.as_int(), 1);
}

TEST(Asn1_Tests, ChildrenAndIteration) {
    auto s = asn1::sequence({asn1::integer(1), asn1::sequence({}), asn1::null(), asn1::integer(4)});
    EXPECT_EQ(s.size(), 4u);
    EXPECT_FALSE(s.empty());
    EXPECT_TRUE(s[1].empty());
    EXPECT_TRUE(s[1].constructed());
    EXPECT_FALSE(s[4]);
    EXPECT_FALSE(s[size_t(-1)]);
    size_t n = 0;
    for (auto e : s) {
        EXPECT_EQ(e, s[n]);
        ++n;
    }
    EXPECT_EQ(n, 4u);
    auto it = s.begin();
    auto old = it++;
    EXPECT_EQ(*old, s[0]);
    EXPECT_EQ(*it, s[1]);
    EXPECT_EQ(std::distance(s.begin(), s.end()), 4);
    static_assert(std::forward_iterator<asn1::iterator>);
    // a primitive has no elements
    auto i = asn1::integer(7);
    EXPECT_EQ(i.size(), 0u);
    EXPECT_TRUE(i.empty());
    EXPECT_FALSE(i[0]);
    EXPECT_EQ(i.begin(), i.end());
    // the view keeps its bytes: the element outlives what made it
    asn1 kept;
    {
        auto t = asn1::sequence({asn1::utf8_string("kept")});
        kept = t[0];
    }
    sgcl::collector::force_collect(true);
    EXPECT_EQ(*kept.as_string(), "kept");
    EXPECT_EQ(hex_of(kept.bytes()), "0c046b657074");
    EXPECT_EQ(hex_of(kept.content()), "6b657074");
}

TEST(Asn1_Tests, ValuesOfTheWrongType) {
    auto i = asn1::integer(1);
    EXPECT_FALSE(i.as_bool());
    EXPECT_FALSE(i.as_bytes());
    EXPECT_FALSE(i.as_bits());
    EXPECT_FALSE(i.as_oid());
    EXPECT_FALSE(i.as_string());
    EXPECT_FALSE(i.as_time());
    auto s = asn1::utf8_string("1");
    EXPECT_FALSE(s.as_int());
    EXPECT_FALSE(s.as_big_integer());
    EXPECT_FALSE(asn1::octet_string(sgcl::vector<byte>(1)).as_string());
    EXPECT_FALSE(asn1::sequence({}).as_int());
    EXPECT_FALSE(asn1::sequence({}).as_bytes());
    // implicitly tagged: read as the type asked, its rules held
    auto t = asn1::implicit_tag(2, asn1::boolean(true));
    EXPECT_EQ(*t.as_bool(), true);
    EXPECT_EQ(*t.as_int(), -1);   // FF as an INTEGER
    EXPECT_FALSE(asn1::implicit_tag(2, asn1::integer(256)).as_bool());
    EXPECT_FALSE(asn1::implicit_tag(2, asn1::octet_string(sgcl::vector<byte>(2))).as_int());   // 00 00
    EXPECT_FALSE(asn1::implicit_tag(2, asn1::octet_string(sgcl::vector<byte>(1, byte(0x80)))).as_oid());
    auto when = asn1::implicit_tag(4, asn1::generalized_time(sgcl::time::datetime::from_unix(5, sgcl::time::zone::utc())));
    EXPECT_EQ(when.as_time()->unix(), 5);
    auto when2 = asn1::implicit_tag(4, asn1::utc_time(sgcl::time::datetime::from_unix(5, sgcl::time::zone::utc())));
    EXPECT_EQ(when2.as_time()->unix(), 5);
    // a big INTEGER is no int64_t
    EXPECT_FALSE(asn1::integer(sgcl::math::big_integer(1) << 63).as_int());
    EXPECT_EQ(*asn1::integer(sgcl::math::big_integer(-1) << 63).as_int(), INT64_MIN);
}

TEST(Asn1_Tests, StringsTheirSets) {
    EXPECT_THROW(asn1::utf8_string(sgcl::string("\xff")), std::invalid_argument);
    EXPECT_THROW(asn1::printable_string("a@b"), std::invalid_argument);
    EXPECT_THROW(asn1::printable_string("a*b"), std::invalid_argument);
    EXPECT_THROW(asn1::ia5_string("ż"), std::invalid_argument);
    EXPECT_THROW(asn1::numeric_string("12a"), std::invalid_argument);
    EXPECT_THROW(asn1::visible_string("a\tb"), std::invalid_argument);
    EXPECT_THROW(asn1::bmp_string("😀"), std::invalid_argument);
    EXPECT_THROW(asn1::bmp_string(sgcl::string("\xc5")), std::invalid_argument);
    EXPECT_EQ(hex_of(asn1::visible_string("a b~").bytes()), "1a046120627e");
    EXPECT_EQ(hex_of(asn1::bmp_string("aż").bytes()), "1e040061017c");
    EXPECT_EQ(*asn1::parse(asn1::bmp_string("aż").bytes())->as_string(), "aż");
    EXPECT_EQ(hex_of(asn1::printable_string("").bytes()), "1300");
    // T61String, GeneralString and the rest as ISO 8859-1; UniversalString as UCS-4
    auto t61 = managed("1403e9c0ff");
    EXPECT_EQ(*asn1::parse(t61)->as_string(), "éÀÿ");
    auto ucs4 = managed("1c080001f600000000e9");
    EXPECT_EQ(*asn1::parse(ucs4)->as_string(), "😀é");
    auto ucs4bad = managed("1c040000d800");
    EXPECT_EQ(asn1::parse(ucs4bad).error().code(), errc::invalid_character);
    // the errors and their places
    auto bad = managed("3007" "0c05616263ff64");
    auto r = asn1::parse(bad);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::invalid_utf8);
    EXPECT_EQ(r.error().offset(), 7u);
    EXPECT_EQ(r.error().message(), "offset 7: invalid UTF-8 in a UTF8String");
    auto at = managed("1303616240");
    EXPECT_EQ(asn1::parse(at).error().offset(), 4u);
}

TEST(Asn1_Tests, TimesWritten) {
    using sgcl::time::datetime;
    using sgcl::time::zone;
    auto t = datetime::from_unix_nano(1700000000123450000, zone::fixed(sgcl::duration(std::chrono::hours(2))));
    EXPECT_EQ(std::string(asn1::generalized_time(t).as_string()->view()), "20231114221320.12345Z");
    EXPECT_EQ(std::string(asn1::utc_time(t).as_string()->view()), "231114221320Z");
    EXPECT_THROW(asn1::utc_time(datetime::from_unix(-631152001, zone::utc())), std::invalid_argument);
    EXPECT_THROW(asn1::utc_time(datetime::from_unix(2524608000, zone::utc())), std::invalid_argument);
    EXPECT_NO_THROW(asn1::utc_time(datetime::from_unix(2524607999, zone::utc())));
    // the ends of a datetime, and back
    for (int64_t s : {-9223372036LL, 9223372035LL, 0LL}) {
        auto g = asn1::generalized_time(datetime::from_unix(s, zone::utc()));
        auto back = asn1::parse(g.bytes());
        ASSERT_TRUE(back);
        EXPECT_EQ(back->as_time()->unix(), s);
    }
}

TEST(Asn1_Tests, TagsAndRaw) {
    EXPECT_EQ(hex_of(asn1::explicit_tag(30, asn1::null()).bytes()), "be020500");
    EXPECT_EQ(hex_of(asn1::explicit_tag(31, asn1::null()).bytes()), "bf1f020500");
    EXPECT_EQ(hex_of(asn1::implicit_tag(127, asn1::null()).bytes()), "9f7f00");
    EXPECT_EQ(hex_of(asn1::implicit_tag(128, asn1::null()).bytes()), "9f810000");
    EXPECT_EQ(hex_of(asn1::implicit_tag((1u << 28) - 1, asn1::null(), asn1::tag_class::private_use).bytes()), "dfffffff7f00");
    EXPECT_THROW(asn1::implicit_tag(1u << 28, asn1::null()), std::invalid_argument);
    EXPECT_THROW(asn1::explicit_tag(1u << 28, asn1::null()), std::invalid_argument);
    auto big = asn1::implicit_tag((1u << 28) - 1, asn1::null());
    auto back = asn1::parse(big.bytes());
    ASSERT_TRUE(back);
    EXPECT_EQ(back->tag(), (1u << 28) - 1);
    // implicit of a constructed keeps it constructed
    auto c = asn1::implicit_tag(0, asn1::sequence({asn1::null()}));
    EXPECT_EQ(hex_of(c.bytes()), "a0020500");
    EXPECT_EQ(c[0], asn1::null());
    // implicit into the universal class is held to the type
    EXPECT_EQ(hex_of(asn1::implicit_tag(2, asn1::octet_string(sgcl::vector<byte>(1)), asn1::tag_class::universal).bytes()), "020100");
    EXPECT_THROW(asn1::implicit_tag(1, asn1::octet_string(sgcl::vector<byte>(2)), asn1::tag_class::universal), std::invalid_argument);
    // raw
    std::vector<byte> two{byte(0), byte(5)};
    EXPECT_THROW(asn1::raw(asn1::tag_class::universal, 2, false, as_slice(two)), std::invalid_argument);
    EXPECT_THROW(asn1::raw(asn1::tag_class::context_specific, 0, true, as_slice(two)), std::invalid_argument);
    EXPECT_THROW(asn1::raw(asn1::tag_class::universal, 0, false, as_slice(two)), std::invalid_argument);
    auto r = asn1::raw(asn1::tag_class::application, 7, false, as_slice(two));
    EXPECT_EQ(hex_of(r.bytes()), "47020005");
    EXPECT_FALSE(r.as_int());   // read as the caller asks, held to INTEGER's rules: 00 05 is not
    EXPECT_EQ(hex_of(r.content()), "0005");
}

TEST(Asn1_Tests, SetsInDerOrder) {
    // X.690 §11.6: by the encodings, the shorter padded with zeros
    auto a = asn1::octet_string(sgcl::vector<byte>{byte(1)});
    auto b = asn1::octet_string(sgcl::vector<byte>{byte(1), byte(0)});
    auto c = asn1::integer(1);
    auto s = asn1::set({b, a, c, a});
    EXPECT_EQ(hex_of(s.bytes()), "310d" "020101" "040101" "040101" "04020100");
    // the same element twice, and the set itself inside
    auto t = asn1::set({s, s});
    EXPECT_EQ(t.size(), 2u);
    EXPECT_EQ(t[0], s);
    EXPECT_EQ(t[1], s);
    // a set made of a vector
    sgcl::vector<asn1> v;
    for (int i = 10; i > 0; --i) {
        v.push_back(asn1::integer(i));
    }
    auto sv = asn1::set(v);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(*sv[size_t(i)].as_int(), i + 1);
    }
    auto qv = asn1::sequence(v);
    EXPECT_EQ(*qv[0].as_int(), 10);
}

TEST(Asn1_Tests, EqualityAndHash) {
    auto a = asn1::sequence({asn1::integer(1)});
    auto b = asn1::parse(a.bytes());
    ASSERT_TRUE(b);
    EXPECT_EQ(a, *b);
    EXPECT_EQ(a.hash(), b->hash());
    EXPECT_EQ(std::hash<asn1>()(a), a.hash());
    EXPECT_NE(a, asn1::sequence({asn1::integer(2)}));
    EXPECT_NE(a, a[0]);
    sgcl::set<asn1::oid> ids;   // std::hash of an oid
    ids.insert(asn1::oid("1.2.3"));
    ids.insert(asn1::oid("1.2.3"));
    EXPECT_EQ(ids.size(), 1u);
}

TEST(Asn1_Tests, Dump) {
    auto s = asn1::sequence({
        asn1::integer(-5), asn1::boolean(false), asn1::null(), asn1::object_identifier(asn1::oid("2.5.4.3")),
        asn1::utf8_string("a\"b\\c\n"), asn1::octet_string(sgcl::vector<byte>(40, byte(0xab))),
        asn1::bit_string(sgcl::vector<byte>{byte(0xf0)}, 4), asn1::explicit_tag(0, asn1::integer(1)),
        asn1::implicit_tag(1, asn1::integer(1)), asn1::implicit_tag(3, asn1::null(), asn1::tag_class::application),
        asn1::implicit_tag(9, asn1::sequence({}), asn1::tag_class::private_use),
        asn1::raw(asn1::tag_class::universal, 9, false, sgcl::vector<byte>{byte(0x40)}),
        asn1::utc_time(sgcl::time::datetime::from_unix(0, sgcl::time::zone::utc()))});
    EXPECT_EQ(std::string(s.to_string().view()),
              "SEQUENCE\n"
              "  INTEGER -5\n"
              "  BOOLEAN false\n"
              "  NULL\n"
              "  OBJECT IDENTIFIER 2.5.4.3\n"
              "  UTF8String \"a\\\"b\\\\c\\x0a\"\n"
              "  OCTET STRING (40 bytes) abababababababababababababababababababababababababababababababab...\n"
              "  BIT STRING (4 bits) f0\n"
              "  [0]\n"
              "    INTEGER 1\n"
              "  [1] (1 byte) 01\n"
              "  [APPLICATION 3] (0 bytes)\n"
              "  [PRIVATE 9]\n"
              "  REAL (1 byte) 40\n"
              "  UTCTime 700101000000Z\n");
}

// --- streams ---

TEST(Asn1_Tests, ReadFromAStream) {
    auto a = asn1::sequence({asn1::integer(1), asn1::octet_string(sgcl::vector<byte>(300, byte(1)))});
    auto b = asn1::implicit_tag(1u << 20, asn1::null(), asn1::tag_class::application);
    std::string text = std::string(reinterpret_cast<const char*>(a.bytes().data()), a.bytes().size())
                     + std::string(reinterpret_cast<const char*>(b.bytes().data()), b.bytes().size())
                     + std::string("\x30\x80\x02\x01\x07\x24\x80\x04\x01\x61\x00\x00\x00\x00", 14);
    for (size_t piece : {1, 2, 3, 7, 4096}) {
        sgcl::io::reader in(make_tracked<dribble>(text, piece));
        auto x = asn1::parse(in);
        ASSERT_TRUE(x) << x.error().message();
        EXPECT_EQ(*x, a);
        auto y = asn1::parse(in);
        ASSERT_TRUE(y);
        EXPECT_EQ(*y, b);
        EXPECT_FALSE(asn1::parse(in));   // BER, read as DER
    }
    for (size_t piece : {1, 5}) {
        sgcl::io::reader in(make_tracked<dribble>(text, piece));
        ASSERT_TRUE(asn1::parse(in, asn1::ber));
        ASSERT_TRUE(asn1::parse(in, asn1::ber));
        auto z = asn1::parse(in, asn1::ber);
        ASSERT_TRUE(z) << z.error().message();
        EXPECT_EQ(hex_of(z->bytes()), "3006020107040161");
        auto end = asn1::parse(in, asn1::ber);
        ASSERT_FALSE(end);
        EXPECT_EQ(end.error().code(), errc::unexpected_end);
        EXPECT_EQ(end.error().offset(), 0u);
    }
}

TEST(Asn1_Tests, ReadFromAStreamFails) {
    auto a = asn1::sequence({asn1::octet_string(sgcl::vector<byte>(300, byte(1)))});
    std::string text(reinterpret_cast<const char*>(a.bytes().data()), a.bytes().size());
    // cut at every byte: the end inside the element
    for (size_t n = 1; n < text.size(); n += 37) {
        sgcl::io::reader in(make_tracked<dribble>(text.substr(0, n), 3));
        auto r = asn1::parse(in);
        ASSERT_FALSE(r) << n;
        EXPECT_EQ(r.error().code(), errc::unexpected_end) << n;
    }
    // a stream that fails
    sgcl::io::reader bad(make_tracked<failing>(text.substr(0, 10)));
    auto r = asn1::parse(bad);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), errc::io);
    EXPECT_TRUE(r.error().io_error());
    // a length past max_size, refused before it is read
    asn1::options o;
    o.max_size = 100;
    sgcl::io::reader big(make_tracked<dribble>(text, 4096));
    auto l = asn1::parse(big, o);
    ASSERT_FALSE(l);
    EXPECT_EQ(l.error().code(), errc::limit_exceeded);
    std::string huge("\x04\x88\x7f\xff\xff\xff\xff\xff\xff\xff", 10);
    sgcl::io::reader h(make_tracked<dribble>(huge, 4096));
    EXPECT_EQ(asn1::parse(h).error().code(), errc::limit_exceeded);
    // indefinite lengths past max_depth
    std::string deep;
    for (int i = 0; i < 1000; ++i) {
        deep += "\x30\x80";
    }
    sgcl::io::reader d(make_tracked<dribble>(deep, 4096));
    EXPECT_EQ(asn1::parse(d, asn1::ber).error().code(), errc::depth_limit);
    // a tag past 2^28
    sgcl::io::reader t(make_tracked<dribble>(std::string("\x1f\xff\xff\xff\xff\x7f\x00", 7), 1));
    EXPECT_EQ(asn1::parse(t).error().code(), errc::out_of_range);
}

TEST(Asn1_Tests, AsyncRead) {
    auto a = asn1::sequence({asn1::integer(1), asn1::utf8_string("x")});
    std::string text(reinterpret_cast<const char*>(a.bytes().data()), a.bytes().size());
    text += std::string("\x30\x80\x05\x00\x00\x00", 6);
    auto t = sgcl::async::spawn([](std::string text, asn1 want) -> sgcl::async::task<int> {
        sgcl::io::reader in(make_tracked<dribble>(text, 2));
        auto x = co_await asn1::async_parse(in);
        if (!x || *x != want) {
            co_return -1;
        }
        auto y = co_await asn1::async_parse(in, asn1::ber);
        if (!y || hex_of(y->bytes()) != "30020500") {
            co_return -2;
        }
        auto z = co_await asn1::async_parse(in);
        if (z || z.error().code() != errc::unexpected_end) {
            co_return -3;
        }
        co_return 1;
    }(text, a));
    EXPECT_EQ(t.wait(), 1);
    sgcl::async::scheduler::stop();
}

// --- oid ---

TEST(Asn1_Tests, OidText) {
    constexpr asn1::oid rsa("1.2.840.113549.1.1.1");
    static_assert(rsa.max_size == 63);
    EXPECT_EQ(rsa.to_string(), "1.2.840.113549.1.1.1");
    EXPECT_EQ(asn1::oid("1.2.840.113549.1.1.1"), rsa);
    EXPECT_EQ(asn1::oid({1, 2, 840, 113549, 1, 1, 1}), rsa);
    EXPECT_EQ(rsa.size(), 7u);
    EXPECT_TRUE(rsa.starts_with(asn1::oid("1.2.840")));
    EXPECT_TRUE(rsa.starts_with(rsa));
    EXPECT_TRUE(rsa.starts_with(asn1::oid()));
    EXPECT_FALSE(rsa.starts_with(asn1::oid("1.2.84")));
    EXPECT_FALSE(asn1::oid("1.2").starts_with(rsa));
    EXPECT_LT(asn1::oid("1.2.3"), asn1::oid("1.2.3.4"));
    EXPECT_LT(asn1::oid("1.2.3"), asn1::oid("1.2.128"));
    EXPECT_LT(asn1::oid("1.39"), asn1::oid("2.0"));
    EXPECT_LT(asn1::oid("2.40"), asn1::oid("2.1000"));
    EXPECT_EQ(sgcl::txt::format("{}", rsa), "1.2.840.113549.1.1.1");
    EXPECT_EQ(sgcl::txt::format("[{:>8}]", asn1::oid("2.5")), "[     2.5]");
    // arcs of any size
    sgcl::string uuid = "2.25.329800735698586629295641978511506172918";
    auto u = asn1::oid::parse(uuid);
    ASSERT_TRUE(u);
    EXPECT_EQ(u->to_string(), uuid);
    EXPECT_EQ(u->size(), 3u);
    EXPECT_EQ(u->arc(1), 25u);
    EXPECT_FALSE(u->arc(2));
    auto big2 = asn1::oid::parse("2.329800735698586629295641978511506172918");
    ASSERT_TRUE(big2);
    EXPECT_EQ(big2->to_string(), "2.329800735698586629295641978511506172918");
    EXPECT_EQ(big2->arc(0), 2u);
    EXPECT_FALSE(big2->arc(1));
    EXPECT_EQ(asn1::oid({2, 18446744073709551615ull}).to_string(), "2.18446744073709551615");
    EXPECT_EQ(asn1::oid({2, 18446744073709551615ull}).arc(1), 18446744073709551615ull);
    // what is not one
    for (const char* bad : {"", "1", "3.1", "1.40", "0.40", "1.2.", ".1.2", "1..2", "1.02", "01.2", "1.2a", "1.-2", " 1.2"}) {
        auto r = asn1::oid::parse(bad);
        ASSERT_FALSE(r) << bad;
        EXPECT_EQ(r.error().code(), errc::syntax) << bad;
    }
    EXPECT_TRUE(asn1::oid::parse("2.40"));
    EXPECT_TRUE(asn1::oid::parse("2.0"));
    EXPECT_TRUE(asn1::oid::parse("1.39"));
    EXPECT_THROW(asn1::oid(sgcl::string("1")), sgcl::bad_expected_access<error>);
    EXPECT_THROW(asn1::oid({1}), std::invalid_argument);
    EXPECT_THROW(asn1::oid({3, 1}), std::invalid_argument);
    EXPECT_THROW(asn1::oid({1, 40}), std::invalid_argument);
    EXPECT_NO_THROW(asn1::oid({2, 40}));
    // past 63 bytes
    std::string longest = "1.2";
    while (asn1::oid::parse(sgcl::string(longest + ".1"))) {
        longest += ".1";
    }
    EXPECT_EQ(asn1::oid::parse(sgcl::string(longest)).value().size(), 64u);   // 1.2 and 62 more
    EXPECT_EQ(asn1::oid::parse(sgcl::string(longest + ".1")).error().code(), errc::limit_exceeded);
    std::initializer_list<uint64_t> list = {1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    EXPECT_THROW(asn1::oid{list}, std::invalid_argument);
    // a long OID of the input: an element, no oid
    std::string content(70, '\x01');
    auto e = asn1::raw(asn1::tag_class::universal, 6, false, sgcl::slice<const byte>(reinterpret_cast<const byte*>(content.data()), content.size()));
    EXPECT_FALSE(e.as_oid());
    EXPECT_EQ(e.content().size(), 70u);
    // the default
    asn1::oid none;
    EXPECT_FALSE(none);
    EXPECT_EQ(none.size(), 0u);
    EXPECT_EQ(none.to_string(), "");
    EXPECT_FALSE(none.arc(0));
    EXPECT_THROW(asn1::object_identifier(none), std::invalid_argument);
}
