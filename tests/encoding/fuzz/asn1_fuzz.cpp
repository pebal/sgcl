//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::asn1 on any bytes. The first byte picks DER or BER, the depth
// bound and the size of the pieces a stream hands out; the rest is the
// input. What must hold:
//   - an element parsed is walked whole (every value asked of every
//     element, the dump made) without a fault, and its bytes parse again to
//     the same element: in DER the input itself, in BER the definite form,
//     which a second reading leaves as it is;
//   - in DER every element is made again from its value — an INTEGER from
//     its number, an OID from its arcs, a string from its text, a SEQUENCE
//     from its elements, anything from its tag and content through raw —
//     and comes out the same bytes;
//   - a stream of the input in pieces reads the element parse reads (the
//     input is that one element), or, where parse refuses, refuses too or
//     reads an element that is a prefix of the input;
//   - a dotted OID parsed writes back as the same text, and parses again.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/asn1_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(std::string s, size_t n) : _s(std::move(s)), _n(n) {}

        expected<size_t, io::error> read(slice<byte> out) {
            size_t k = std::min({out.size(), _n, _s.size() - _at});
            std::memcpy(out.data(), _s.data() + _at, k);
            _at += k;
            return k;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

    private:
        std::string _s;
        size_t _n;
        size_t _at = 0;
    };

    bool same(const slice<const byte>& a, const slice<const byte>& b) {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
    }

    // Every value asked; in DER the element made again from it
    void visit(const asn1& e, bool der) {
        auto b = e.as_bool();
        auto i = e.as_int();
        auto big = e.as_big_integer();
        auto bytes = e.as_bytes();
        auto bits = e.as_bits();
        auto id = e.as_oid();
        auto text = e.as_string();
        auto when = e.as_time();
        if (i) {
            check(big && *big == math::big_integer(*i));
        }
        if (bits) {
            check(bits->length <= bits->bytes.size() * 8 && bits->length + 8 > bits->bytes.size() * 8);
            for (size_t k = 0; k < bits->length; k += 7) {
                (void)(*bits)[k];
            }
        }
        if (id) {
            check(asn1::oid::parse(id->to_string()).value() == *id);
        }
        (void)when;
        if (!der) {
            return;
        }
        // made again, the same bytes
        check(asn1::raw(e.cls(), e.tag(), e.constructed(), e.content()) == e);
        if (e.cls() != asn1::tag_class::universal) {
            return;
        }
        switch (asn1::type(e.tag())) {
            case asn1::type::boolean:
                check(b && asn1::boolean(*b) == e);
                break;
            case asn1::type::integer:
                check(big && asn1::integer(*big) == e);
                if (i) {
                    check(asn1::integer(*i) == e);
                }
                break;
            case asn1::type::enumerated:
                check(!i || asn1::enumerated(*i) == e);
                break;
            case asn1::type::null:
                check(asn1::null() == e);
                break;
            case asn1::type::object_identifier:
                check(!id || asn1::object_identifier(*id) == e);
                break;
            case asn1::type::octet_string:
                check(bytes && asn1::octet_string(*bytes) == e);
                break;
            case asn1::type::bit_string:
                check(bits && asn1::bit_string(bits->bytes, bits->length) == e);
                break;
            case asn1::type::utf8_string:
                check(text && asn1::utf8_string(*text) == e);
                break;
            case asn1::type::printable_string:
                check(text && asn1::printable_string(*text) == e);
                break;
            case asn1::type::ia5_string:
                check(text && asn1::ia5_string(*text) == e);
                break;
            case asn1::type::bmp_string:
                check(text && asn1::bmp_string(*text) == e);
                break;
            case asn1::type::utc_time:
                check(when && asn1::utc_time(*when) == e);
                break;
            case asn1::type::sequence: {
                vector<asn1> all;
                for (auto c : e) {
                    all.push_back(c);
                }
                check(asn1::sequence(all) == e);
                break;
            }
            default:
                break;
        }
    }

    void walk(const asn1& top, bool der) {
        vector<asn1> todo;
        todo.push_back(top);
        size_t seen = 0;
        while (!todo.empty() && seen < 20000) {
            asn1 e = todo.back();
            todo.pop_back();
            ++seen;
            visit(e, der);
            size_t n = 0;
            for (auto c : e) {
                check(c == e[n]);
                todo.push_back(c);
                ++n;
            }
            check(n == e.size());
            check(!e[n]);
        }
        (void)top.to_string();
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    uint8_t mode = data[0];
    asn1::options o;
    o.ber = mode & 1;
    o.max_depth = (mode & 2) ? 8 : 512;
    size_t piece = 1 + (mode >> 2) % 9;
    auto in = slice<const byte>(reinterpret_cast<const byte*>(data + 1), size - 1);
    // first from a buffer of exactly the input's size outside the managed
    // heap, where ASan sees a read past its end
    {
        auto* exact = static_cast<byte*>(std::malloc(in.size() ? in.size() : 1));
        std::copy(in.begin(), in.end(), exact);
        auto x = asn1::parse(slice<const byte>(exact, in.size()), o);
        (void)x;
        std::free(exact);
    }
    vector<byte> owned(in.begin(), in.end());
    auto e = asn1::parse(owned, o);
    if (e) {
        walk(*e, !o.ber);
        auto again = asn1::parse(e->bytes(), o);
        check(again && *again == *e);
        if (!o.ber) {
            check(same(e->bytes(), owned.as_slice()));
        }
    }
    // the same through a stream in pieces
    std::string text(reinterpret_cast<const char*>(in.data()), in.size());
    io::reader r(make_tracked<dribble>(text, piece));
    auto s = asn1::parse(r, o);
    if (e) {
        check(s && *s == *e);
    } else if (s && !o.ber) {
        check(s->bytes().size() <= in.size() && std::memcmp(s->bytes().data(), in.data(), s->bytes().size()) == 0);
    }
    // the input as the text of an OID
    auto id = asn1::oid::parse(string(std::string_view(text)));
    if (id) {
        check(id->to_string() == text);
        check(asn1::oid::parse(id->to_string()).value() == *id);
    }
    return 0;
}
