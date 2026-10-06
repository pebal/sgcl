//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// TIFF's own compressions of a strip or a tile (TIFF 6.0, section 9 and
// 13): PackBits and LZW as TIFF has it. TIFF's LZW is compress's LZW with
// the codes most significant bit first, a clear code (256) at the start of
// each strip, and the "early change": the width grows one code before the
// table needs it (after entry 510 is made, codes are 10 bits; after 1022,
// 11; after 2046, 12), which compress's decoder does not take (lzw.h).
// LibTIFF and every writer since 1990 write it so.
namespace sgcl::codec::detail::tiff_codec {
    // PackBits: a byte n then 0..127: n + 1 literal bytes; -127..-1: the
    // next byte 1 - n times; -128: nothing. Decoded into out[0..cap);
    // the bytes written
    inline size_t unpackbits(const uint8_t* in, size_t n, uint8_t* out, size_t cap) noexcept {
        size_t i = 0, o = 0;
        while (i < n && o < cap) {
            const int c = int8_t(in[i++]);
            if (c >= 0) {
                const size_t k = std::min({size_t(c) + 1, n - i, cap - o});
                sgcl::detail::copy_bytes(out + o, in + i, k);
                i += size_t(c) + 1;
                o += k;
            } else if (c != -128) {
                if (i >= n) {
                    break;
                }
                const size_t k = std::min(size_t(1 - c), cap - o);
                sgcl::detail::fill_bytes(out + o, in[i++], k);
                o += k;
            }
        }
        return o;
    }

    // PackBits of a row (encoded rows end where they end, as TIFF wants)
    inline void packbits(const uint8_t* in, size_t n, std::vector<uint8_t>& out) {
        size_t i = 0;
        while (i < n) {
            size_t run = 1;
            while (i + run < n && run < 128 && in[i + run] == in[i]) {
                ++run;
            }
            if (run >= 2) {
                out.push_back(uint8_t(int8_t(1 - int(run))));
                out.push_back(in[i]);
                i += run;
                continue;
            }
            size_t lit = 1;
            while (i + lit < n && lit < 128 && !(i + lit + 1 < n && in[i + lit] == in[i + lit + 1])) {
                ++lit;
            }
            out.push_back(uint8_t(lit - 1));
            out.insert(out.end(), in + i, in + i + lit);
            i += lit;
        }
    }

    // The width of the code after the table's next free entry `next`
    // (early change: one entry before the power of two)
    SGCL_INLINE_HOT unsigned lzw_width(unsigned next) noexcept {
        return next < 511 ? 9 : next < 1023 ? 10 : next < 2047 ? 11 : 12;
    }

