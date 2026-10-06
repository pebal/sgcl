//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// math::big_float from any text: the first byte picks a precision (1 to
// 256 bits) and a mode, the rest is one or two texts split at a NUL. What
// must hold:
//   - a text read writes back in hexadecimal (to_hex) to the same value at the
//     same precision, and the shortest decimal (to_string) reads back to the
//     same value — the interval it is chosen from is right, a quarter below a
//     power of two included; an error's offset is within the text;
//   - every operation is the exact result rounded once: a + b, a - b, a·b and
//     a/b against rational's exact arithmetic rounded by the constructor from
//     a fraction (a different road: a division with a remainder), the zeros
//     compared as values; the root rounded down squares to at most the value
//     and rounded up to at least;
//   - to_scientific reads back; txt::format's e, f, g and a with a few digits
//     never fail.
// Values whose exponents pass a few thousand are read and left: their
// decimals are of that many digits.
// Built with libFuzzer (tests/fuzz/run.sh tests/math/fuzz/big_float_fuzz.cpp)
// or replayed by the library's own driver (tests/fuzz/driver.cpp).
#include "sgcl/math/math.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace {
    using namespace sgcl;
    using namespace sgcl::math;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    bool small(const big_float& x) {
        int64_t e = x.exponent();
        return x.is_infinite() || x.sign() == 0 || (e > -3000 && e < 3000);
    }

    void run(std::string_view at, std::string_view bt, bool two, uint32_t precision, rounding mode) {
        auto a = big_float::parse(string(at), precision, mode);
        if (!a) {
            check(a.error().offset() <= at.size());
            return;
        }
        check(a->precision() == precision);
        if (!small(*a)) {
            return;
        }
        auto exact = big_float::parse(a->to_hex(), precision, mode);
        check(exact && exact->to_hex() == a->to_hex() && exact->signbit() == a->signbit());
        auto shortest = big_float::parse(a->to_string(), precision);
        check(shortest && *shortest == *a);
        auto scientific = big_float::parse(a->to_scientific(), precision);
        check(scientific && *scientific == *a);
        for (const char* pattern : {"{:.0e}", "{:.3e}", "{:.17e}", "{:.3f}", "{:.3g}", "{:.5a}", "{:a}"}) {
            if (pattern[3] == 'f' && a->exponent() < -500) {
                continue;
            }
            (void)txt::format(txt::runtime(pattern), *a);
        }
        if (!two || a->is_infinite()) {
            return;
        }
        auto b = big_float::parse(string(bt), precision);
        if (!b || !small(*b) || b->is_infinite()) {
            return;
        }
        rational ra = a->to_rational();
        rational rb = b->to_rational();
        auto same = [&](const big_float& got, const rational& r) {
            if (r == 0) {
                return got.sign() == 0;
            }
            big_float want(r, precision, mode);
            return got.to_hex() == want.to_hex() && got.signbit() == want.signbit();
        };
        check(same(*a + *b, ra + rb));
        check(same(*a - *b, ra - rb));
        check(same(*a * *b, ra * rb));
        if (b->sign() != 0) {
            check(same(*a / *b, ra / rb));
        }
        if (a->sign() > 0) {
            big_float down = big_float(*a, precision, rounding::down).sqrt();
            big_float up = big_float(*a, precision, rounding::up).sqrt();
            check(down.to_rational() * down.to_rational() <= ra);
            check(up.to_rational() * up.to_rational() >= ra);
        }
        check(((*a <=> *b) == 0) == (ra == rb));
        check((*a < *b) == (ra < rb));
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2 || size > 2048) {
        return 0;
    }
    constexpr rounding Modes[] = {rounding::half_even, rounding::half_up, rounding::half_down, rounding::up,
                                  rounding::down,      rounding::ceiling, rounding::floor};
    uint32_t precision = 1 + data[0];
    rounding mode = Modes[data[1] % 7];
    std::string_view rest(reinterpret_cast<const char*>(data + 2), size - 2);
    auto nul = rest.find('\0');
    bool two = nul != std::string_view::npos;
    run(two ? rest.substr(0, nul) : rest, two ? rest.substr(nul + 1) : std::string_view(), two, precision, mode);
    return 0;
}
