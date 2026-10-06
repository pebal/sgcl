//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/os.h"
#include "../core/vector.h"
#include "geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// Bézier curves of the plane, the segments of SVG paths, fonts and the
// shapes of a user interface: a quadratic of three control points and a
// cubic of four, as values of points (float). What a renderer asks of one:
// a point and the tangent at a parameter, a split, the tight bounding box,
// a polyline within a tolerance and the length.
//
// The polyline is the curve at evenly spaced parameters, as many as Wang's
// formula asks: for a curve of degree d whose second differences of the
// control points are at most M long, n segments keep every chord within
// d(d-1)/8 · M / n² of the curve, so n = ceil(sqrt(d(d-1)·M / (8·tolerance))).
// The count is known before a point is computed, and the bound holds for
// every curve; it is held to 2^16 segments, a curve that would want more
// (coordinates near float's limit) getting the 2^16.
//
// The length is the integral of the speed |B'(t)| by Gauss–Legendre's rule
// of five points, on halves of halves until two levels agree within the
// tolerance (and at most twenty levels down).
namespace sgcl::math {
    struct cubic_bezier;

    namespace detail {
        constexpr size_t MaxBezierSegments = size_t(1) << 16;

        // n from Wang's formula: k·M / tolerance under the root, k = d(d-1)/8;
        // a tolerance not above zero (or NaN) is taken as a ten-thousandth
        // of the curve's own extent, never as an endless subdivision
        inline size_t wang_segments(double k, double m, double tolerance, double extent) noexcept {
            if (!(tolerance > 0)) {
                tolerance = std::max(extent * 1e-4, 1e-30);
            }
            double n = std::ceil(std::sqrt(k * m / tolerance));
            if (!(n >= 1)) {
                return 1;
            }
            return n > double(MaxBezierSegments) ? MaxBezierSegments : size_t(n);
        }

        // Gauss–Legendre on [a, b] with five points
        template<class Speed>
        double gauss5(const Speed& speed, double a, double b) noexcept {
            constexpr double X[5] = {0.0, 0.5384693101056831, -0.5384693101056831, 0.9061798459386640,
                                     -0.9061798459386640};
            constexpr double W[5] = {0.5688888888888889, 0.4786286704993665, 0.4786286704993665, 0.2369268850561891,
                                     0.2369268850561891};
            double half = (b - a) / 2;
            double mid = (a + b) / 2;
            double sum = 0;
            for (int i = 0; i < 5; ++i) {
                sum += W[i] * speed(mid + half * X[i]);
            }
            return sum * half;
        }

        template<class Speed>
        double arc_length(const Speed& speed, double a, double b, double whole, double tolerance, int depth) noexcept {
            double mid = (a + b) / 2;
            double left = gauss5(speed, a, mid);
            double right = gauss5(speed, mid, b);
            if (depth >= 20 || std::fabs(left + right - whole) <= tolerance) {
                return left + right;
            }
            return arc_length(speed, a, mid, left, tolerance / 2, depth + 1)
                 + arc_length(speed, mid, b, right, tolerance / 2, depth + 1);
        }

        SGCL_INLINE_HOT point mix(const point& a, const point& b, float t) noexcept {
            return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        }

        SGCL_INLINE_HOT double norm(double x, double y) noexcept {
            return std::sqrt(x * x + y * y);
        }

        // The roots in (0, 1) of a·t² + b·t + c, appended to t
        inline void unit_roots(double a, double b, double c, double* t, int& n) noexcept {
            auto keep = [&](double r) {
                if (r > 0 && r < 1) {
                    t[n++] = r;
                }
            };
            if (std::fabs(a) < 1e-12 * (std::fabs(b) + std::fabs(c)) || a == 0) {
                if (b != 0) {
                    keep(-c / b);
                }
                return;
            }
            double d = b * b - 4 * a * c;
            if (d < 0) {
                return;
            }
            // the root of the larger magnitude first, the other from the product
            double q = -0.5 * (b + std::copysign(std::sqrt(d), b));
            if (q != 0) {
                keep(q / a);
                keep(c / q);
            } else {
                keep(0);
            }
        }
    }

    // A quadratic Bézier curve: from p0 towards p1 and on to p2
    struct quadratic_bezier {
        point p0;
        point p1;
        point p2;

        // The point at t (0: p0, 1: p2), by de Casteljau's construction
        SGCL_INLINE_HOT point at(float t) const noexcept {
            return detail::mix(detail::mix(p0, p1, t), detail::mix(p1, p2, t), t);
        }

