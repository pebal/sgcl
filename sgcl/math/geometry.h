//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/detail/os.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

// The shapes of a plane: a point, a size, a rectangle, in float for what a
// user interface and SVG draw and in int32_t for pixels, and the affine
// transform of the plane, SVG's matrix(a, b, c, d, e, f). Plain values of a
// few numbers, with no pointer in them: they live anywhere, and their
// arithmetic is the processor's own.
//
// A rectangle is x, y, width and height, as CSS, SVG and a DOMRect write it,
// with y growing downwards on a screen and upwards in a drawing alike — the
// type does not care which. It holds the points with left <= x < right and
// top <= y < bottom, so that tiles meet without overlapping; one of no width
// or height holds nothing.
//
// The transforms compose as matrices do: (l * r).apply(p) is
// l.apply(r.apply(p)), r first — so SVG's transform="translate(…) rotate(…)"
// is translation(…) * rotation(…), read in the order it is written.
namespace sgcl::math {
    struct int_point;
    struct int_size;
    struct int_rect;

    // A point of the plane, or a step across it (the difference of two)
    struct point {
        float x = 0;
        float y = 0;

        constexpr point() noexcept = default;

        SGCL_INLINE_HOT constexpr point(float x, float y) noexcept
        : x(x)
        , y(y) {
        }

        constexpr explicit point(const int_point& p) noexcept;

        // The distance to another point
        SGCL_INLINE_HOT float distance(const point& other) const noexcept {
            return std::hypot(x - other.x, y - other.y);
        }

        SGCL_INLINE_HOT friend constexpr point operator+(const point& a, const point& b) noexcept {
            return {a.x + b.x, a.y + b.y};
        }

        SGCL_INLINE_HOT friend constexpr point operator-(const point& a, const point& b) noexcept {
            return {a.x - b.x, a.y - b.y};
        }

        SGCL_INLINE_HOT friend constexpr point operator*(const point& a, float k) noexcept {
            return {a.x * k, a.y * k};
        }

        SGCL_INLINE_HOT friend constexpr point operator*(float k, const point& a) noexcept {
            return {a.x * k, a.y * k};
        }

        SGCL_INLINE_HOT friend constexpr point operator/(const point& a, float k) noexcept {
            return {a.x / k, a.y / k};
        }

        SGCL_INLINE_HOT constexpr point operator-() const noexcept {
            return {-x, -y};
        }

        SGCL_INLINE_HOT constexpr point& operator+=(const point& b) noexcept {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT constexpr point& operator-=(const point& b) noexcept {
            return *this = *this - b;
        }

        SGCL_INLINE_HOT constexpr point& operator*=(float k) noexcept {
            return *this = *this * k;
        }

        SGCL_INLINE_HOT constexpr point& operator/=(float k) noexcept {
            return *this = *this / k;
        }

        friend constexpr bool operator==(const point&, const point&) noexcept = default;
    };

    // A width and a height
    struct size {
        float width = 0;
        float height = 0;

        constexpr size() noexcept = default;

        SGCL_INLINE_HOT constexpr size(float width, float height) noexcept
        : width(width)
        , height(height) {
        }

        constexpr explicit size(const int_size& s) noexcept;

        // Whether it covers nothing: a width or a height of zero or below
        // (or NaN)
        SGCL_INLINE_HOT constexpr bool is_empty() const noexcept {
            return !(width > 0 && height > 0);
        }

        SGCL_INLINE_HOT friend constexpr size operator*(const size& s, float k) noexcept {
            return {s.width * k, s.height * k};
        }

        SGCL_INLINE_HOT friend constexpr size operator*(float k, const size& s) noexcept {
            return {s.width * k, s.height * k};
        }

        SGCL_INLINE_HOT friend constexpr size operator/(const size& s, float k) noexcept {
            return {s.width / k, s.height / k};
        }

