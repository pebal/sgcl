//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::ldap on any bytes; the first byte picks what the rest is:
//   0  a filter's text (RFC 4515): one that reads is a BER element that
//      reads back as DER, a context-specific tag of 0 to 9
//   1  a value: filter_escape of it inside "(cn=...)" reads, and its
//      equality's value is the value itself
//   2  a server's bytes to the client's reader: framed, parsed, each
//      operation read as a result, an entry or a reference as its tag says
// Every parse reads a malloc'd block of exactly its bytes, never a managed
// copy, so a read past the end is ASan's to see.
// Built with libFuzzer (tests/fuzz/run.sh tests/net/fuzz/ldap_fuzz.cpp) or
// replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/net/ldap.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using encoding::asn1;
    namespace ld = sgcl::net::ldap::detail;

    // The bytes in a malloc'd block of exactly their size, never a managed
    // or a std::string's copy: a read past their end is ASan's to see
    class Exact {
    public:
        explicit Exact(std::string_view s)
        : _p(static_cast<char*>(std::malloc(s.size()))), _n(s.size()) {
            std::copy_n(s.data(), s.size(), _p);
        }

        Exact(const Exact&) = delete;
        Exact& operator=(const Exact&) = delete;

        ~Exact() {
            std::free(_p);
        }

        std::string_view view() const noexcept {
            return std::string_view(_p, _n);
        }

    private:
        char* _p;
        size_t _n;
    };

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    expected<asn1, encoding::error> parse(const Exact& b) {
        return asn1::parse(slice<const byte>(reinterpret_cast<const byte*>(b.view().data()), b.view().size()), asn1::ber);
    }

    void filter(std::string_view text) {
        asn1 f = ld::ldap_filter(text);
        if (!f) {
            return;
        }
        check(f.cls() == asn1::tag_class::context_specific && f.tag() <= 9);
        auto b = f.bytes();
        Exact der(std::string_view(reinterpret_cast<const char*>(b.data()), b.size()));
        auto again = asn1::parse(slice<const byte>(reinterpret_cast<const byte*>(der.view().data()), b.size()));
        check(again && *again == f);
    }

    void escape(std::string_view value) {
        string escaped = net::ldap::filter_escape(string(value));
        asn1 f = ld::ldap_filter(std::string("(cn=") + std::string(escaped.view()) + ")");
        check(bool(f));
        if (value.empty()) {
            return;
        }
        check(f.tag() == 3 && ld::ldap_text(f[1]) == value);
    }

    void messages(std::string_view bytes) {
        for (int i = 0; i < 64; ++i) {
            size_t total = 0;
            int r = ld::ldap_frame(bytes, total);
            if (r <= 0 || total > bytes.size()) {
                return;
            }
            Exact one(bytes.substr(0, total));   // the element alone, as the client's reader cuts it
            bytes.remove_prefix(total);
            if (!ld::ldap_definite(reinterpret_cast<const uint8_t*>(one.view().data()), total)) {
                continue;
            }
            auto m = parse(one);
            if (!m || !m->is(asn1::type::sequence) || m->size() < 2) {
                continue;
            }
            asn1 op = (*m)[1];
            net::ldap::result res;
            net::ldap::entry e;
            if (op.tag() == ld::op::search_entry) {
                (void)ld::ldap_read_entry(op, e);
            } else if (op.tag() == ld::op::search_reference) {
                for (auto u : op) {
                    (void)ld::ldap_text(u);
                }
            } else {
                (void)ld::ldap_read_result(op, res);
            }
            if (m->size() > 2) {
                for (auto c : (*m)[2]) {
                    auto v = c[c.size() - 1].as_bytes();
                    if (v && ld::ldap_definite(reinterpret_cast<const uint8_t*>(v->data()), v->size())) {
                        Exact value(std::string_view(reinterpret_cast<const char*>(v->data()), v->size()));
                        (void)parse(value);
                    }
                }
            }
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) {
        return 0;
    }
    std::string_view text(reinterpret_cast<const char*>(data + 1), size - 1);
    switch (data[0] % 3) {
        case 0: filter(text); break;
        case 1: escape(text); break;
        case 2: messages(text); break;
    }
    return 0;
}
