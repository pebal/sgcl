//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/slice.h"
#include "../../core/vector.h"
#include "../../hash/detail/xxh3.h"
#include "../error.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace sgcl::concurrent::detail {
    using namespace sgcl::detail;

    // What the sketches hash a key with: XXH3 of its bytes, seed 0, the
    // same in every process (a serialized sketch is read by another); a
    // number by its eight bytes, little-endian. From the hash module's
    // detail, which stands on core alone (the module itself depends on io,
    // and io on async, which depends on this module)
    struct SketchHash {
        uint64_t low;
        uint64_t high;
    };

    SGCL_INLINE_HOT SketchHash sketch_hash128(const void* p, size_t n) noexcept {
        auto w = hash::detail::xxh3_128(static_cast<const unsigned char*>(p), n, 0);
        return {w.low, w.high};
    }

    SGCL_INLINE_HOT uint64_t sketch_hash64(const void* p, size_t n) noexcept {
        return hash::detail::xxh3_64(static_cast<const unsigned char*>(p), n, 0);
    }

    SGCL_INLINE_HOT void sketch_le64(uint64_t v, unsigned char out[8]) noexcept {
        for (int i = 0; i < 8; ++i) {
            out[i] = (unsigned char)(v >> (8 * i));
        }
    }

    SGCL_INLINE_HOT uint64_t sketch_read64(const unsigned char* p) noexcept {
        uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= uint64_t(p[i]) << (8 * i);
        }
        return v;
    }

    // A word of a sketch read beside other threads' atomic writes
    SGCL_INLINE_HOT uint64_t sketch_load(const uint64_t& w) noexcept {
        return std::atomic_ref<uint64_t>(const_cast<uint64_t&>(w)).load(std::memory_order_relaxed);
    }

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define SGCL_SKETCH_TSAN 1
#endif
#endif
#if defined(__SANITIZE_THREAD__) && !defined(SGCL_SKETCH_TSAN)
#define SGCL_SKETCH_TSAN 1
#endif
#ifndef SGCL_SKETCH_TSAN
#define SGCL_SKETCH_TSAN 0
#endif

    // A key taken as bytes: what converts to a slice of bytes and is not
    // text (a literal, a std::string, a string go to the string_view
    // overload, which a slice would take too)
    template<class B>
    concept SketchBytes = std::is_convertible_v<const B&, slice<const byte>> && !std::is_convertible_v<const B&, std::string_view>;

    // A position in [0, n) from a hash, by multiply-shift (Lemire): the
    // high word of the product, no division
    SGCL_INLINE_HOT uint64_t sketch_reduce(uint64_t h, uint64_t n) noexcept {
        return uint64_t(((unsigned __int128)h * n) >> 64);
    }

    // The header of a serialized sketch: four bytes of magic, a version,
    // and the sketch's own fields after it
    inline constexpr unsigned char SketchVersion = 1;

    SGCL_INLINE_HOT optional<error> sketch_check_header(const slice<const byte>& bytes, const char magic[4], size_t header) noexcept {
        if (bytes.size() < header) {
            return error("too short for the sketch's header", bytes.size());
        }
        if (std::memcmp(bytes.data(), magic, 4) != 0) {
            return error("not the sketch's magic", 0);
        }
        if ((unsigned char)bytes[4] != SketchVersion) {
            return error("an unknown version of the sketch's format", 4);
        }
        return nullopt;
    }
}