        // The derivative at t, the tangent's direction and the speed
        SGCL_INLINE_HOT point derivative(float t) const noexcept {
            return detail::mix(p1 - p0, p2 - p1, t) * 2;
        }

        // The two halves at t, each a quadratic of its own: the first from
        // p0 to at(t), the second from at(t) to p2
        SGCL_INLINE_HOT pair<quadratic_bezier, quadratic_bezier> split(float t) const noexcept {
            point a = detail::mix(p0, p1, t);
            point b = detail::mix(p1, p2, t);
            point m = detail::mix(a, b, t);
            return {quadratic_bezier{p0, a, m}, quadratic_bezier{m, b, p2}};
        }

        // The smallest rectangle holding the curve (not the control points):
        // the ends and the extremes where a coordinate's derivative is zero
        rect bounds() const noexcept {
            float l = std::min(p0.x, p2.x);
            float r = std::max(p0.x, p2.x);
            float t = std::min(p0.y, p2.y);
            float b = std::max(p0.y, p2.y);
            auto extreme = [](float a, float c, float d) {
                double den = double(a) - 2.0 * c + d;
                return den == 0 ? -1.0 : (double(a) - c) / den;
            };
            double tx = extreme(p0.x, p1.x, p2.x);
            if (tx > 0 && tx < 1) {
                float x = at(float(tx)).x;
                l = std::min(l, x);
                r = std::max(r, x);
            }
            double ty = extreme(p0.y, p1.y, p2.y);
            if (ty > 0 && ty < 1) {
                float y = at(float(ty)).y;
                t = std::min(t, y);
                b = std::max(b, y);
            }
            return {l, t, r - l, b - t};
        }

        // How many segments flatten() makes for the tolerance
        size_t segments(float tolerance = 0.25f) const noexcept {
            double m = detail::norm(double(p0.x) - 2.0 * p1.x + p2.x, double(p0.y) - 2.0 * p1.y + p2.y);
            return detail::wang_segments(0.25, m, tolerance, _extent());
        }

        // A polyline no further than the tolerance from the curve: p0, the
        // curve at evenly spaced parameters, p2
        vector<point> flatten(float tolerance = 0.25f) const {
            vector<point> out;
            out.push_back(p0);
            flatten(out, tolerance);
            return out;
        }

        // The same appended to out, but p0, which a path has already as the
        // end of its segment before
        void flatten(vector<point>& out, float tolerance = 0.25f) const {
            size_t n = segments(tolerance);
            out.reserve(out.size() + n);
            for (size_t i = 1; i < n; ++i) {
                out.push_back(at(float(double(i) / double(n))));
            }
            out.push_back(p2);
        }

        // The length of the curve, within about the tolerance
        float length(float tolerance = 1e-3f) const noexcept {
            auto speed = [this](double t) {
                double u = 1 - t;
                double x = 2 * (u * (double(p1.x) - p0.x) + t * (double(p2.x) - p1.x));
                double y = 2 * (u * (double(p1.y) - p0.y) + t * (double(p2.y) - p1.y));
                return detail::norm(x, y);
            };
            double whole = detail::gauss5(speed, 0, 1);
            return float(detail::arc_length(speed, 0, 1, whole, std::max(double(tolerance), 1e-9), 0));
        }

        // The same curve as a cubic: the degree raised, the control points
        // two thirds of the way from each end to p1
        cubic_bezier to_cubic() const noexcept;

        friend constexpr bool operator==(const quadratic_bezier&, const quadratic_bezier&) noexcept = default;

    private:
        double _extent() const noexcept {
            double w = std::max({p0.x, p1.x, p2.x}) - double(std::min({p0.x, p1.x, p2.x}));
            double h = std::max({p0.y, p1.y, p2.y}) - double(std::min({p0.y, p1.y, p2.y}));
            return std::max(w, h);
        }
    };

    // A cubic Bézier curve: from p0, leaving towards p1, arriving from p2,
    // at p3
    struct cubic_bezier {
        point p0;
        point p1;
        point p2;
        point p3;

        // The point at t (0: p0, 1: p3), by de Casteljau's construction
        SGCL_INLINE_HOT point at(float t) const noexcept {
            point a = detail::mix(p0, p1, t);
            point b = detail::mix(p1, p2, t);
            point c = detail::mix(p2, p3, t);
            return detail::mix(detail::mix(a, b, t), detail::mix(b, c, t), t);
        }

        // The derivative at t, the tangent's direction and the speed
        SGCL_INLINE_HOT point derivative(float t) const noexcept {
            point a = p1 - p0;
            point b = p2 - p1;
            point c = p3 - p2;
            return detail::mix(detail::mix(a, b, t), detail::mix(b, c, t), t) * 3;
        }

