//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The SvcParams of SVCB and HTTPS (RFC 9460; sgcl/net/detail/dns_message.h
// and dns::svcb's typing in sgcl/net/dns.h) on any bytes. The input's
// first byte picks the priority (0: AliasMode) and the type; the rest is
// the SvcParams. What must hold:
//   - nothing is read past the input (ASan): every reader is given a
//     buffer of malloc of exactly its bytes (the SvcParams alone, the whole
//     message), never a managed copy, whose overreads ASan does not see;
//   - dns_svc_params_ok agrees with a reference written here another way
//     (the parameters split into a list first, then each rule of §2.2, §7
//     and §8 checked on the list);
//   - a well-formed list walked by dns_svc_params_each gives the list's
//     keys, lengths and values, and written again gives the same bytes;
//   - the input as the rdata of an answer (target "." and a name, the
//     priority, the params): dns_read_answer is ok exactly when the record
//     is well formed (AliasMode: whatever its params), and the typed record
//     says what the list says — every alpn id, hint, port, ech, dohpath,
//     mandatory key, and the other keys raw in order.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/dns_svcb_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/dns.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
    namespace nd = sgcl::net::detail;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    struct Param {
        unsigned key;
        std::string value;
    };

    unsigned u16(const std::string& s, size_t at) {
        return unsigned(uint8_t(s[at])) << 8 | uint8_t(s[at + 1]);
    }

    // The reference: split, then judge
    bool reference(const std::string& p, std::vector<Param>& out) {
        size_t i = 0;
        while (i < p.size()) {
            if (p.size() - i < 4) {
                return false;
            }
            unsigned k = u16(p, i), n = u16(p, i + 2);
            if (p.size() - i - 4 < n) {
                return false;
            }
            out.push_back({k, p.substr(i + 4, n)});
            i += 4 + n;
        }
        std::set<unsigned> keys;
        for (size_t j = 0; j < out.size(); ++j) {
            if (j > 0 && out[j].key <= out[j - 1].key) {
                return false;
            }
            keys.insert(out[j].key);
        }
        for (auto& [k, v] : out) {
            switch (k) {
                case 0: {
                    if (v.empty() || v.size() % 2) {
                        return false;
                    }
                    std::vector<unsigned> m;
                    for (size_t j = 0; j < v.size(); j += 2) {
                        m.push_back(u16(v, j));
                    }
                    for (size_t j = 0; j < m.size(); ++j) {
                        if (m[j] == 0 || (j > 0 && m[j] <= m[j - 1]) || !keys.count(m[j])) {
                            return false;
                        }
                    }
                    break;
                }
                case 1: {
                    if (v.empty()) {
                        return false;
                    }
                    size_t j = 0;
                    while (j < v.size()) {
                        size_t len = uint8_t(v[j]);
                        if (len == 0 || j + 1 + len > v.size()) {
                            return false;
                        }
                        j += 1 + len;
                    }
                    break;
                }
                case 2:
                    if (!v.empty()) {
                        return false;
                    }
                    break;
                case 3:
                    if (v.size() != 2) {
                        return false;
                    }
                    break;
                case 4:
                    if (v.empty() || v.size() % 4) {
                        return false;
                    }
                    break;
                case 6:
                    if (v.empty() || v.size() % 16) {
                        return false;
                    }
                    break;
                case 65535:
                    return false;
                default:
                    break;
            }
        }
        return true;
    }

    std::string bytes(const sgcl::vector<sgcl::byte>& v) {
        return std::string(reinterpret_cast<const char*>(v.data()), v.size());
    }

    std::string be16(unsigned v) {
        return std::string{char(v >> 8), char(v & 0xFF)};
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 60000) {
        return 0;
    }
    const unsigned priority = data[0] & 0x7F ? data[0] & 0x7F : 0;   // 0..127, 0 the alias
    const uint16_t type = data[0] & 0x80 ? nd::dns_type::https : nd::dns_type::svcb;
    const std::string params(reinterpret_cast<const char*>(data + 1), size - 1);
    const uint8_t* p = data + 1;   // libFuzzer's own buffer: its end is the input's

    std::vector<Param> list;
    const bool ok = nd::dns_svc_params_ok(p, params.size());
    check(ok == reference(params, list));
    if (ok) {
        std::vector<Param> walked;
        nd::dns_svc_params_each(p, params.size(), [&](uint16_t k, const uint8_t* v, size_t n) {
            walked.push_back({k, std::string(reinterpret_cast<const char*>(v), n)});
        });
        check(walked.size() == list.size());
        std::string again;
        for (size_t i = 0; i < walked.size(); ++i) {
            check(walked[i].key == list[i].key && walked[i].value == list[i].value);
            again += be16(walked[i].key) + be16(unsigned(walked[i].value.size())) + walked[i].value;
        }
        check(again == params);
    }

    // the record in an answer: owner q.test., target "." or t.test.
    for (bool dot : {true, false}) {
        std::string target = dot ? std::string(1, '\0') : std::string("\x01t\x04test\x00", 8);
        std::string rdata = be16(priority) + target + params;
        if (rdata.size() > 65000) {
            return 0;
        }
        std::string m = be16(0x1234) + be16(0x8180) + be16(1) + be16(1) + be16(0) + be16(0);
        m += std::string("\x01q\x04test\x00", 8) + be16(type) + be16(1);
        m += std::string("\xc0\x0c", 2) + be16(type) + be16(1) + be16(0) + be16(60) + be16(unsigned(rdata.size())) + rdata;
        nd::DnsName q;
        check(nd::dns_name_from_text("q.test.", q));
        // the message in a malloc buffer of exactly its size
        uint8_t* exact = static_cast<uint8_t*>(std::malloc(m.size()));
        check(exact != nullptr);
        std::memcpy(exact, m.data(), m.size());
        nd::DnsAnswer a;
        auto st = nd::dns_read_answer(exact, m.size(), 0x1234, q, type, false, a);
        std::free(exact);
        check((st == nd::DnsStatus::ok) == (priority == 0 || ok));
        if (st != nd::DnsStatus::ok) {
            continue;
        }
        check(a.records.size() == 1);
        const auto& r = a.records[0];
        check(r.first == priority);
        check(std::string(r.name.view()) == (dot ? "." : "t.test."));
        if (priority == 0) {
            check(r.text.empty());
            continue;
        }
        check(std::string(r.text.view()) == params);
        // typed from the input's own bytes, not from the record's managed copy
        sgcl::net::dns::svcb s = nd::DnsAccess::svcb_of(r.first, r.name, a.end, p, params.size());
        check(s.priority == priority);
        check(std::string(s.target.view()) == (dot ? "q.test." : "t.test."));
        size_t others = 0;
        for (auto& [k, v] : list) {
            switch (k) {
                case 0:
                    check(s.mandatory.size() == v.size() / 2);
                    for (size_t j = 0; j < s.mandatory.size(); ++j) {
                        check(s.mandatory[j] == u16(v, 2 * j));
                    }
                    break;
                case 1: {
                    std::string joined;
                    for (auto& id : s.alpn) {
                        check(!id.empty() && id.size() < 256);
                        joined += char(id.size());
                        joined += std::string(id.view());
                    }
                    check(joined == v);
                    break;
                }
                case 2:
                    check(s.no_default_alpn);
                    break;
                case 3:
                    check(s.port == u16(v, 0));
                    break;
                case 4:
                    check(s.ipv4_hints.size() == v.size() / 4);
                    for (size_t j = 0; j < s.ipv4_hints.size(); ++j) {
                        auto b = s.ipv4_hints[j].bytes();
                        check(s.ipv4_hints[j].is_v4() && std::string(reinterpret_cast<const char*>(b.data()) + 12, 4) == v.substr(4 * j, 4));
                    }
                    break;
                case 5:
                    check(bytes(s.ech) == v);
                    break;
                case 6:
                    check(s.ipv6_hints.size() == v.size() / 16);
                    for (size_t j = 0; j < s.ipv6_hints.size(); ++j) {
                        auto b = s.ipv6_hints[j].bytes();
                        check(std::string(reinterpret_cast<const char*>(b.data()), 16) == v.substr(16 * j, 16));
                    }
                    break;
                case 7:
                    check(std::string(s.dohpath.view()) == v);
                    break;
                default:
                    check(others < s.params.size());
                    check(s.params[others].key == k && bytes(s.params[others].value) == v);
                    ++others;
            }
        }
        check(others == s.params.size());
    }
    return 0;
}