        friend constexpr bool operator==(const size&, const size&) noexcept = default;
    };

    // A rectangle of the plane, its sides along the axes
    struct rect {
        float x = 0;
        float y = 0;
        float width = 0;
        float height = 0;

        constexpr rect() noexcept = default;

        SGCL_INLINE_HOT constexpr rect(float x, float y, float width, float height) noexcept
        : x(x)
        , y(y)
        , width(width)
        , height(height) {
        }

        SGCL_INLINE_HOT constexpr rect(const point& origin, const size& extent) noexcept
        : x(origin.x)
        , y(origin.y)
        , width(extent.width)
        , height(extent.height) {
        }

        constexpr explicit rect(const int_rect& r) noexcept;

        // The rectangle with two opposite corners, given in any order
        SGCL_INLINE_HOT static constexpr rect from_points(const point& a, const point& b) noexcept {
            float l = std::min(a.x, b.x);
            float t = std::min(a.y, b.y);
            return {l, t, std::max(a.x, b.x) - l, std::max(a.y, b.y) - t};
        }

        SGCL_INLINE_HOT constexpr float left() const noexcept {
            return x;
        }

        SGCL_INLINE_HOT constexpr float top() const noexcept {
            return y;
        }

        SGCL_INLINE_HOT constexpr float right() const noexcept {
            return x + width;
        }

        SGCL_INLINE_HOT constexpr float bottom() const noexcept {
            return y + height;
        }

        SGCL_INLINE_HOT constexpr point origin() const noexcept {
            return {x, y};
        }

        SGCL_INLINE_HOT constexpr math::size extent() const noexcept {
            return {width, height};
        }

        SGCL_INLINE_HOT constexpr point center() const noexcept {
            return {x + width / 2, y + height / 2};
        }

        // Whether it covers nothing: a width or a height of zero or below
        SGCL_INLINE_HOT constexpr bool is_empty() const noexcept {
            return !(width > 0 && height > 0);
        }

        // Whether the point is inside: left <= x < right and top <= y <
        // bottom, so a point on a shared edge belongs to one tile
        SGCL_INLINE_HOT constexpr bool contains(const point& p) const noexcept {
            return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
        }

        // Whether the other lies wholly inside; an empty one lies inside
        // none, and none inside an empty one
        SGCL_INLINE_HOT constexpr bool contains(const rect& r) const noexcept {
            return !is_empty() && !r.is_empty() && r.x >= x && r.y >= y && r.right() <= right() && r.bottom() <= bottom();
        }

        // Whether the two share any point; an empty one shares none
        SGCL_INLINE_HOT constexpr bool intersects(const rect& r) const noexcept {
            return !is_empty() && !r.is_empty() && r.x < right() && x < r.right() && r.y < bottom() && y < r.bottom();
        }

        // The part both cover, an empty rectangle (all zero) when they do
        // not meet
        SGCL_INLINE_HOT constexpr rect intersection(const rect& r) const noexcept {
            if (!intersects(r)) {
                return {};
            }
            float l = std::max(x, r.x);
            float t = std::max(y, r.y);
            return {l, t, std::min(right(), r.right()) - l, std::min(bottom(), r.bottom()) - t};
        }

        // The smallest rectangle covering both; an empty one adds nothing
        SGCL_INLINE_HOT constexpr rect united(const rect& r) const noexcept {
            if (r.is_empty()) {
                return *this;
            }
            if (is_empty()) {
                return r;
            }
            float l = std::min(x, r.x);
            float t = std::min(y, r.y);
            return {l, t, std::max(right(), r.right()) - l, std::max(bottom(), r.bottom()) - t};
        }

        SGCL_INLINE_HOT constexpr rect translated(float dx, float dy) const noexcept {
            return {x + dx, y + dy, width, height};
        }

