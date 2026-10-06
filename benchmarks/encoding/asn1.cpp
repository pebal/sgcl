//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// ASN.1 of the encoding module against Go's encoding/asn1 (benchmarks/go/asn1,
// the same bytes, the same work).
//   asn1 sgcl [op=parse] [count]
//
//   parse    asn1::parse of a certificate's structure and every field read
//            into a program's struct: the version, the serial as a
//            big_integer, the OIDs, the names' strings, the times, the key's
//            bits, the extensions (Go: Unmarshal into tagged structs)
//   marshal  the same structure made by sequence, integer and the rest, its
//            bytes (Go: Marshal of the structs)
//   walk     asn1::parse and every element visited, no schema (Go: Unmarshal
//            of every element into a RawValue, recursively)
//
// The structure: RFC 5280's shape, an ECDSA key and signature, three names of
// three attributes, four extensions, 508 bytes; the SHA-256 of the bytes is
// printed on both sides. A run takes about two seconds by default. Prints
// nanoseconds per certificate and megabytes of it per second.
#include "benchmarks/common.h"
#include "sgcl/crypto/sha256.h"
#include "sgcl/encoding/asn1.h"

#include <cstdlib>
#include <cstring>
#include <string>

using namespace sgcl;
using encoding::asn1;

namespace {
    vector<byte> filled(size_t n, uint8_t b) {
        vector<byte> out;
        for (size_t i = 0; i < n; ++i) {
            out.push_back(byte(uint8_t(b + i)));
        }
        return out;
    }

    asn1 name(const char* org) {
        auto rdn = [](const asn1::oid& type, const char* value) {
            return asn1::set({asn1::sequence({asn1::object_identifier(type), asn1::utf8_string(value)})});
        };
        return asn1::sequence({rdn(asn1::oid("2.5.4.6"), "PL"), rdn(asn1::oid("2.5.4.10"), org), rdn(asn1::oid("2.5.4.3"), "www.example.com")});
    }

    asn1 extension(const asn1::oid& id, bool critical, const vector<byte>& value) {
        return asn1::sequence({asn1::object_identifier(id), critical ? asn1::boolean(true) : asn1(), asn1::octet_string(value)});
    }

    asn1 certificate() {
        auto utc = [](int64_t s) { return asn1::utc_time(time::datetime::from_unix(s, time::zone::utc())); };
        asn1 tbs = asn1::sequence({
            asn1::explicit_tag(0, asn1::integer(2)),
            asn1::integer(math::big_integer("123456789012345678901234567890123456789")),
            asn1::sequence({asn1::object_identifier(asn1::oid("1.2.840.10045.4.3.2"))}),
            name("Example Issuing CA"),
            asn1::sequence({utc(1700000000), utc(1731536000)}),
            name("Example Subject Ltd"),
            asn1::sequence({asn1::sequence({asn1::object_identifier(asn1::oid("1.2.840.10045.2.1")), asn1::object_identifier(asn1::oid("1.2.840.10045.3.1.7"))}),
                            asn1::bit_string(filled(65, 4))}),
            asn1::explicit_tag(3, asn1::sequence({extension(asn1::oid("2.5.29.15"), true, filled(4, 3)), extension(asn1::oid("2.5.29.19"), true, filled(2, 0x30)),
                                                  extension(asn1::oid("2.5.29.17"), false, filled(40, 0x82)), extension(asn1::oid("2.5.29.14"), false, filled(22, 4))})),
        });
        return asn1::sequence({tbs, asn1::sequence({asn1::object_identifier(asn1::oid("1.2.840.10045.4.3.2"))}), asn1::bit_string(filled(72, 0x30))});
    }

    // What Go's Unmarshal fills: every field of the certificate
    struct attribute {
        asn1::oid type;
        string value;
    };

    struct extension_value {
        asn1::oid id;
        bool critical = false;
        slice<const byte> value;
    };

