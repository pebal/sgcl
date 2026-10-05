//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The TXT record of DNS-SD (RFC 6763 §6: net::dns_sd::txt_record and the
// key/value reading of sgcl/net/detail/mdns_message.h) on any bytes,
// without an oracle. The first byte chooses: even, the rest is a TXT rdata
// as it comes off the wire; odd, the rest is a list of edits of a record
// (set with a value, set alone, remove), each a byte of kind, a key and a
// value after their lengths. What must hold:
//   - the strings of an rdata fill it exactly or it is refused; the entries
//     kept have printable keys without '=', none twice without regard to
//     case, each at most 255 bytes;
//   - the entries written as an rdata and read back are the same entries;
//   - a record made of them answers contains and get as its entries say;
//   - an edit refused (invalid_argument) changes nothing; one taken leaves
//     the key there with its value (or alone), or gone, the others as they
//     were.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/dns_sd_txt_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/mdns.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    namespace nd = sgcl::net::detail;
    namespace dns_sd = sgcl::net::dns_sd;
    using sgcl::string;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    void sound(const dns_sd::txt_record& t) {
        auto& e = t.entries();
        check(e.size() == t.size());
        for (size_t i = 0; i < e.size(); ++i) {
            std::string_view v = e[i].view();
            check(v.size() <= 255);
            std::string_view key = nd::mdns_txt_key(v);
            check(nd::mdns_txt_key_valid(key));
            for (size_t j = 0; j < i; ++j) {
                check(!nd::mdns_txt_key_equal(key, nd::mdns_txt_key(e[j].view())));
            }
            string k(key);
            check(t.contains(k));
            auto value = t.get(k);
            size_t eq = v.find('=');
            check(value.has_value() == (eq != std::string_view::npos));
            if (value) {
                check(value->view() == v.substr(eq + 1));
            }
        }
        // the record as an rdata, read back
        std::vector<std::string> entries;
        for (auto& x : e) {
            entries.emplace_back(x.view());
        }
        std::vector<std::string> strings;
        check(nd::mdns_txt_strings(nd::mdns_txt_rdata(entries), strings));
        check(nd::mdns_txt_entries(strings) == entries);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    bool edits = data[0] & 1;
    std::string rest(reinterpret_cast<const char*>(data + 1), size - 1);
    if (!edits) {
        std::vector<std::string> strings;
        if (!nd::mdns_txt_strings(rest, strings)) {
            return 0;
        }
        size_t total = 0;
        for (auto& s : strings) {
            check(s.size() <= 255);
            total += s.size() + 1;
        }
        check(total == rest.size());
        auto entries = nd::mdns_txt_entries(strings);
        dns_sd::txt_record t;
        nd::TxtAccess::assign(t, entries);
        sound(t);
        return 0;
    }
    dns_sd::txt_record t;
    size_t p = 0;
    while (p + 3 <= rest.size()) {
        uint8_t kind = uint8_t(rest[p]) % 3;
        size_t klen = uint8_t(rest[p + 1]), vlen = uint8_t(rest[p + 2]);
        p += 3;
        if (rest.size() - p < klen + vlen) {
            break;
        }
        string key(std::string_view(rest).substr(p, klen));
        string value(std::string_view(rest).substr(p + klen, vlen));
        p += klen + vlen;
        auto before = t.entries();
        bool had = t.contains(key);
        try {
            if (kind == 0) {
                t.set(key, value);
                check(t.get(key).has_value() && *t.get(key) == value);
            } else if (kind == 1) {
                t.set(key);
                check(t.contains(key) && !t.get(key).has_value());
            } else {
                check(t.remove(key) == had);
                check(!t.contains(key));
            }
        } catch (const std::invalid_argument&) {
            check(kind != 2);
            check(t.entries() == before);
        }
        sound(t);
    }
    return 0;
}