        // Grown by dx on the left and the right and by dy on the top and
        // the bottom; shrunk by negative ones
        SGCL_INLINE_HOT constexpr rect inflated(float dx, float dy) const noexcept {
            return {x - dx, y - dy, width + 2 * dx, height + 2 * dy};
        }

        // The smallest rectangle of whole pixels holding it: the edges
        // rounded outwards
        int_rect rounded_out() const noexcept;

        friend constexpr bool operator==(const rect&, const rect&) noexcept = default;
    };

    // A point of whole numbers: a pixel, a cell of a grid
    struct int_point {
        int32_t x = 0;
        int32_t y = 0;

        constexpr int_point() noexcept = default;

        SGCL_INLINE_HOT constexpr int_point(int32_t x, int32_t y) noexcept
        : x(x)
        , y(y) {
        }

        SGCL_INLINE_HOT friend constexpr int_point operator+(const int_point& a, const int_point& b) noexcept {
            return {a.x + b.x, a.y + b.y};
        }

        SGCL_INLINE_HOT friend constexpr int_point operator-(const int_point& a, const int_point& b) noexcept {
            return {a.x - b.x, a.y - b.y};
        }

        SGCL_INLINE_HOT constexpr int_point operator-() const noexcept {
            return {-x, -y};
        }

        SGCL_INLINE_HOT constexpr int_point& operator+=(const int_point& b) noexcept {
            return *this = *this + b;
        }

        SGCL_INLINE_HOT constexpr int_point& operator-=(const int_point& b) noexcept {
            return *this = *this - b;
        }

        friend constexpr bool operator==(const int_point&, const int_point&) noexcept = default;
    };

    struct int_size {
        int32_t width = 0;
        int32_t height = 0;

        constexpr int_size() noexcept = default;

        SGCL_INLINE_HOT constexpr int_size(int32_t width, int32_t height) noexcept
        : width(width)
        , height(height) {
        }

        SGCL_INLINE_HOT constexpr bool is_empty() const noexcept {
            return width <= 0 || height <= 0;
        }

        friend constexpr bool operator==(const int_size&, const int_size&) noexcept = default;
    };

    // A rectangle of whole pixels. The edges are computed in 64 bits, so a
    // rectangle reaching past INT32_MAX still compares right
    struct int_rect {
        int32_t x = 0;
        int32_t y = 0;
        int32_t width = 0;
        int32_t height = 0;

        constexpr int_rect() noexcept = default;

        SGCL_INLINE_HOT constexpr int_rect(int32_t x, int32_t y, int32_t width, int32_t height) noexcept
        : x(x)
        , y(y)
        , width(width)
        , height(height) {
        }

        SGCL_INLINE_HOT constexpr int_rect(const int_point& origin, const int_size& extent) noexcept
        : x(origin.x)
        , y(origin.y)
        , width(extent.width)
        , height(extent.height) {
        }

        // The rectangle with two opposite corners, given in any order
        SGCL_INLINE_HOT static constexpr int_rect from_points(const int_point& a, const int_point& b) noexcept {
            int32_t l = std::min(a.x, b.x);
            int32_t t = std::min(a.y, b.y);
            return {l, t, int32_t(int64_t(std::max(a.x, b.x)) - l), int32_t(int64_t(std::max(a.y, b.y)) - t)};
        }

        SGCL_INLINE_HOT constexpr int64_t left() const noexcept {
            return x;
        }

        SGCL_INLINE_HOT constexpr int64_t top() const noexcept {
            return y;
        }

        SGCL_INLINE_HOT constexpr int64_t right() const noexcept {
            return int64_t(x) + width;
        }

        SGCL_INLINE_HOT constexpr int64_t bottom() const noexcept {
            return int64_t(y) + height;
        }

        SGCL_INLINE_HOT constexpr int_point origin() const noexcept {
            return {x, y};
        }

        SGCL_INLINE_HOT constexpr int_size extent() const noexcept {
            return {width, height};
        }