    struct parsed {
        int64_t version = 0;
        math::big_integer serial;
        asn1::oid signature;
        vector<attribute> issuer;
        time::datetime not_before;
        time::datetime not_after;
        vector<attribute> subject;
        asn1::oid key_algorithm;
        asn1::oid curve;
        asn1::bits key;
        vector<extension_value> extensions;
        asn1::oid algorithm;
        asn1::bits signature_bits;
    };

    void names(const asn1& seq, vector<attribute>& out) {
        for (auto rdn : seq) {
            for (auto atv : rdn) {
                out.push_back({*atv[0].as_oid(), *atv[1].as_string()});
            }
        }
    }

    size_t read(const vector<byte>& der) {
        auto c = asn1::parse(der);
        if (!c) {
            std::fprintf(stderr, "asn1: %s\n", std::string(c.error().message().view()).c_str());
            std::exit(1);
        }
        parsed p;
        asn1 tbs = (*c)[0];
        auto it = tbs.begin();
        p.version = *(*it)[0].as_int();
        p.serial = *(*++it).as_big_integer();
        p.signature = *(*++it)[0].as_oid();
        names(*++it, p.issuer);
        asn1 validity = *++it;
        p.not_before = *validity[0].as_time();
        p.not_after = *validity[1].as_time();
        names(*++it, p.subject);
        asn1 spki = *++it;
        p.key_algorithm = *spki[0][0].as_oid();
        p.curve = *spki[0][1].as_oid();
        p.key = *spki[1].as_bits();
        for (auto ext : (*++it)[0]) {
            extension_value e;
            auto f = ext.begin();
            e.id = *(*f).as_oid();
            ++f;
            if ((*f).is(asn1::type::boolean)) {
                e.critical = *(*f).as_bool();
                ++f;
            }
            e.value = *(*f).as_bytes();
            p.extensions.push_back(e);
        }
        p.algorithm = *(*c)[1][0].as_oid();
        p.signature_bits = *(*c)[2].as_bits();
        return p.extensions.size() + p.issuer.size();
    }

    size_t walk(const asn1& e) {
        size_t n = 1;
        for (auto c : e) {
            n += walk(c);
        }
        return n;
    }

    volatile size_t sink = 0;

    template<class F>
    double timed(long count, F&& f) {
        for (long i = 0; i < std::min(count, 1000L); ++i) {
            f();
        }
        auto t0 = bench::Clock::now();
        for (long i = 0; i < count; ++i) {
            f();
        }
        return bench::seconds_since(t0) / double(count) * 1e9;
    }
}

int main(int argc, char** argv) {
    const char* variant = argc > 1 ? argv[1] : "sgcl";
    const char* op = argc > 2 ? argv[2] : "parse";
    if (std::strcmp(variant, "sgcl") != 0) {
        std::fprintf(stderr, "usage: asn1 sgcl [parse|marshal|walk] [count]\n");
        return 2;
    }
    asn1 cert = certificate();
    vector<byte> der(cert.bytes().begin(), cert.bytes().end());
    long count = argc > 3 ? std::atol(argv[3]) : long(2000000000 / der.size() / 20);
    double ns;
    if (!std::strcmp(op, "parse")) {
        ns = timed(count, [&] { sink += read(der); });
    } else if (!std::strcmp(op, "marshal")) {
        ns = timed(count, [&] { sink += certificate().bytes().size(); });
    } else if (!std::strcmp(op, "walk")) {
        ns = timed(count, [&] { sink += walk(asn1::parse(der).value()); });
    } else {
        std::fprintf(stderr, "asn1: no op called %s\n", op);
        return 2;
    }
    auto digest = crypto::sha256::of(der);
    char hex[17];
    for (int i = 0; i < 8; ++i) {
        std::snprintf(hex + 2 * i, 3, "%02x", unsigned(digest[size_t(i)]));
    }
    std::printf("%s op=%s bytes=%zu sha256=%s count=%ld ns/op=%.0f MB/s=%.0f\n", variant, op, der.size(), hex, count, ns,
                double(der.size()) / ns * 1e3);
}