        // The two halves at t, each a cubic of its own
        SGCL_INLINE_HOT pair<cubic_bezier, cubic_bezier> split(float t) const noexcept {
            point a = detail::mix(p0, p1, t);
            point b = detail::mix(p1, p2, t);
            point c = detail::mix(p2, p3, t);
            point ab = detail::mix(a, b, t);
            point bc = detail::mix(b, c, t);
            point m = detail::mix(ab, bc, t);
            return {cubic_bezier{p0, a, ab, m}, cubic_bezier{m, bc, c, p3}};
        }

        // The smallest rectangle holding the curve: the ends and the
        // extremes, the roots of each coordinate's derivative, a quadratic
        rect bounds() const noexcept {
            float l = std::min(p0.x, p3.x);
            float r = std::max(p0.x, p3.x);
            float t = std::min(p0.y, p3.y);
            float b = std::max(p0.y, p3.y);
            auto roots = [](double a0, double a1, double a2, double a3, double* out, int& n) {
                // B'(t)/3 = (1-t)²(a1-a0) + 2(1-t)t(a2-a1) + t²(a3-a2)
                double p = a1 - a0;
                double q = a2 - a1;
                double s = a3 - a2;
                detail::unit_roots(p - 2 * q + s, 2 * (q - p), p, out, n);
            };
            double ts[2];
            int n = 0;
            roots(p0.x, p1.x, p2.x, p3.x, ts, n);
            for (int i = 0; i < n; ++i) {
                float x = at(float(ts[i])).x;
                l = std::min(l, x);
                r = std::max(r, x);
            }
            n = 0;
            roots(p0.y, p1.y, p2.y, p3.y, ts, n);
            for (int i = 0; i < n; ++i) {
                float y = at(float(ts[i])).y;
                t = std::min(t, y);
                b = std::max(b, y);
            }
            return {l, t, r - l, b - t};
        }

        // How many segments flatten() makes for the tolerance
        size_t segments(float tolerance = 0.25f) const noexcept {
            double m = std::max(detail::norm(double(p0.x) - 2.0 * p1.x + p2.x, double(p0.y) - 2.0 * p1.y + p2.y),
                                detail::norm(double(p1.x) - 2.0 * p2.x + p3.x, double(p1.y) - 2.0 * p2.y + p3.y));
            return detail::wang_segments(0.75, m, tolerance, _extent());
        }

        // A polyline no further than the tolerance from the curve: p0, the
        // curve at evenly spaced parameters, p3
        vector<point> flatten(float tolerance = 0.25f) const {
            vector<point> out;
            out.push_back(p0);
            flatten(out, tolerance);
            return out;
        }

        // The same appended to out, but p0
        void flatten(vector<point>& out, float tolerance = 0.25f) const {
            size_t n = segments(tolerance);
            out.reserve(out.size() + n);
            for (size_t i = 1; i < n; ++i) {
                out.push_back(at(float(double(i) / double(n))));
            }
            out.push_back(p3);
        }

        // The length of the curve, within about the tolerance
        float length(float tolerance = 1e-3f) const noexcept {
            auto speed = [this](double t) {
                double u = 1 - t;
                auto d = [&](float a0, float a1, float a2, float a3) {
                    return 3 * (u * u * (double(a1) - a0) + 2 * u * t * (double(a2) - a1) + t * t * (double(a3) - a2));
                };
                return detail::norm(d(p0.x, p1.x, p2.x, p3.x), d(p0.y, p1.y, p2.y, p3.y));
            };
            double whole = detail::gauss5(speed, 0, 1);
            return float(detail::arc_length(speed, 0, 1, whole, std::max(double(tolerance), 1e-9), 0));
        }

        friend constexpr bool operator==(const cubic_bezier&, const cubic_bezier&) noexcept = default;

    private:
        double _extent() const noexcept {
            double w = std::max({p0.x, p1.x, p2.x, p3.x}) - double(std::min({p0.x, p1.x, p2.x, p3.x}));
            double h = std::max({p0.y, p1.y, p2.y, p3.y}) - double(std::min({p0.y, p1.y, p2.y, p3.y}));
            return std::max(w, h);
        }
    };

    SGCL_INLINE_HOT cubic_bezier quadratic_bezier::to_cubic() const noexcept {
        return {p0, detail::mix(p0, p1, 2.0f / 3), detail::mix(p2, p1, 2.0f / 3), p2};
    }
}