        SGCL_INLINE_HOT constexpr bool is_empty() const noexcept {
            return width <= 0 || height <= 0;
        }

        SGCL_INLINE_HOT constexpr bool contains(const int_point& p) const noexcept {
            return p.x >= x && p.x < right() && p.y >= y && p.y < bottom();
        }

        SGCL_INLINE_HOT constexpr bool contains(const int_rect& r) const noexcept {
            return !is_empty() && !r.is_empty() && r.x >= x && r.y >= y && r.right() <= right() && r.bottom() <= bottom();
        }

        SGCL_INLINE_HOT constexpr bool intersects(const int_rect& r) const noexcept {
            return !is_empty() && !r.is_empty() && r.x < right() && x < r.right() && r.y < bottom() && y < r.bottom();
        }

        SGCL_INLINE_HOT constexpr int_rect intersection(const int_rect& r) const noexcept {
            if (!intersects(r)) {
                return {};
            }
            int32_t l = std::max(x, r.x);
            int32_t t = std::max(y, r.y);
            return {l, t, int32_t(std::min(right(), r.right()) - l), int32_t(std::min(bottom(), r.bottom()) - t)};
        }

        SGCL_INLINE_HOT constexpr int_rect united(const int_rect& r) const noexcept {
            if (r.is_empty()) {
                return *this;
            }
            if (is_empty()) {
                return r;
            }
            int32_t l = std::min(x, r.x);
            int32_t t = std::min(y, r.y);
            return {l, t, int32_t(std::max(right(), r.right()) - l), int32_t(std::max(bottom(), r.bottom()) - t)};
        }

        SGCL_INLINE_HOT constexpr int_rect translated(int32_t dx, int32_t dy) const noexcept {
            return {x + dx, y + dy, width, height};
        }

        SGCL_INLINE_HOT constexpr int_rect inflated(int32_t dx, int32_t dy) const noexcept {
            return {x - dx, y - dy, width + 2 * dx, height + 2 * dy};
        }

        friend constexpr bool operator==(const int_rect&, const int_rect&) noexcept = default;
    };

    SGCL_INLINE_HOT constexpr point::point(const int_point& p) noexcept
    : x(float(p.x))
    , y(float(p.y)) {
    }

    SGCL_INLINE_HOT constexpr size::size(const int_size& s) noexcept
    : width(float(s.width))
    , height(float(s.height)) {
    }

    SGCL_INLINE_HOT constexpr rect::rect(const int_rect& r) noexcept
    : x(float(r.x))
    , y(float(r.y))
    , width(float(r.width))
    , height(float(r.height)) {
    }

    inline int_rect rect::rounded_out() const noexcept {
        auto edge = [](float v, bool up) {
            double e = up ? std::ceil(double(v)) : std::floor(double(v));
            return int64_t(std::clamp(e, double(INT32_MIN), double(INT32_MAX)));
        };
        int64_t l = edge(x, false);
        int64_t t = edge(y, false);
        int64_t r = std::max(edge(x + width, true), l);
        int64_t b = std::max(edge(y + height, true), t);
        return {int32_t(l), int32_t(t), int32_t(std::min<int64_t>(r - l, INT32_MAX)), int32_t(std::min<int64_t>(b - t, INT32_MAX))};
    }

    // An affine transform of the plane: x' = a·x + c·y + e, y' = b·x + d·y
    // + f, the six numbers of SVG's matrix(a, b, c, d, e, f) and of a
    // canvas's setTransform in that order. The identity by default.
    struct affine {
        float a = 1;
        float b = 0;
        float c = 0;
        float d = 1;
        float e = 0;
        float f = 0;

        SGCL_INLINE_HOT static constexpr affine identity() noexcept {
            return {};
        }

        SGCL_INLINE_HOT static constexpr affine translation(float dx, float dy) noexcept {
            return {1, 0, 0, 1, dx, dy};
        }

