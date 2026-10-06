//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "../error.h"
#include "../secure_zero.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../encoding/base64.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// The PHC string format of Argon2 (the Password Hashing Competition's,
// as the reference implementation writes it):
//
//   $argon2id$v=19$m=65536,t=3,p=4$<salt>$<hash>
//
// the variant's name, the version, the costs in that order as decimal
// numbers without leading zeros, the salt and the hash in base64 of the
// standard alphabet without padding. Read strictly: anything else is
// malformed; a version other than 19 and costs past what a stored string
// may make the program spend are unsupported.
namespace sgcl::crypto::detail {
    inline constexpr size_t argon2_phc_max_salt = 64;
    inline constexpr size_t argon2_phc_max_hash = 64;
    inline constexpr uint32_t argon2_phc_max_memory = 4u * 1024 * 1024;   // KiB: 4 GiB
    inline constexpr uint32_t argon2_phc_max_iterations = 1u << 16;
    inline constexpr uint32_t argon2_phc_max_parallelism = 255;

    struct Argon2Phc {
        uint32_t variant;
        uint32_t memory;
        uint32_t iterations;
        uint32_t parallelism;
        unsigned char salt[argon2_phc_max_salt];
        size_t salt_size;
        unsigned char hash[argon2_phc_max_hash];
        size_t hash_size;
    };

    inline const char* const argon2_names[3] = {"argon2d", "argon2i", "argon2id"};

    // A cursor over the string: each step takes what it expects or fails
    struct PhcReader {
        const char* p;
        const char* end;

        SGCL_INLINE_HOT bool take(std::string_view s) noexcept {
            if (size_t(end - p) < s.size() || std::string_view(p, s.size()) != s) {
                return false;
            }
            p += s.size();
            return true;
        }

        // a decimal of up to ten digits that fits 32 bits, no leading zero
        bool number(uint32_t& out) noexcept {
            const char* start = p;
            uint64_t v = 0;
            while (p != end && *p >= '0' && *p <= '9' && p - start < 11) {
                v = v * 10 + uint64_t(*p - '0');
                ++p;
            }
            size_t digits = size_t(p - start);
            if (digits == 0 || digits > 10 || (digits > 1 && *start == '0') || v > 0xffffffffull) {
                return false;
            }
            out = uint32_t(v);
            return true;
        }

        // the base64 up to the next '$' (or the end), decoded into out
        bool base64(unsigned char* out, size_t max, size_t& size, bool& too_long) noexcept {
            const char* start = p;
            while (p != end && *p != '$') {
                ++p;
            }
            size_t chars = size_t(p - start);
            too_long = encoding::base64::raw_standard.max_decoded_size(chars) > max + 2;
            if (too_long) {
                return false;
            }
            unsigned char buf[argon2_phc_max_hash + 3];
            auto n = encoding::base64::raw_standard.decode_to(slice<byte>(reinterpret_cast<byte*>(buf), sizeof buf),
                                                             slice<const char>(start, chars));
            if (!n) {
                return false;
            }
            if (*n > max) {
                too_long = true;
                return false;
            }
            copy_bytes(out, buf, *n);
            size = *n;
            secure_zero(buf, sizeof buf);
            return true;
        }
    };

    inline expected<void, error> argon2_phc_parse(const slice<const char>& text, Argon2Phc& out) noexcept {
        auto malformed = [] {
            return unexpected(error(errc::malformed, "sgcl::crypto::argon2: not a PHC string of Argon2"));
        };
        auto unsupported = [](const char* what) {
            return unexpected(error(errc::unsupported, string(what)));
        };
        PhcReader r{text.data(), text.data() + text.size()};
        if (!r.take("$")) {
            return malformed();
        }
        // argon2id before argon2i: the longer name first
        if (r.take("argon2id$")) {
            out.variant = 2;
        } else if (r.take("argon2i$")) {
            out.variant = 1;
        } else if (r.take("argon2d$")) {
            out.variant = 0;
        } else {
            return malformed();
        }
        uint32_t version = 0x10;   // a string without v= is of version 1.0
        if (r.take("v=")) {
            if (!r.number(version) || !r.take("$")) {
                return malformed();
            }
        }
        if (version != 19) {
            return unsupported("sgcl::crypto::argon2: a version other than 19 (0x13)");
        }
        if (!r.take("m=") || !r.number(out.memory) || !r.take(",t=") || !r.number(out.iterations) || !r.take(",p=")
            || !r.number(out.parallelism) || !r.take("$")) {
            return malformed();
        }
        bool too_long = false;
        if (!r.base64(out.salt, argon2_phc_max_salt, out.salt_size, too_long)) {
            return too_long ? unsupported("sgcl::crypto::argon2: a salt longer than 64 bytes") : malformed();
        }
        if (!r.take("$") || !r.base64(out.hash, argon2_phc_max_hash, out.hash_size, too_long) || r.p != r.end) {
            return too_long ? unsupported("sgcl::crypto::argon2: a hash longer than 64 bytes") : malformed();
        }
        if (out.salt_size < 8 || out.hash_size < 4 || out.iterations == 0 || out.parallelism == 0
            || uint64_t(out.memory) < 8ull * out.parallelism) {
            return malformed();
        }
        if (out.memory > argon2_phc_max_memory || out.iterations > argon2_phc_max_iterations
            || out.parallelism > argon2_phc_max_parallelism) {
            return unsupported("sgcl::crypto::argon2: costs past what a stored hash may ask (4 GiB, 2^16 passes, 255 lanes)");
        }
        return {};
    }

    SGCL_INLINE_HOT size_t phc_put(char* at, std::string_view s) noexcept {
        copy_bytes(at, s.data(), s.size());
        return s.size();
    }

    inline size_t phc_put_number(char* at, uint32_t v) noexcept {
        char digits[10];
        size_t n = 0;
        do {
            digits[n++] = char('0' + v % 10);
            v /= 10;
        } while (v != 0);
        for (size_t i = 0; i < n; ++i) {
            at[i] = digits[n - 1 - i];
        }
        return n;
    }

    inline string argon2_phc_format(uint32_t variant, uint32_t memory, uint32_t iterations, uint32_t parallelism,
                                    const unsigned char* salt, size_t salt_size, const unsigned char* hash, size_t hash_size) {
        char buf[256];
        size_t n = 0;
        n += phc_put(buf + n, "$");
        n += phc_put(buf + n, argon2_names[variant]);
        n += phc_put(buf + n, "$v=19$m=");
        n += phc_put_number(buf + n, memory);
        n += phc_put(buf + n, ",t=");
        n += phc_put_number(buf + n, iterations);
        n += phc_put(buf + n, ",p=");
        n += phc_put_number(buf + n, parallelism);
        n += phc_put(buf + n, "$");
        n += encoding::base64::raw_standard.encode_to(slice<char>(buf + n, sizeof buf - n),
                                                      slice<const byte>(reinterpret_cast<const byte*>(salt), salt_size));
        n += phc_put(buf + n, "$");
        n += encoding::base64::raw_standard.encode_to(slice<char>(buf + n, sizeof buf - n),
                                                      slice<const byte>(reinterpret_cast<const byte*>(hash), hash_size));
        return string(buf, n);
    }
}
