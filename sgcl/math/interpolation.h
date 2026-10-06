//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/os.h"
#include "algebra.h"
#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Interpolation and the timing functions of animations. lerp, inverse_lerp
// and smoothstep for the numbers and the shapes of the module, as overloads;
// easing, a timing function as a value: CSS's cubic-bezier() (bezier) with its
// solver, CSS's steps(), CSS's keywords and the named set of easings.net
// (Robert Penner's equations), each a constant of the class.
namespace sgcl::math {
    // a + (b - a)·t: exactly a at t = 0 and b at t = 1, monotonic in t —
    // std::lerp's promises, std::lerp being what computes the numbers; t
    // outside [0, 1] extrapolates
    SGCL_INLINE_HOT float lerp(float a, float b, float t) noexcept {
        return std::lerp(a, b, t);
    }

    SGCL_INLINE_HOT double lerp(double a, double b, double t) noexcept {
        return std::lerp(a, b, t);
    }

    SGCL_INLINE_HOT point lerp(const point& a, const point& b, float t) noexcept {
        return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t)};
    }

    SGCL_INLINE_HOT vec2 lerp(const vec2& a, const vec2& b, float t) noexcept {
        return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t)};
    }

    SGCL_INLINE_HOT vec3 lerp(const vec3& a, const vec3& b, float t) noexcept {
        return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t)};
    }

    SGCL_INLINE_HOT vec4 lerp(const vec4& a, const vec4& b, float t) noexcept {
        return {std::lerp(a.x, b.x, t), std::lerp(a.y, b.y, t), std::lerp(a.z, b.z, t), std::lerp(a.w, b.w, t)};
    }

    // Where v lies on the way from a to b: (v - a)/(b - a), 0 at a and 1 at
    // b, not clamped; 0 when a == b, where every v is as far as any other
    SGCL_INLINE_HOT constexpr float inverse_lerp(float a, float b, float v) noexcept {
        return a == b ? 0.0f : (v - a) / (b - a);
    }

    SGCL_INLINE_HOT constexpr double inverse_lerp(double a, double b, double v) noexcept {
        return a == b ? 0.0 : (v - a) / (b - a);
    }

    // GLSL's smoothstep: 0 at and below edge0, 1 at and above edge1, and
    // 3t² - 2t³ of the way between; edges that meet are a step at them
    // (GLSL leaves that undefined); edge0 above edge1 runs the other way
    SGCL_INLINE_HOT constexpr float smoothstep(float edge0, float edge1, float x) noexcept {
        if (edge0 == edge1) {
            return x < edge0 ? 0.0f : 1.0f;
        }
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    }

    SGCL_INLINE_HOT constexpr double smoothstep(double edge0, double edge1, double x) noexcept {
        if (edge0 == edge1) {
            return x < edge0 ? 0.0 : 1.0;
        }
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
        return t * t * (3 - 2 * t);
    }

    // Where the jumps of CSS's steps() fall: jump_start at the start of
    // each interval, jump_end at its end (CSS's start and end), jump_none at
    // neither end of the whole, jump_both at both
    enum class step_position : uint8_t {
        jump_start,
        jump_end,
        jump_none,
        jump_both
    };

    // A timing function: the progress of an animation, 0 to 1, to the eased
    // progress. A value of a few numbers, compared and kept anywhere (an
    // immutable model of a user interface holds one); linear by default.
    // The progress is clamped to [0, 1]; what comes out may leave it (back,
    // elastic, a cubic-bezier with y outside [0, 1]). NaN in, NaN out.
    class easing {
        enum class Kind : uint8_t {
            linear,
            bezier,
            steps,
            in_sine,
            out_sine,
            in_out_sine,
            in_quad,
            out_quad,
            in_out_quad,
            in_cubic,
            out_cubic,
            in_out_cubic,
            in_quart,
            out_quart,
            in_out_quart,
            in_quint,
            out_quint,
            in_out_quint,
            in_expo,
            out_expo,
            in_out_expo,
            in_circ,
            out_circ,
            in_out_circ,
            in_back,
            out_back,
            in_out_back,
            in_elastic,
            out_elastic,
            in_out_elastic,
            in_bounce,
            out_bounce,
            in_out_bounce
        };

    public:
        // Linear: the progress as it is
        constexpr easing() noexcept = default;

        // CSS's cubic-bezier(x1, y1, x2, y2), named bezier so as not to be
        // taken for the curve type math::cubic_bezier: the curve from (0, 0) to
        // (1, 1) with those two control points, x the progress and y what
        // comes out; x1 and x2 are held to [0, 1], as CSS requires of them,
        // so that each progress has one value
        SGCL_INLINE_HOT static constexpr easing bezier(float x1, float y1, float x2, float y2) noexcept {
            easing e(Kind::bezier);
            e._p[0] = std::clamp(x1, 0.0f, 1.0f);
            e._p[1] = y1;
            e._p[2] = std::clamp(x2, 0.0f, 1.0f);
            e._p[3] = y2;
            return e;
        }

        // CSS's steps(count, position): the progress in count equal jumps;
        // a count below 1 is one (below 2 for jump_none, which needs two)
        SGCL_INLINE_HOT static constexpr easing steps(int count, step_position position = step_position::jump_end) noexcept {
            easing e(Kind::steps);
            int least = position == step_position::jump_none ? 2 : 1;
            e._count = count < least ? least : count;
            e._position = position;
            return e;
        }

        // CSS's keywords
        static const easing linear;
        static const easing ease;
        static const easing ease_in;
        static const easing ease_out;
        static const easing ease_in_out;

        // The named set of easings.net, Robert Penner's equations
        static const easing ease_in_sine;
        static const easing ease_out_sine;
        static const easing ease_in_out_sine;
        static const easing ease_in_quad;
        static const easing ease_out_quad;
        static const easing ease_in_out_quad;
        static const easing ease_in_cubic;
        static const easing ease_out_cubic;
        static const easing ease_in_out_cubic;
        static const easing ease_in_quart;
        static const easing ease_out_quart;
        static const easing ease_in_out_quart;
        static const easing ease_in_quint;
        static const easing ease_out_quint;
        static const easing ease_in_out_quint;
        static const easing ease_in_expo;
        static const easing ease_out_expo;
        static const easing ease_in_out_expo;
        static const easing ease_in_circ;
        static const easing ease_out_circ;
        static const easing ease_in_out_circ;
        static const easing ease_in_back;
        static const easing ease_out_back;
        static const easing ease_in_out_back;
        static const easing ease_in_elastic;
        static const easing ease_out_elastic;
        static const easing ease_in_out_elastic;
        static const easing ease_in_bounce;
        static const easing ease_out_bounce;
        static const easing ease_in_out_bounce;

        // The eased progress
        float operator()(float progress) const noexcept;

        friend constexpr bool operator==(const easing&, const easing&) noexcept = default;

    private:
        SGCL_INLINE_HOT constexpr explicit easing(Kind kind) noexcept
        : _kind(kind) {
        }

        static float _solve_bezier(const float* p, float x) noexcept;
        static double _bounce_out(double x) noexcept;

        float _p[4] = {};
        int32_t _count = 0;
        Kind _kind = Kind::linear;
        step_position _position = step_position::jump_end;
    };

    inline constexpr easing easing::linear = easing();
    inline constexpr easing easing::ease = easing::bezier(0.25f, 0.1f, 0.25f, 1.0f);
    inline constexpr easing easing::ease_in = easing::bezier(0.42f, 0.0f, 1.0f, 1.0f);
    inline constexpr easing easing::ease_out = easing::bezier(0.0f, 0.0f, 0.58f, 1.0f);
    inline constexpr easing easing::ease_in_out = easing::bezier(0.42f, 0.0f, 0.58f, 1.0f);
    inline constexpr easing easing::ease_in_sine = easing(Kind::in_sine);
    inline constexpr easing easing::ease_out_sine = easing(Kind::out_sine);
    inline constexpr easing easing::ease_in_out_sine = easing(Kind::in_out_sine);
    inline constexpr easing easing::ease_in_quad = easing(Kind::in_quad);
    inline constexpr easing easing::ease_out_quad = easing(Kind::out_quad);
    inline constexpr easing easing::ease_in_out_quad = easing(Kind::in_out_quad);
    inline constexpr easing easing::ease_in_cubic = easing(Kind::in_cubic);
    inline constexpr easing easing::ease_out_cubic = easing(Kind::out_cubic);
    inline constexpr easing easing::ease_in_out_cubic = easing(Kind::in_out_cubic);
    inline constexpr easing easing::ease_in_quart = easing(Kind::in_quart);
    inline constexpr easing easing::ease_out_quart = easing(Kind::out_quart);
    inline constexpr easing easing::ease_in_out_quart = easing(Kind::in_out_quart);
    inline constexpr easing easing::ease_in_quint = easing(Kind::in_quint);
    inline constexpr easing easing::ease_out_quint = easing(Kind::out_quint);
    inline constexpr easing easing::ease_in_out_quint = easing(Kind::in_out_quint);
    inline constexpr easing easing::ease_in_expo = easing(Kind::in_expo);
    inline constexpr easing easing::ease_out_expo = easing(Kind::out_expo);
    inline constexpr easing easing::ease_in_out_expo = easing(Kind::in_out_expo);
    inline constexpr easing easing::ease_in_circ = easing(Kind::in_circ);
    inline constexpr easing easing::ease_out_circ = easing(Kind::out_circ);
    inline constexpr easing easing::ease_in_out_circ = easing(Kind::in_out_circ);
    inline constexpr easing easing::ease_in_back = easing(Kind::in_back);
    inline constexpr easing easing::ease_out_back = easing(Kind::out_back);
    inline constexpr easing easing::ease_in_out_back = easing(Kind::in_out_back);
    inline constexpr easing easing::ease_in_elastic = easing(Kind::in_elastic);
    inline constexpr easing easing::ease_out_elastic = easing(Kind::out_elastic);
    inline constexpr easing easing::ease_in_out_elastic = easing(Kind::in_out_elastic);
    inline constexpr easing easing::ease_in_bounce = easing(Kind::in_bounce);
    inline constexpr easing easing::ease_out_bounce = easing(Kind::out_bounce);
    inline constexpr easing easing::ease_in_out_bounce = easing(Kind::in_out_bounce);

    // x(t) = 3(1-t)²t·x1 + 3(1-t)t²·x2 + t³ solved for t by Newton's method
    // from t = x, and by halving the interval where the slope is too flat
    // for it (x is monotonic in t, x1 and x2 being in [0, 1]); y(t) after.
    // In double, to 1e-13 of the progress: where the curve's x stands still
    // (a vertical tangent, cubic-bezier(1, 0, 0, 1) at its middle) the
    // parameter is known only to the cube root of that
    inline float easing::_solve_bezier(const float* p, float progress) noexcept {
        double x = progress;
        double cx = 3.0 * p[0];
        double bx = 3.0 * (p[2] - p[0]) - cx;
        double ax = 1.0 - cx - bx;
        double cy = 3.0 * p[1];
        double by = 3.0 * (p[3] - p[1]) - cy;
        double ay = 1.0 - cy - by;
        auto curve_x = [&](double t) {
            return ((ax * t + bx) * t + cx) * t;
        };
        constexpr double Epsilon = 1e-13;
        double t = x;
        bool found = false;
        for (int i = 0; i < 8; ++i) {
            double error = curve_x(t) - x;
            if (std::fabs(error) < Epsilon) {
                found = true;
                break;
            }
            double slope = (3.0 * ax * t + 2.0 * bx) * t + cx;
            if (std::fabs(slope) < 1e-6) {
                break;
            }
            t -= error / slope;
        }
        if (!found) {
            double low = 0;
            double high = 1;
            t = x;
            for (int i = 0; i < 64; ++i) {
                double v = curve_x(t);
                if (std::fabs(v - x) < Epsilon) {
                    break;
                }
                if (v < x) {
                    low = t;
                } else {
                    high = t;
                }
                t = (low + high) / 2;
            }
        }
        return float(((ay * t + by) * t + cy) * t);
    }

    inline double easing::_bounce_out(double x) noexcept {
        constexpr double N = 7.5625;
        constexpr double D = 2.75;
        if (x < 1 / D) {
            return N * x * x;
        }
        if (x < 2 / D) {
            x -= 1.5 / D;
            return N * x * x + 0.75;
        }
        if (x < 2.5 / D) {
            x -= 2.25 / D;
            return N * x * x + 0.9375;
        }
        x -= 2.625 / D;
        return N * x * x + 0.984375;
    }

    inline float easing::operator()(float progress) const noexcept {
        if (std::isnan(progress)) {
            return progress;
        }
        double x = std::clamp(progress, 0.0f, 1.0f);
        // every curve but steps() starts at 0 and ends at 1, exactly — the
        // equations of back round to 2^-52 there
        if ((x == 0 || x == 1) && _kind != Kind::steps) {
            return float(x);
        }
        constexpr double Pi = 3.14159265358979323846;
        auto power = [](double v, int n) {
            double r = 1;
            for (int i = 0; i < n; ++i) {
                r *= v;
            }
            return r;
        };
        // in, out and in-out of a power
        auto in = [&](int n) {
            return power(x, n);
        };
        auto out = [&](int n) {
            return 1 - power(1 - x, n);
        };
        auto in_out = [&](int n) {
            return x < 0.5 ? power(2, n - 1) * power(x, n) : 1 - power(-2 * x + 2, n) / 2;
        };
        constexpr double C1 = 1.70158;
        constexpr double C2 = C1 * 1.525;
        constexpr double C3 = C1 + 1;
        constexpr double C4 = 2 * Pi / 3;
        constexpr double C5 = 2 * Pi / 4.5;
        double y;
        switch (_kind) {
            case Kind::linear:
                return float(x);
            case Kind::bezier:
                if (x == 0 || x == 1) {
                    return float(x);
                }
                return _solve_bezier(_p, float(x));
            case Kind::steps: {
                // CSS Easing Functions, the step easing function: the step
                // is floor(x·count), one more for a jump at the start, held
                // to [0, jumps]
                int jumps = _count + (_position == step_position::jump_both) - (_position == step_position::jump_none);
                auto step = int64_t(std::floor(x * _count));
                if (_position == step_position::jump_start || _position == step_position::jump_both) {
                    ++step;
                }
                step = std::clamp<int64_t>(step, 0, jumps);
                return float(double(step) / jumps);
            }
            case Kind::in_sine:
                y = 1 - std::cos(x * Pi / 2);
                break;
            case Kind::out_sine:
                y = std::sin(x * Pi / 2);
                break;
            case Kind::in_out_sine:
                y = -(std::cos(Pi * x) - 1) / 2;
                break;
            case Kind::in_quad:
                y = in(2);
                break;
            case Kind::out_quad:
                y = out(2);
                break;
            case Kind::in_out_quad:
                y = in_out(2);
                break;
            case Kind::in_cubic:
                y = in(3);
                break;
            case Kind::out_cubic:
                y = out(3);
                break;
            case Kind::in_out_cubic:
                y = in_out(3);
                break;
            case Kind::in_quart:
                y = in(4);
                break;
            case Kind::out_quart:
                y = out(4);
                break;
            case Kind::in_out_quart:
                y = in_out(4);
                break;
            case Kind::in_quint:
                y = in(5);
                break;
            case Kind::out_quint:
                y = out(5);
                break;
            case Kind::in_out_quint:
                y = in_out(5);
                break;
            case Kind::in_expo:
                y = x == 0 ? 0 : std::exp2(10 * x - 10);
                break;
            case Kind::out_expo:
                y = x == 1 ? 1 : 1 - std::exp2(-10 * x);
                break;
            case Kind::in_out_expo:
                y = x == 0   ? 0
                    : x == 1 ? 1
                    : x < 0.5 ? std::exp2(20 * x - 10) / 2
                              : (2 - std::exp2(-20 * x + 10)) / 2;
                break;
            case Kind::in_circ:
                y = 1 - std::sqrt(1 - x * x);
                break;
            case Kind::out_circ:
                y = std::sqrt(1 - (x - 1) * (x - 1));
                break;
            case Kind::in_out_circ:
                y = x < 0.5 ? (1 - std::sqrt(1 - 4 * x * x)) / 2 : (std::sqrt(1 - (-2 * x + 2) * (-2 * x + 2)) + 1) / 2;
                break;
            case Kind::in_back:
                y = C3 * x * x * x - C1 * x * x;
                break;
            case Kind::out_back:
                y = 1 + C3 * power(x - 1, 3) + C1 * power(x - 1, 2);
                break;
            case Kind::in_out_back:
                y = x < 0.5 ? (power(2 * x, 2) * ((C2 + 1) * 2 * x - C2)) / 2
                            : (power(2 * x - 2, 2) * ((C2 + 1) * (x * 2 - 2) + C2) + 2) / 2;
                break;
            case Kind::in_elastic:
                y = x == 0 ? 0 : x == 1 ? 1 : -std::exp2(10 * x - 10) * std::sin((x * 10 - 10.75) * C4);
                break;
            case Kind::out_elastic:
                y = x == 0 ? 0 : x == 1 ? 1 : std::exp2(-10 * x) * std::sin((x * 10 - 0.75) * C4) + 1;
                break;
            case Kind::in_out_elastic:
                y = x == 0    ? 0
                    : x == 1  ? 1
                    : x < 0.5 ? -(std::exp2(20 * x - 10) * std::sin((20 * x - 11.125) * C5)) / 2
                              : (std::exp2(-20 * x + 10) * std::sin((20 * x - 11.125) * C5)) / 2 + 1;
                break;
            case Kind::in_bounce:
                y = 1 - _bounce_out(1 - x);
                break;
            case Kind::out_bounce:
                y = _bounce_out(x);
                break;
            case Kind::in_out_bounce:
                y = x < 0.5 ? (1 - _bounce_out(1 - 2 * x)) / 2 : (1 + _bounce_out(2 * x - 1)) / 2;
                break;
            default:
                y = x;
                break;
        }
        return float(y);
    }
}
