//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../core/aliases.h"
#include "../core/array.h"
#include "../core/expected.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../math/random.h"
#include "../time/datetime.h"
#include "../time/zone.h"
#include "../txt/format.h"

#include <atomic>
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace sgcl::encoding {
    namespace detail {
        using namespace sgcl::detail;

        // The value of every byte as a hex digit, 16 for one that is not
        struct UuidNibbles {
            uint8_t value[256] = {};

            constexpr UuidNibbles() noexcept {
                for (int c = 0; c < 256; ++c) {
                    value[c] = c >= '0' && c <= '9' ? uint8_t(c - '0')
                             : c >= 'a' && c <= 'f' ? uint8_t(c - 'a' + 10)
                             : c >= 'A' && c <= 'F' ? uint8_t(c - 'A' + 10)
                                                    : uint8_t(16);
                }
            }
        };

        inline constexpr UuidNibbles uuid_nibbles{};

        SGCL_INLINE_HOT constexpr uint8_t uuid_nibble(char c) noexcept {
            return uuid_nibbles.value[uint8_t(c)];
        }

        // The sixteen bytes of a text in one of the forms parse takes; 0 when
        // it is one, else the offset of the byte that broke it plus 1, and in
        // hex_fault whether that byte is a digit's place (invalid_character)
        // or the form's (syntax)
        constexpr size_t uuid_parse(std::string_view t, uint8_t (&out)[16], bool& hex_fault) noexcept {
            hex_fault = false;
            size_t base = 0;
            if (t.size() == 45 && (t.substr(0, 9) == "urn:uuid:" || t.substr(0, 9) == "URN:UUID:")) {
                base = 9;
            } else if (t.size() == 38) {
                if (t[0] != '{') {
                    return 1;
                }
                if (t[37] != '}') {
                    return 38;
                }
                base = 1;
            } else if (t.size() == 32) {
                uint8_t fault = 0;
                for (size_t i = 0; i < 16; ++i) {
                    uint8_t h = uuid_nibble(t[2 * i]), l = uuid_nibble(t[2 * i + 1]);
                    fault |= h | l;
                    out[i] = uint8_t(h << 4 | l);
                }
                if (fault & 16) {
                    hex_fault = true;
                    for (size_t i = 0; i < 32; ++i) {
                        if (uuid_nibble(t[i]) & 16) {
                            return i + 1;
                        }
                    }
                }
                return 0;
            } else if (t.size() != 36) {
                return t.size() + 1;
            }
            if (t[base + 8] != '-') {
                return base + 9;
            }
            if (t[base + 13] != '-') {
                return base + 14;
            }
            if (t[base + 18] != '-') {
                return base + 19;
            }
            if (t[base + 23] != '-') {
                return base + 24;
            }
            // the 32 digits read without a branch, their faults ORed; the
            // place of a fault found only when there is one
            constexpr uint8_t place[16] = {0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34};
            uint8_t fault = 0;
            for (size_t i = 0; i < 16; ++i) {
                uint8_t h = uuid_nibble(t[base + place[i]]), l = uuid_nibble(t[base + place[i] + 1]);
                fault |= h | l;
                out[i] = uint8_t(h << 4 | l);
            }
            if (fault & 16) {
                hex_fault = true;
                for (size_t i = 0; i < 16; ++i) {
                    size_t at = base + place[i];
                    if (uuid_nibble(t[at]) & 16) {
                        return at + 1;
                    }
                    if (uuid_nibble(t[at + 1]) & 16) {
                        return at + 2;
                    }
                }
            }
            return 0;
        }

        // The thread's stream of random words, keyed from the system and
        // keyed again in the child of a fork, so that a parent and its child
        // never draw the same UUIDs
        inline uint64_t uuid_random() noexcept {
            struct Source {
                math::random stream;
                uint64_t generation = math::detail::fork_generation.load(std::memory_order_relaxed);
            };
            thread_local Source source;
            uint64_t now = math::detail::fork_generation.load(std::memory_order_relaxed);
            if (source.generation != now) {
                source.stream = math::random();
                source.generation = now;
            }
            return source.stream.next_uint64();
        }

        // The last v7's millisecond and counter (48 + 12 bits) of the
        // process: every v7 takes a greater pair than the one before
        inline std::atomic<uint64_t> uuid_v7_last{0};
    }

    // A UUID of RFC 9562: sixteen bytes, a plain value with no pointer (a
    // constant or a global may hold one), written 8-4-4-4-12 in lower-case
    // hexadecimal. v4 and v7 are made here; every version is read.
    class uuid {
    public:
        using error = encoding::error;

        // The variant field (RFC 9562 §4.1): the layout the rest follows
        enum class variant_kind : uint8_t {
            ncs,         // 0xxx: NCS, before RFC 4122
            rfc9562,     // 10xx: the layout of RFC 9562 (and 4122)
            microsoft,   // 110x: Microsoft's GUIDs of old
            future       // 111x: reserved
        };

        // The nil UUID, all zeros
        constexpr uuid() noexcept = default;

        // A literal: one that is not a UUID is an error of the compiler
        template<size_t N>
        explicit consteval uuid(const char (&text)[N]) {
            bool hex = false;
            if (detail::uuid_parse(std::string_view(text, N - 1), _bytes, hex) != 0) {
                throw "not a UUID";
            }
        }

        // parse's value, or bad_expected_access<encoding::error>
        explicit uuid(const string& text)
        : uuid(parse(text).value()) {
        }

        // Of its sixteen bytes, as they are written
        explicit uuid(const array<uint8_t, 16>& bytes) noexcept {
            for (size_t i = 0; i < 16; ++i) {
                _bytes[i] = bytes[i];
            }
        }

        // The forms Go's google/uuid and Python read: 8-4-4-4-12, the same
        // in braces or after urn:uuid:, and 32 hex digits; either case
        static expected<uuid, error> parse(const string& text) noexcept {
            uuid u;
            bool hex = false;
            size_t at = detail::uuid_parse(text.view(), u._bytes, hex);
            if (at == 0) {
                return u;
            }
            if (hex) {
                return unexpected<error>(error(errc::invalid_character, at - 1, string("invalid character ") + detail::quoted_byte(uint8_t(text[at - 1])) + " in a UUID"));
            }
            if (at > text.size()) {
                return unexpected<error>(error(errc::syntax, text.size(), string("not a UUID: its length is ") + string(std::to_string(text.size()))));
            }
            return unexpected<error>(error(errc::syntax, at - 1, string("not a UUID: ") + detail::quoted_byte(uint8_t(text[at - 1])) + " out of place"));
        }

        // Of sixteen bytes; errc::syntax for another count
        static expected<uuid, error> from_bytes(const slice<const byte>& bytes) noexcept {
            if (bytes.size() != 16) {
                return unexpected<error>(error(errc::syntax, bytes.size() < 16 ? bytes.size() : 16, string("not a UUID: ") + string(std::to_string(bytes.size())) + " bytes, not 16"));
            }
            uuid u;
            std::memcpy(u._bytes, bytes.data(), 16);
            return u;
        }

        // Version 4: 122 random bits
        static uuid v4() noexcept {
            uuid u;
            uint64_t a = detail::uuid_random(), b = detail::uuid_random();
            u._put(a, b);
            u._bytes[6] = uint8_t((u._bytes[6] & 0x0F) | 0x40);
            u._bytes[8] = uint8_t((u._bytes[8] & 0x3F) | 0x80);
            return u;
        }

        // Version 7: the Unix time in milliseconds, then a counter of 12
        // bits and 62 random bits. Every v7 of the process is greater than
        // the one before, on any thread, even when the clock steps back: a
        // millisecond not past the last one's keeps the last one's and its
        // counter plus one, a counter full carries into the millisecond (RFC
        // 9562 §6.2, method 1); a new millisecond starts its counter at a
        // random value below 2048, which leaves room to count
        static uuid v7() noexcept {
            auto ms = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
            uint64_t r = detail::uuid_random();
            uint64_t now = (ms & 0xFFFFFFFFFFFFull) << 12 | (r & 0x7FF);
            uint64_t prev = detail::uuid_v7_last.load(std::memory_order_relaxed);
            uint64_t next;
            do {
                next = (now >> 12) > (prev >> 12) ? now : prev + 1;
            } while (!detail::uuid_v7_last.compare_exchange_weak(prev, next, std::memory_order_relaxed));
            uuid u;
            uint64_t high = (next >> 12) << 16 | 0x7000 | (next & 0xFFF);
            u._put(high, detail::uuid_random());
            u._bytes[8] = uint8_t((u._bytes[8] & 0x3F) | 0x80);
            return u;
        }

        static constexpr uuid nil() noexcept {
            return uuid();
        }

        // ffffffff-ffff-ffff-ffff-ffffffffffff (RFC 9562 §5.10)
        static constexpr uuid max() noexcept {
            uuid u;
            for (auto& b : u._bytes) {
                b = 0xFF;
            }
            return u;
        }

        // The sixteen bytes as they are written, as net::ip_address gives its own
        SGCL_INLINE_HOT array<uint8_t, 16> bytes() const noexcept {
            array<uint8_t, 16> out;
            for (size_t i = 0; i < 16; ++i) {
                out[i] = _bytes[i];
            }
            return out;
        }

        // The version field as written, 0 to 15
        SGCL_INLINE_HOT constexpr int version() const noexcept {
            return _bytes[6] >> 4;
        }

        SGCL_INLINE_HOT constexpr variant_kind variant() const noexcept {
            uint8_t b = _bytes[8];
            return b < 0x80 ? variant_kind::ncs : b < 0xC0 ? variant_kind::rfc9562 : b < 0xE0 ? variant_kind::microsoft : variant_kind::future;
        }

        // The instant of a version 1, 6 or 7 of the RFC's variant: 100 ns
        // since 1582-10-15 for 1 and 6, milliseconds since 1970 for 7; in
        // UTC. nullopt for every other
        optional<time::datetime> timestamp() const noexcept {
            if (variant() != variant_kind::rfc9562) {
                return nullopt;
            }
            uint64_t t;
            switch (version()) {
                case 1:
                    t = uint64_t(_word(0, 4)) | uint64_t(_word(4, 2)) << 32 | uint64_t(_word(6, 2) & 0x0FFF) << 48;
                    break;
                case 6:
                    t = uint64_t(_word(0, 4)) << 28 | uint64_t(_word(4, 2)) << 12 | (_word(6, 2) & 0x0FFF);
                    break;
                case 7:
                    return time::datetime::from_unix_milli(int64_t(uint64_t(_word(0, 4)) << 16 | _word(4, 2)), time::zone::utc());
                default:
                    return nullopt;
            }
            // 100 ns intervals since 1582-10-15T00:00:00Z: the days to 1970
            // are 141427
            int64_t ticks = int64_t(t) - int64_t(141427) * 86400 * 10000000;
            int64_t seconds = ticks >= 0 ? ticks / 10000000 : -((-ticks + 9999999) / 10000000);
            int64_t rest = ticks - seconds * 10000000;
            if (seconds > -9223372036 && seconds < 9223372036) {
                return time::datetime::from_unix_nano(seconds * 1000000000 + rest * 100, time::zone::utc());
            }
            return time::datetime::from_unix(seconds, time::zone::utc());
        }

        SGCL_INLINE_HOT constexpr bool is_nil() const noexcept {
            for (auto b : _bytes) {
                if (b) {
                    return false;
                }
            }
            return true;
        }

        // 8-4-4-4-12, lower case
        string to_string() const noexcept {
            return detail::StringAccess::filled<string>(36, [&](char* out) {
                _write(out);
            });
        }

        SGCL_INLINE_HOT friend constexpr bool operator==(const uuid& a, const uuid& b) noexcept {
            for (size_t i = 0; i < 16; ++i) {
                if (a._bytes[i] != b._bytes[i]) {
                    return false;
                }
            }
            return true;
        }

        // The order of the bytes, the order RFC 9562 §6.11 compares by: v7s
        // by their time
        SGCL_INLINE_HOT friend constexpr std::strong_ordering operator<=>(const uuid& a, const uuid& b) noexcept {
            for (size_t i = 0; i < 16; ++i) {
                if (a._bytes[i] != b._bytes[i]) {
                    return a._bytes[i] <=> b._bytes[i];
                }
            }
            return std::strong_ordering::equal;
        }

        SGCL_INLINE_HOT size_t hash() const noexcept {
            uint64_t a, b;
            std::memcpy(&a, _bytes, 8);
            std::memcpy(&b, _bytes + 8, 8);
            uint64_t h = (a ^ 0x9e3779b97f4a7c15ull) * 0xff51afd7ed558ccdull;
            h = (h ^ (h >> 32) ^ b) * 0xc4ceb9fe1a85ec53ull;
            return size_t(h ^ (h >> 29));
        }

    private:
        friend void format_value(txt::format_sink& out, const uuid& id, const txt::format_spec& spec) noexcept;

        uint8_t _bytes[16] = {};

        SGCL_INLINE_HOT void _put(uint64_t high, uint64_t low) noexcept {
            for (int i = 0; i < 8; ++i) {
                _bytes[i] = uint8_t(high >> (56 - 8 * i));
                _bytes[8 + i] = uint8_t(low >> (56 - 8 * i));
            }
        }

        SGCL_INLINE_HOT uint32_t _word(size_t at, size_t n) const noexcept {
            uint32_t v = 0;
            for (size_t i = 0; i < n; ++i) {
                v = v << 8 | _bytes[at + i];
            }
            return v;
        }

        SGCL_INLINE_HOT void _write(char* out) const noexcept {
            static constexpr char digits[] = "0123456789abcdef";
            for (size_t i = 0; i < 16; ++i) {
                if (i == 4 || i == 6 || i == 8 || i == 10) {
                    *out++ = '-';
                }
                *out++ = digits[_bytes[i] >> 4];
                *out++ = digits[_bytes[i] & 15];
            }
        }
    };

    static_assert(sizeof(uuid) == 16);

    inline void format_value(txt::format_sink& out, const uuid& id, const txt::format_spec& spec) noexcept {
        char text[36];
        id._write(text);
        txt::write_padded(out, std::string_view(text, 36), spec);
    }
}

// Which specifications a uuid takes: none but the width, the fill and the
// alignment; the writing is format_value's, above
template<>
struct sgcl::txt::formatter<sgcl::encoding::uuid> {
    SGCL_INLINE_HOT static constexpr bool takes(char type) noexcept {
        return !type;
    }

    SGCL_INLINE_HOT static constexpr bool takes_precision() noexcept {
        return false;
    }
};

template<>
struct std::hash<sgcl::encoding::uuid> {
    SGCL_INLINE_HOT size_t operator()(const sgcl::encoding::uuid& id) const noexcept {
        return id.hash();
    }
};