        SGCL_INLINE_HOT static constexpr affine scaling(float sx, float sy) noexcept {
            return {sx, 0, 0, sy, 0, 0};
        }

        // A turn by the angle, counter-clockwise when y grows upwards
        // (clockwise on a screen, whose y grows downwards, as SVG's rotate)
        SGCL_INLINE_HOT static affine rotation(float radians) noexcept {
            float s = std::sin(radians);
            float co = std::cos(radians);
            return {co, s, -s, co, 0, 0};
        }

        // A turn about a point: SVG's rotate(angle, cx, cy)
        SGCL_INLINE_HOT static affine rotation(float radians, const point& center) noexcept {
            return translation(center.x, center.y) * rotation(radians) * translation(-center.x, -center.y);
        }

        // SVG's skewX and skewY: x' = x + tan(angle)·y, y' = y + tan(angle)·x
        SGCL_INLINE_HOT static affine skew_x(float radians) noexcept {
            return {1, 0, std::tan(radians), 1, 0, 0};
        }

        SGCL_INLINE_HOT static affine skew_y(float radians) noexcept {
            return {1, std::tan(radians), 0, 1, 0, 0};
        }

        // r first, then l: (l * r).apply(p) == l.apply(r.apply(p))
        SGCL_INLINE_HOT friend constexpr affine operator*(const affine& l, const affine& r) noexcept {
            return {l.a * r.a + l.c * r.b, l.b * r.a + l.d * r.b, l.a * r.c + l.c * r.d,
                    l.b * r.c + l.d * r.d, l.a * r.e + l.c * r.f + l.e, l.b * r.e + l.d * r.f + l.f};
        }

        // *this = *this * r: r is applied before what the transform did
        SGCL_INLINE_HOT constexpr affine& operator*=(const affine& r) noexcept {
            return *this = *this * r;
        }

        SGCL_INLINE_HOT constexpr float determinant() const noexcept {
            return a * d - b * c;
        }

        // The transform undoing this one; nothing when it is singular (a
        // determinant of zero: everything onto a line or a point)
        optional<affine> inverse() const noexcept {
            // in double, so that a transform of large and small parts loses
            // no more than its own rounding
            double det = double(a) * d - double(b) * c;
            if (det == 0 || !std::isfinite(det)) {
                return nullopt;
            }
            double ia = d / det;
            double ib = -b / det;
            double ic = -c / det;
            double id = a / det;
            return affine{float(ia), float(ib), float(ic), float(id), float(-(ia * e + ic * f)), float(-(ib * e + id * f))};
        }

        SGCL_INLINE_HOT constexpr point apply(const point& p) const noexcept {
            return {a * p.x + c * p.y + e, b * p.x + d * p.y + f};
        }

        // A step rather than a place: the translation left out
        SGCL_INLINE_HOT constexpr point apply_vector(const point& v) const noexcept {
            return {a * v.x + c * v.y, b * v.x + d * v.y};
        }

        // The smallest rectangle holding the rectangle transformed: the box
        // of its four corners (the rectangle itself for a translation and a
        // scaling)
        rect apply(const rect& r) const noexcept {
            point p[4] = {apply(point(r.x, r.y)), apply(point(r.right(), r.y)), apply(point(r.x, r.bottom())),
                          apply(point(r.right(), r.bottom()))};
            float l = std::min({p[0].x, p[1].x, p[2].x, p[3].x});
            float t = std::min({p[0].y, p[1].y, p[2].y, p[3].y});
            float rr = std::max({p[0].x, p[1].x, p[2].x, p[3].x});
            float bb = std::max({p[0].y, p[1].y, p[2].y, p[3].y});
            return {l, t, rr - l, bb - t};
        }

        SGCL_INLINE_HOT constexpr bool is_identity() const noexcept {
            return *this == affine{};
        }

        friend constexpr bool operator==(const affine&, const affine&) noexcept = default;
    };
}