    // TIFF's LZW decoded into out[0..cap): the bytes written; ok false for
    // a code past the table (the bytes before it written). A strip that
    // ends without its end code is taken (libtiff and Go take it); old
    // files of the bit order reversed (LSB first, no early change) are
    // not read.
    //
    // The output of a strip is one run, so a code's string is never built
    // from the table: each entry is where its string already lies in the
    // output and how long it is (a new entry is the string before and the
    // first byte of this one, which is the string before followed by its
    // next byte where it was written), copied from there.
    inline size_t unlzw(const uint8_t* in, size_t n, uint8_t* out, size_t cap, bool& ok) noexcept {
        ok = true;
        struct Entry {
            uint32_t at;       // where the string lies in out
            uint32_t length;
        };
        std::unique_ptr<Entry[]> table(new Entry[4096]);
        unsigned next = 258, width = 9;
        int prev = -1;
        uint32_t prev_at = 0, prev_length = 0;
        uint64_t bits = 0;
        unsigned count = 0;
        size_t i = 0, o = 0;
        for (;;) {
            if (count < width) {
                // refill whole bytes: up to 56 bits held
                while (count <= 56 && i < n) {
                    bits = bits << 8 | in[i++];
                    count += 8;
                }
                if (count < width) {
                    return o;
                }
            }
            const unsigned code = unsigned(bits >> (count - width)) & ((1u << width) - 1);
            count -= width;
            if (code == 256) {
                next = 258;
                width = 9;
                prev = -1;
                continue;
            }
            if (code == 257) {
                return o;
            }
            const uint32_t at = uint32_t(o);
            uint32_t length;
            if (code < 256) {
                out[o] = uint8_t(code);
                length = 1;
            } else if (code < next) {
                const Entry e = table[code];
                length = e.length;
                const size_t k = std::min<size_t>(length, cap - o);
                // the string lies wholly before o: no overlap
                sgcl::detail::copy_bytes(out + o, out + e.at, k);
            } else if (code == next && prev >= 0) {
                // the string before and its own first byte
                length = prev_length + 1;
                const size_t k = std::min<size_t>(prev_length, cap - o);
                sgcl::detail::copy_bytes(out + o, out + prev_at, k);
                if (o + prev_length < cap) {
                    out[o + prev_length] = out[prev_at];
                }
            } else {
                ok = false;
                return o;
            }
            if (prev >= 0 && next < 4096) {
                table[next] = Entry{prev_at, prev_length + 1};
                ++next;
            }
            o = std::min<size_t>(o + length, cap);
            if (o >= cap) {
                return o;
            }
            prev = int(code);
            prev_at = at;
            prev_length = length;
            width = lzw_width(next);
        }
    }

    // TIFF's LZW of a strip: a clear code first, the end code last; a clear
    // code where the next entry would be 4094, so that no code passes 12
    // bits
    class LzwWriter {
    public:
        LzwWriter() : _slots(new uint32_t[Slots]) {
            _clear_table();
        }

        void write(const uint8_t* p, size_t n, std::vector<uint8_t>& out) {
            if (!_started) {
                _started = true;
                _put(256, out);
            }
            for (size_t i = 0; i < n; ++i) {
                const uint32_t b = p[i];
                if (_current < 0) {
                    _current = int(b);
                    continue;
                }
                const uint32_t key = uint32_t(_current) << 8 | b;
                uint32_t s = (key ^ key >> 8) & (Slots - 1);
                uint32_t f;
                while ((f = _slots[s]) != 0 && (f >> 12) != key) {
                    s = (s + 1) & (Slots - 1);
                }
                if (f) {
                    _current = int(f & 4095);
                    continue;
                }
                _put(uint32_t(_current), out);
                _slots[s] = key << 12 | _next;
                ++_next;
                if (_next >= 4094) {
                    // the table full: a clear code at the width the decoder
                    // reads it with, then the table anew
                    _put(256, out);
                    _clear_table();
                }
                _current = int(b);
            }
        }

        void finish(std::vector<uint8_t>& out) {
            if (!_started) {
                _started = true;
                _put(256, out);
            }
            if (_current >= 0) {
                _put(uint32_t(_current), out);
                ++_next;   // the decoder makes an entry on this code too
                _current = -1;
            }
            _put(257, out);
            if (_count) {
                out.push_back(uint8_t(_bits << (8 - _count)));
            }
            _bits = 0;
            _count = 0;
        }

    private:
        static constexpr uint32_t Slots = 16384;

        void _clear_table() noexcept {
            std::memset(_slots.get(), 0, Slots * sizeof(uint32_t));
            _next = 258;
        }

        // A code at the width the decoder reads it with: the decoder has
        // made one entry fewer than the encoder when it reads a code (it
        // makes each one a code later)
        void _put(uint32_t code, std::vector<uint8_t>& out) {
            const unsigned width = lzw_width(_next == 258 ? 258 : _next - 1);
            _bits = _bits << width | code;
            _count += width;
            while (_count >= 8) {
                _count -= 8;
                out.push_back(uint8_t(_bits >> _count));
            }
        }

        std::unique_ptr<uint32_t[]> _slots;
        uint32_t _next = 258;
        int _current = -1;
        bool _started = false;
        uint64_t _bits = 0;
        unsigned _count = 0;
    };
}
