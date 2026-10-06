//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::cbor on any bytes. The first byte picks the options (duplicate
// keys, invalid UTF-8, a small depth); the rest is the input. What must hold:
// a value read writes, in the preferred and in the deterministic
// serialization, bytes that read back to an equal value of the same hash and
// write again the same bytes; its diagnostic notation and its JSON are made
// without a fault, and the JSON read back as CBOR writes the same JSON; a
// value refused is refused at an offset within the input; the bytes read as
// a stream in pieces give the value the bytes give.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/cbor_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/encoding/encoding.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {
    using namespace sgcl;
    using namespace sgcl::encoding;

    class dribble final : public io::mixin::reader<dribble> {
    public:
        dribble(const uint8_t* p, size_t n, size_t piece) : _p(p), _n(n), _piece(piece) {}

        expected<size_t, io::error> read(slice<byte> out) {
            size_t k = std::min({out.size(), _piece, _n - _at});
            std::memcpy(out.data(), _p + _at, k);
            _at += k;
            return k;
        }

        async::task<expected<size_t, io::error>> async_read(slice<byte> out) {
            co_return read(out);
        }

    private:
        const uint8_t* _p;
        size_t _n;
        size_t _piece;
        size_t _at = 0;
    };

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool same(const vector<byte>& a, const vector<byte>& b) {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) == 0);
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    cbor::options o;
    o.allow_duplicate_keys = data[0] & 1;
    o.allow_invalid_utf8 = data[0] & 2;
    o.max_depth = (data[0] & 4) ? 4 : 512;
    vector<byte> in(reinterpret_cast<const byte*>(data + 1), reinterpret_cast<const byte*>(data + size));
    auto v = cbor::parse(in, o);
    // the same bytes as a stream in pieces: the item read where the bytes
    // are one, and no item past the bytes
    io::reader stream(make_tracked<dribble>(data + 1, size - 1, 1 + data[0] % 5));
    auto s = cbor::parse(stream, o);
    if (v) {
        check(s && *s == *v);
    }
    if (!v) {
        check(v.error().offset() <= in.size());
        return 0;
    }
    cbor::options lax;
    lax.allow_invalid_utf8 = o.allow_invalid_utf8;
    for (const auto& style : {cbor::preferred, cbor::deterministic}) {
        auto out = v->to_bytes(style);
        auto back = cbor::parse(out, lax);
        check(back && *back == *v && back->hash() == v->hash());
        check(same(back->to_bytes(style), out));
    }
    (void)v->to_string();
    if (!o.allow_invalid_utf8) {
        json j = v->to_json();
        check(cbor::from_json(j).to_json() == j);
    }
    return 0;
}
