//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/bytes.h"
#include "detail/wide.h"
#include "mixin/hasher.h"

#include <cstddef>
#include <cstdint>

// SipHash-2-4 (Aumasson and Bernstein, "SipHash: a fast short-input PRF",
// 2012): a keyed hash with an argument behind it. With a secret key of 128
// bits, its values are as good as random to whoever does not have the key,
// even one who picks the inputs and sees the outputs — the one hash of the
// module for keys from an adversary (Python, Rust and Perl hash their
// tables' strings with it). It pays for that: several times the cost
// of maphash on a short key, and more on a long one. The key is
// mandatory; a program makes it once from a source of randomness and keeps
// it secret.
//
// The state is four words, the key (for reset) and the bytes of a word not
// yet complete. A message goes in as little-endian words, each through two
// rounds (the "2"); the last word carries the length modulo 256 in its top
// byte; four rounds (the "4") finish. The value is the 64-bit result; its
// digest is, as every digest of the module, big-endian — the reference
// implementation writes the same number little-endian.
namespace sgcl::hash {
    namespace detail {
        struct Sip {
            uint64_t v0, v1, v2, v3;

            Sip(uint64_t k0, uint64_t k1) noexcept
            : v0(k0 ^ 0x736f6d6570736575ull)   // "somepseudorandomlygeneratedbytes"
            , v1(k1 ^ 0x646f72616e646f6dull)
            , v2(k0 ^ 0x6c7967656e657261ull)
            , v3(k1 ^ 0x7465646279746573ull) {
            }

            void round() noexcept {
                v0 += v1;
                v1 = rotate_left(v1, 13);
                v1 ^= v0;
                v0 = rotate_left(v0, 32);
                v2 += v3;
                v3 = rotate_left(v3, 16);
                v3 ^= v2;
                v0 += v3;
                v3 = rotate_left(v3, 21);
                v3 ^= v0;
                v2 += v1;
                v1 = rotate_left(v1, 17);
                v1 ^= v2;
                v2 = rotate_left(v2, 32);
            }

            void word(uint64_t m) noexcept {
                v3 ^= m;
                round();
                round();
                v0 ^= m;
            }

            // The last word (the bytes after the whole words, the length
            // in its top byte), then the finish
            uint64_t finish(uint64_t last) noexcept {
                word(last);
                v2 ^= 0xff;
                round();
                round();
                round();
                round();
                return v0 ^ v1 ^ v2 ^ v3;
            }
        };

        // Up to seven bytes as the low bytes of a little-endian word
        inline uint64_t sip_tail(const unsigned char* p, size_t n) noexcept {
            uint64_t w = 0;
            for (size_t i = 0; i < n; ++i) {
                w |= uint64_t(p[i]) << (8 * i);
            }
            return w;
        }

        inline uint64_t siphash(const unsigned char* p, size_t n, uint64_t k0, uint64_t k1) noexcept {
            Sip s(k0, k1);
            const size_t whole = n & ~size_t(7);
            for (size_t i = 0; i < whole; i += 8) {
                s.word(load_le64(p + i));
            }
            return s.finish(uint64_t(n) << 56 | sip_tail(p + whole, n - whole));
        }
    }

    class siphash : public mixin::hasher<siphash> {
        friend class mixin::hasher<siphash>;

    public:
        using hasher::update;

        static constexpr size_t digest_size = 8;
        static constexpr size_t block_size = 8;

        // The key, 16 bytes, read as two little-endian words as the
        // reference implementation reads it
        explicit siphash(const array<byte, 16>& key) noexcept
        : _k0(detail::load_le64(detail::bytes(key.data())))
        , _k1(detail::load_le64(detail::bytes(key.data() + 8)))
        , _state(_k0, _k1) {
        }

        void update(const slice<const byte>& data) noexcept {
            const unsigned char* p = detail::bytes(data.data());
            size_t n = data.size();
            _length += n;
            if (_filled > 0) {   // complete the word begun earlier
                while (n > 0 && _filled < 8) {
                    _tail |= uint64_t(*p++) << (8 * _filled++);
                    --n;
                }
                if (_filled < 8) {
                    return;
                }
                _state.word(_tail);   // _tail and _filled are set below, from what is left
            }
            const size_t whole = n & ~size_t(7);
            for (size_t i = 0; i < whole; i += 8) {
                _state.word(detail::load_le64(p + i));
            }
            _tail = detail::sip_tail(p + whole, n - whole);
            _filled = unsigned(n - whole);
        }

        uint64_t value() const noexcept {
            detail::Sip s = _state;
            return s.finish(_length << 56 | _tail);
        }

        array<byte, 8> digest() const noexcept {
            return detail::big_endian<8>(value());
        }

        // As new, with the same key
        void reset() noexcept {
            _state = detail::Sip(_k0, _k1);
            _length = 0;
            _tail = 0;
            _filled = 0;
        }

    private:
        uint64_t _k0;
        uint64_t _k1;
        detail::Sip _state;
        uint64_t _length = 0;   // bytes taken in; its low byte goes into the last word
        uint64_t _tail = 0;     // the bytes of the word not yet complete
        unsigned _filled = 0;   // how many

        // the one-shot form of() calls: `siphash::of(data, key)`
        static uint64_t _of(const slice<const byte>& data, const array<byte, 16>& key) noexcept {
            return detail::siphash(detail::bytes(data.data()), data.size(), detail::load_le64(detail::bytes(key.data())),
                                   detail::load_le64(detail::bytes(key.data() + 8)));
        }
    };
}
