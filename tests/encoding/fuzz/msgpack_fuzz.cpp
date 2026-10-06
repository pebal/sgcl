//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// encoding::msgpack on any bytes. The first byte picks the options; the rest
// is the input. What must hold: a value read writes bytes that read back to
// an equal value and write again the same bytes (the shortest formats); the
// value goes through CBOR and back unchanged when it holds no extension;
// every timestamp read gives an instant or none, without a fault; a value
// refused is refused at an offset within the input; the bytes read as a
// stream in pieces give the value the bytes give.
// Built with libFuzzer (tests/fuzz/run.sh tests/encoding/fuzz/msgpack_fuzz.cpp)
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

    bool holds_extension(const cbor& c) {
        if (c.type() == cbor::kind::extension) {
            (void)c.as_time();
            return true;
        }
        for (const auto& e : c.elements()) {
            if (holds_extension(e)) {
                return true;
            }
        }
        for (const auto& m : c.members()) {
            if (holds_extension(m.key) || holds_extension(m.value)) {
                return true;
            }
        }
        return false;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 65536) {
        return 0;
    }
    cbor::options o;
    o.allow_duplicate_keys = data[0] & 1;
    o.allow_invalid_utf8 = data[0] & 2;
    o.max_depth = (data[0] & 4) ? 4 : 64;
    vector<byte> in(reinterpret_cast<const byte*>(data + 1), reinterpret_cast<const byte*>(data + size));
    auto v = msgpack::parse(in, o);
    // the same bytes as a stream in pieces: the item read where the bytes
    // are one, and no item past the bytes
    io::reader stream(make_tracked<dribble>(data + 1, size - 1, 1 + data[0] % 5));
    auto s = msgpack::parse(stream, o);
    if (v) {
        check(s && *s == *v);
    }
    if (!v) {
        check(v.error().offset() <= in.size());
        return 0;
    }
    cbor::options lax;
    lax.allow_invalid_utf8 = o.allow_invalid_utf8;
    lax.max_depth = 64;
    auto out = msgpack::encode(*v);
    auto back = msgpack::parse(out, lax);
    check(back && *back == *v);
    auto again = msgpack::encode(*back);
    check(again.size() == out.size() && std::memcmp(again.data(), out.data(), out.size()) == 0);
    if (!holds_extension(*v)) {
        auto through = cbor::parse(v->to_bytes(), lax);
        check(through && *through == *v);
    }
    (void)v->to_string();
    return 0;
}
