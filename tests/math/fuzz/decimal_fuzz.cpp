//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// math::decimal from any text: the first byte picks a number of digits, a
// scale and a rounding mode, the rest is one or two texts split at a NUL.
// What must hold:
//   - a text read writes back in scientific form to the same
//     representation and in plain form to the same value — the limit of a
//     million on the place of the first digit lets every value read be
//     written and read again; an error's offset is within the text;
//   - with a second value: (a + b) - b == a, a - b == -(b - a), a·b == b·a,
//     q·b + r == a for div_rem with |r| < |b| and r of a's sign, the order
//     of a and b is the sign of a - b, equal values hash alike;
//   - a quotient to n digits has at most n, rescaling is idempotent, a
//     square root squared brackets the value in the modes down and up;
//   - against libmpdec, when its header is found (Python's decimal is
//     built on it): the scientific text of a, of a + b, a - b, a·b, a % b,
//     the whole quotient, the quotient to n digits in the mode, the square
//     root to n digits (half-even, as libmpdec rounds it), a rescaled to the
//     scale and rounded to n digits in the mode — the representation, not
//     only the value. Build with
//       SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/mpdecimal/include -L/opt/homebrew/opt/mpdecimal/lib -lmpdec" \
//           sh tests/fuzz/run.sh tests/math/fuzz/decimal_fuzz.cpp 300
// Exponents far apart are kept to a few thousand, so that a sum stays
// within libFuzzer's time per input; parse's limit of a million is checked
// on its own.
#include "sgcl/math/math.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#if __has_include(<mpdecimal.h>)
#include <mpdecimal.h>
#define SGCL_FUZZ_MPDEC 1
#endif

namespace {
    using namespace sgcl;
    using namespace sgcl::math;

    void check(bool ok) {
        if (!ok) {
            __builtin_trap();
        }
    }

    std::string text(const string& s) {
        return std::string(s.data(), s.size());
    }

    // The exponent of a text, saturated, 0 without one
    int64_t exponent_of(std::string_view t) {
        auto e = t.find_first_of("eE");
        if (e == std::string_view::npos) {
            return 0;
        }
        auto d = t.substr(e + 1);
        bool negative = !d.empty() && d[0] == '-';
        if (!d.empty() && (d[0] == '+' || d[0] == '-')) {
            d.remove_prefix(1);
        }
        int64_t v = 0;
        for (char c : d) {
            if (c < '0' || c > '9') {
                return 0;
            }
            v = v > 100000000 ? v : v * 10 + (c - '0');
        }
        return negative ? -v : v;
    }

    constexpr rounding Modes[] = {rounding::half_even, rounding::half_up, rounding::half_down, rounding::up,
                                  rounding::down,      rounding::ceiling, rounding::floor};

#if defined(SGCL_FUZZ_MPDEC)
    constexpr int MpdModes[] = {MPD_ROUND_HALF_EVEN, MPD_ROUND_HALF_UP, MPD_ROUND_HALF_DOWN, MPD_ROUND_UP,
                                        MPD_ROUND_DOWN,      MPD_ROUND_CEILING, MPD_ROUND_FLOOR};

    struct Mpd {
        mpd_t* v;
        Mpd()
        : v(mpd_qnew()) {
        }
        ~Mpd() {
            mpd_del(v);
        }
        Mpd(const Mpd&) = delete;
        Mpd& operator=(const Mpd&) = delete;
    };

    mpd_context_t max_context() {
        mpd_context_t c;
        mpd_maxcontext(&c);
        return c;
    }

    // libmpdec's format 'e' (Python's), without the minus of a zero, which
    // decimal does not have
    std::string sci(const mpd_t* v) {
        mpd_context_t c = max_context();
        char* s = mpd_format(v, "e", &c);
        std::string out(s);
        mpd_free(s);
        if (mpd_iszero(v) && !out.empty() && out[0] == '-') {
            out.erase(0, 1);
        }
        return out;
    }

