//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/dynamic_array.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"
#include "detail/sketch.h"
#include "error.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace sgcl::concurrent {
    // The count-min sketch (Cormode and Muthukrishnan 2005): how often a
    // key was added, from depth rows of width counters, a key adding to
    // one counter of each row and its count read as the least of them. An
    // estimate is never below the true count, and above it by more than
    // ε·N (N all the counts added) with a probability under δ, for a width
    // of e/ε and a depth of ln(1/δ). The positions are double hashing of
    // one XXH3-128 of the key, one per row, reduced by multiply-shift. The
    // counters are 64-bit and added to atomically, so any number of
    // threads add and ask at once. No conservative update (Estan and
    // Varghese: only the counters below the new least raised): two
    // updates of one key at once that read the same least would raise
    // to the same value and lose a count, an estimate under the truth,
    // and a lock across the rows would cost every add its freedom from
    // locks. A handle: one tracked word to the counters, copies sharing
    // them, clone() copying them.
    namespace detail {
        struct CmsState {
            uint64_t width = 1;
            uint64_t depth = 1;
            uint64_t total = 0;
            dynamic_array<uint64_t> counters;   // depth rows of width
        };
    }

    class count_min_sketch {
    public:
        // A sketch whose estimate is within ε·N of the true count with a
        // probability of 1 - δ: width ceil(e/ε), depth ceil(ln(1/δ));
        // invalid_argument for ε or δ outside (0, 1)
        explicit count_min_sketch(double epsilon = 0.001, double delta = 0.01)
        : _s(make_tracked<detail::CmsState>()) {
            if (!(epsilon > 0 && epsilon < 1) || !(delta > 0 && delta < 1)) {
                throw std::invalid_argument("sgcl::concurrent::count_min_sketch: epsilon and delta in (0, 1)");
            }
            _shape(uint64_t(std::ceil(2.718281828459045 / epsilon)), uint64_t(std::ceil(std::log(1 / delta))));
        }

        // A sketch of depth rows of width counters: width at least one,
        // depth from 1 to 64 (invalid_argument otherwise)
        static count_min_sketch with_size(size_t width, size_t depth) {
            if (width < 1 || depth < 1 || depth > 64 || width > (size_t(1) << 40) / depth) {
                throw std::invalid_argument("sgcl::concurrent::count_min_sketch: a width of at least 1, a depth from 1 to 64");
            }
            return count_min_sketch(width, depth, Shape{});
        }

        // count more of the key
        SGCL_INLINE_HOT void add(std::string_view key, uint64_t count = 1) noexcept {
            _add(detail::sketch_hash128(key.data(), key.size()), count);
        }

        template<class Bytes>
        requires detail::SketchBytes<Bytes>
        SGCL_INLINE_HOT void add(const Bytes& bytes, uint64_t count = 1) noexcept {
            const slice<const byte>& key = bytes;
            _add(detail::sketch_hash128(key.data(), key.size()), count);
        }

        SGCL_INLINE_HOT void add(uint64_t key, uint64_t count = 1) noexcept {
            unsigned char b[8];
            detail::sketch_le64(key, b);
            _add(detail::sketch_hash128(b, 8), count);
        }

        // How often the key was added, never less than the truth
        SGCL_INLINE_HOT uint64_t estimate(std::string_view key) const noexcept {
            return _estimate(detail::sketch_hash128(key.data(), key.size()));
        }

        template<class Bytes>
        requires detail::SketchBytes<Bytes>
        SGCL_INLINE_HOT uint64_t estimate(const Bytes& bytes) const noexcept {
            const slice<const byte>& key = bytes;
            return _estimate(detail::sketch_hash128(key.data(), key.size()));
        }

        SGCL_INLINE_HOT uint64_t estimate(uint64_t key) const noexcept {
            unsigned char b[8];
            detail::sketch_le64(key, b);
            return _estimate(detail::sketch_hash128(b, 8));
        }

        // All the counts added, N
        SGCL_INLINE_HOT uint64_t total() const noexcept {
            return detail::sketch_load(_s->total);
        }

        SGCL_INLINE_HOT size_t width() const noexcept {
            return size_t(_s->width);
        }

        SGCL_INLINE_HOT size_t depth() const noexcept {
            return size_t(_s->depth);
        }

        // The other's counts added to these, as if every count added there
        // had been added here. invalid_argument for a sketch of another
        // shape; a sketch merged into itself counts everything twice
        void merge(const count_min_sketch& other) {
            if (other._s->width != _s->width || other._s->depth != _s->depth) {
                throw std::invalid_argument("sgcl::concurrent::count_min_sketch::merge: a sketch of another shape");
            }
            uint64_t* mine = _s->counters.data();
            const uint64_t* theirs = other._s->counters.data();
            for (size_t i = 0, n = _s->counters.size(); i < n; ++i) {
                if (uint64_t t = detail::sketch_load(theirs[i])) {
                    std::atomic_ref<uint64_t>(mine[i]).fetch_add(t, std::memory_order_relaxed);
                }
            }
            std::atomic_ref<uint64_t>(_s->total).fetch_add(detail::sketch_load(other._s->total), std::memory_order_relaxed);
        }

        // Every counter zero
        void clear() noexcept {
            uint64_t* c = _s->counters.data();
            for (size_t i = 0, n = _s->counters.size(); i < n; ++i) {
                std::atomic_ref<uint64_t>(c[i]).store(0, std::memory_order_relaxed);
            }
            std::atomic_ref<uint64_t>(_s->total).store(0, std::memory_order_relaxed);
        }

        // A sketch of its own with the same counters
        count_min_sketch clone() const {
            count_min_sketch c(size_t(_s->width), size_t(_s->depth), Shape{});
            c.merge(*this);
            return c;
        }

        // The sketch as bytes: "SGCM", the version 1, three zero bytes,
        // the width, the depth and the total in 8 bytes each, then the
        // counters row by row, little-endian
        vector<byte> to_bytes() const {
            size_t n = _s->counters.size();
            vector<byte> out(32 + 8 * n);
            unsigned char* p = reinterpret_cast<unsigned char*>(out.data());
            p[0] = 'S';
            p[1] = 'G';
            p[2] = 'C';
            p[3] = 'M';
            p[4] = detail::SketchVersion;
            detail::sketch_le64(_s->width, p + 8);
            detail::sketch_le64(_s->depth, p + 16);
            detail::sketch_le64(detail::sketch_load(_s->total), p + 24);
            const uint64_t* c = _s->counters.data();
            for (size_t i = 0; i < n; ++i) {
                detail::sketch_le64(detail::sketch_load(c[i]), p + 32 + 8 * i);
            }
            return out;
        }

        // A sketch from to_bytes' bytes: the shape and the size checked
        // before the counters are allocated
        static expected<count_min_sketch, error> from_bytes(const slice<const byte>& bytes) noexcept {
            if (auto e = detail::sketch_check_header(bytes, "SGCM", 32)) {
                return unexpected(*e);
            }
            const unsigned char* p = reinterpret_cast<const unsigned char*>(bytes.data());
            for (size_t i = 5; i < 8; ++i) {
                if (p[i] != 0) {
                    return unexpected(error("a reserved byte of a count-min sketch not zero", i));
                }
            }
            uint64_t width = detail::sketch_read64(p + 8);
            uint64_t depth = detail::sketch_read64(p + 16);
            if (width < 1 || width > (uint64_t(1) << 40)) {
                return unexpected(error("a count-min sketch's width from 1 to 2^40", 8));
            }
            if (depth < 1 || depth > 64) {
                return unexpected(error("a count-min sketch's depth from 1 to 64", 16));
            }
            if ((bytes.size() - 32) % 8 != 0 || (bytes.size() - 32) / 8 != width * depth) {
                return unexpected(error("a count-min sketch's size does not match its shape", bytes.size()));
            }
            count_min_sketch s(size_t(width), size_t(depth), Shape{});
            s._s->total = detail::sketch_read64(p + 24);
            uint64_t* c = s._s->counters.data();
            for (size_t i = 0, n = s._s->counters.size(); i < n; ++i) {
                c[i] = detail::sketch_read64(p + 32 + 8 * i);
            }
            return s;
        }

        SGCL_INLINE_HOT friend bool operator==(const count_min_sketch& a, const count_min_sketch& b) noexcept {
            return a._s == b._s;
        }

    private:
        struct Shape {};

        count_min_sketch(size_t width, size_t depth, Shape)
        : _s(make_tracked<detail::CmsState>()) {
            _shape(width, depth);
        }

        void _shape(uint64_t width, uint64_t depth) {
            _s->width = width < 1 ? 1 : width;
            _s->depth = depth < 1 ? 1 : depth > 64 ? 64 : depth;
            _s->counters = dynamic_array<uint64_t>(size_t(_s->width * _s->depth));
        }

        SGCL_INLINE_HOT void _add(detail::SketchHash h, uint64_t count) noexcept {
            uint64_t* c = _s->counters.data();
            const uint64_t w = _s->width;
            uint64_t x = h.low;
            for (uint64_t row = 0, d = _s->depth; row < d; ++row, x += h.high) {
                std::atomic_ref<uint64_t>(c[row * w + detail::sketch_reduce(x, w)]).fetch_add(count, std::memory_order_relaxed);
            }
            std::atomic_ref<uint64_t>(_s->total).fetch_add(count, std::memory_order_relaxed);
        }

        SGCL_INLINE_HOT uint64_t _estimate(detail::SketchHash h) const noexcept {
            const uint64_t* c = _s->counters.data();
            const uint64_t w = _s->width;
            uint64_t least = std::numeric_limits<uint64_t>::max();
            uint64_t x = h.low;
            for (uint64_t row = 0, d = _s->depth; row < d; ++row, x += h.high) {
                uint64_t v = detail::sketch_load(c[row * w + detail::sketch_reduce(x, w)]);
                least = v < least ? v : least;
            }
            return least;
        }

        tracked_ptr<detail::CmsState> _s;
    };
}