    void against_libmpdec(const decimal& a, std::string_view at, const decimal* b, std::string_view bt, int digits,
                          int32_t scale, int mode) {
        mpd_context_t c = max_context();
        uint32_t status = 0;
        Mpd x;
        mpd_qset_string(x.v, std::string(at).c_str(), &c, &status);
        check(sci(x.v) == text(a.to_scientific()));
        mpd_context_t p = max_context();
        p.prec = digits;
        p.round = MpdModes[mode];
        Mpd r;
        if (a.sign() >= 0) {
            mpd_context_t h = max_context();
            h.prec = digits;
            mpd_qsqrt(r.v, x.v, &h, &status);
            check(sci(r.v) == text(a.sqrt(digits).to_scientific()));
        }
        mpd_qplus(r.v, x.v, &p, &status);
        check(sci(r.v) == text(a.round_precision(digits, Modes[mode]).to_scientific()));
        Mpd q;
        mpd_qsset_ssize(q.v, 1, &c, &status);
        q.v->exp = -scale;
        mpd_context_t round = max_context();
        round.round = MpdModes[mode];
        mpd_qquantize(r.v, x.v, q.v, &round, &status);
        check(sci(r.v) == text(a.rescale(scale, Modes[mode]).to_scientific()));
        if (!b) {
            return;
        }
        Mpd y;
        mpd_qset_string(y.v, std::string(bt).c_str(), &c, &status);
        mpd_qadd(r.v, x.v, y.v, &c, &status);
        check(sci(r.v) == text((a + *b).to_scientific()));
        mpd_qsub(r.v, x.v, y.v, &c, &status);
        check(sci(r.v) == text((a - *b).to_scientific()));
        mpd_qmul(r.v, x.v, y.v, &c, &status);
        check(sci(r.v) == text((a * *b).to_scientific()));
        if (b->sign() == 0) {
            return;
        }
        mpd_qrem(r.v, x.v, y.v, &c, &status);
        check(sci(r.v) == text((a % *b).to_scientific()));
        mpd_qdivint(r.v, x.v, y.v, &c, &status);
        check(sci(r.v) == text(a.div_rem(*b).first.to_scientific()));
        mpd_qdiv(r.v, x.v, y.v, &p, &status);
        check(sci(r.v) == text(a.div_precision(*b, digits, Modes[mode]).to_scientific()));
    }
#endif

    void run(std::string_view at, std::string_view bt, bool two, int digits, int32_t scale, int mode) {
        // a text whose exponent alone is past a million and a half, with
        // fewer digits than that, is refused: the place of its first digit
        // is past the million
        if (std::abs(exponent_of(at)) > 1500000 && at.size() < 400000) {
            check(!decimal::parse(string(at)));
            return;
        }
        auto a = decimal::parse(string(at));
        if (!a) {
            check(a.error().offset() <= at.size());
            return;
        }
        // a value whose text would be past a few thousand characters plain
        // is read and left
        if (std::abs(int64_t(a->scale())) > 4000 || !a->is_finite()) {
            check(decimal::parse(a->to_scientific())->identical(*a));
            return;
        }
        auto again = decimal::parse(a->to_scientific());
        check(again && again->identical(*a));
        auto plain = decimal::parse(a->to_string());
        check(plain && *plain == *a);
        check(a->rescale(scale, Modes[mode]).rescale(scale).identical(a->rescale(scale, Modes[mode])));
        decimal rounded = a->round_precision(digits, Modes[mode]);
        check(rounded.precision() <= size_t(digits));
        if (a->sign() > 0) {
            decimal down = a->sqrt(digits, rounding::down);
            decimal up = a->sqrt(digits, rounding::up);
            check(down * down <= *a && up * up >= *a);
        }
        std::hash<decimal> h;
        check(h(*a) == h(a->rescale(a->scale() + 3)));
        if (!two) {
#if defined(SGCL_FUZZ_MPDEC)
            against_libmpdec(*a, at, nullptr, {}, digits, scale, mode);
#endif
            return;
        }
        auto b = decimal::parse(string(bt));
        if (!b || std::abs(int64_t(b->scale())) > 4000 || !b->is_finite()) {
            return;
        }
        check((*a + *b) - *b == *a);
        check(*a - *b == -(*b - *a));
        check((*a * *b).identical(*b * *a));
        auto order = *a <=> *b;
        auto difference = (*a - *b).sign();
        check((order < 0) == (difference < 0) && (order == 0) == (difference == 0));
        check((*a == *b) == (h(*a) == h(*b)) || !(*a == *b));
        if (b->sign() != 0) {
            auto [q, r] = a->div_rem(*b);
            check(q * *b + r == *a);
            check(r.abs() < b->abs());
            check(r.sign() == 0 || r.sign() == a->sign());
            decimal quotient = a->div_precision(*b, digits, Modes[mode]);
            check(quotient.precision() <= size_t(digits));
        }
#if defined(SGCL_FUZZ_MPDEC)
        against_libmpdec(*a, at, &*b, bt, digits, scale, mode);
#endif
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1 || size > 4096) {
        return 0;
    }
    // the first byte: digits 1 to 40 (its low bits), a scale of -8 to 23
    // and the mode, all from it and the length
    uint8_t pick = data[0];
    int digits = 1 + pick % 40;
    auto scale = int32_t((pick >> 3) % 32) - 8;
    int mode = int((pick + size) % 7);
    std::string_view rest(reinterpret_cast<const char*>(data + 1), size - 1);
    auto nul = rest.find('\0');
    bool two = nul != std::string_view::npos;
    std::string_view at = two ? rest.substr(0, nul) : rest;
    std::string_view bt = two ? rest.substr(nul + 1) : std::string_view();
    run(at, bt, two, digits, scale, mode);
    return 0;
}
