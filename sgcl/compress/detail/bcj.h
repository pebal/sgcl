//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/bytes.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sgcl::compress::detail {
    // The filters that go before LZMA2 in .xz and 7z: the branch
    // converters (BCJ) of eight processors and Delta. A branch converter
    // finds the calls and jumps of machine code and turns their relative
    // targets into absolute ones when encoding (and back when decoding):
    // calls to one function from many places then look alike and compress
    // better. Delta codes each byte as its difference from the byte
    // `distance` before it (sound, images of fixed-size samples). Each
    // works in place and keeps the length; a converter works on whole
    // instructions, so it converts a prefix of the bytes it is given and
    // leaves the last few (fewer than an instruction) for the next call —
    // at the end of the data they stay as they are. The instruction
    // layouts and conditions are the formats' own (xz's and 7-Zip's
    // filters, which the data must match bit for bit); the code is this
    // library's.
    enum class SimpleKind : uint8_t {
        x86,
        powerpc,
        ia64,
        arm,
        armt,
        sparc,
        arm64,
        riscv,
        delta
    };

    class SimpleFilter {
    public:
        // start: the position of the first byte (the filter's start offset);
        // distance: Delta's, 1..256
        void init(SimpleKind kind, bool encoder, uint32_t start = 0, uint32_t distance = 1) noexcept {
            _kind = kind;
            _encoder = encoder;
            _pos = start;
            _prev_mask = 0;
            _prev_pos = start - 5;
            _distance = distance;
            _delta_at = 0;
            std::memset(_history, 0, sizeof(_history));
        }

        SGCL_INLINE_HOT SimpleKind kind() const noexcept {
            return _kind;
        }

        // The alignment a start offset must have, as the formats require
        static uint32_t alignment(SimpleKind k) noexcept {
            switch (k) {
                case SimpleKind::powerpc:
                case SimpleKind::arm:
                case SimpleKind::sparc:
                case SimpleKind::arm64:
                    return 4;
                case SimpleKind::armt:
                case SimpleKind::riscv:
                    return 2;
                case SimpleKind::ia64:
                    return 16;
                default:
                    return 1;
            }
        }

        // Converts the prefix of p[0, n) it can; its length
        size_t run(uint8_t* p, size_t n) noexcept {
            size_t done = 0;
            switch (_kind) {
                case SimpleKind::x86: done = _x86(p, n); break;
                case SimpleKind::powerpc: done = _powerpc(p, n); break;
                case SimpleKind::ia64: done = _ia64(p, n); break;
                case SimpleKind::arm: done = _arm(p, n); break;
                case SimpleKind::armt: done = _armt(p, n); break;
                case SimpleKind::sparc: done = _sparc(p, n); break;
                case SimpleKind::arm64: done = _arm64(p, n); break;
                case SimpleKind::riscv: done = _encoder ? _riscv_encode(p, n) : _riscv_decode(p, n); break;
                case SimpleKind::delta: done = _delta(p, n); break;
            }
            _pos += uint32_t(done);
            return done;
        }

    private:
        SGCL_INLINE_HOT static uint32_t _le32(const uint8_t* p) noexcept {
            uint32_t v;
            std::memcpy(&v, p, 4);
            return v;
        }

        SGCL_INLINE_HOT static void _put_le32(uint8_t* p, uint32_t v) noexcept {
            std::memcpy(p, &v, 4);
        }

        SGCL_INLINE_HOT static uint32_t _be32(const uint8_t* p) noexcept {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        SGCL_INLINE_HOT static void _put_be32(uint8_t* p, uint32_t v) noexcept {
            p[0] = uint8_t(v >> 24);
            p[1] = uint8_t(v >> 16);
            p[2] = uint8_t(v >> 8);
            p[3] = uint8_t(v);
        }

        // An address `rel` bytes on from pc, or back: encoding adds the
        // position, decoding takes it away
        SGCL_INLINE_HOT uint32_t _shift(uint32_t v, uint32_t pc) const noexcept {
            return _encoder ? v + pc : v - pc;
        }

        // x86: E8 (call) and E9 (jmp) with a 32-bit operand whose top byte
        // is 00 or FF (a near target). A mask of the E8/E9 bytes seen in
        // the last few positions keeps an opcode byte inside another
        // instruction's operand from being taken; the mask and the last
        // opcode's position go on from one call to the next.
        size_t _x86(uint8_t* p, size_t n) noexcept {
            static constexpr bool allowed[8] = {true, true, true, false, true, false, false, false};
            static constexpr uint32_t byte_of[8] = {0, 1, 2, 2, 3, 3, 3, 3};
            if (n < 5) {
                return 0;
            }
            auto near = [](uint8_t b) noexcept {
                return b == 0 || b == 0xFF;
            };
            uint32_t mask = _prev_mask;
            uint32_t prev = _prev_pos;
            if (_pos - prev > 5) {
                prev = _pos - 5;
            }
            const size_t last = n - 5;
            size_t i = 0;
            while (i <= last) {
                // the search for an opcode, which most bytes are not
                while ((p[i] & 0xFE) != 0xE8) {
                    if (++i > last) {
                        goto out;
                    }
                }
                {
                    uint32_t here = _pos + uint32_t(i);
                    uint32_t gap = here - prev;
                    prev = here;
                    if (gap > 5) {
                        mask = 0;
                    } else {
                        for (uint32_t k = 0; k < gap; ++k) {
                            mask = (mask & 0x77) << 1;
                        }
                    }
                    uint8_t top = p[i + 4];
                    if (near(top) && allowed[(mask >> 1) & 7] && (mask >> 1) < 0x10) {
                        uint32_t src = _le32(p + i + 1);
                        uint32_t dest;
                        for (;;) {
                            dest = _shift(src, here + 5);
                            if (mask == 0) {
                                break;
                            }
                            uint32_t k = byte_of[mask >> 1];
                            if (!near(uint8_t(dest >> (24 - k * 8)))) {
                                break;
                            }
                            src = dest ^ ((uint32_t(1) << (32 - k * 8)) - 1);
                        }
                        p[i + 4] = uint8_t(~(((dest >> 24) & 1) - 1));
                        p[i + 3] = uint8_t(dest >> 16);
                        p[i + 2] = uint8_t(dest >> 8);
                        p[i + 1] = uint8_t(dest);
                        i += 5;
                        mask = 0;
                    } else {
                        ++i;
                        mask |= 1;
                        if (near(top)) {
                            mask |= 0x10;
                        }
                    }
                }
            }
        out:
            _prev_mask = mask;
            _prev_pos = prev;
            return i;
        }

        // PowerPC: the big-endian "bl" (opcode 18 with AA = 0, LK = 1)
        size_t _powerpc(uint8_t* p, size_t n) noexcept {
            size_t i = 0;
            for (; i + 4 <= n; i += 4) {
                if ((p[i] >> 2) == 0x12 && (p[i + 3] & 3) == 1) {
                    uint32_t src = (uint32_t(p[i] & 3) << 24) | (uint32_t(p[i + 1]) << 16) | (uint32_t(p[i + 2]) << 8) | (p[i + 3] & ~3u);
                    uint32_t dest = _shift(src, _pos + uint32_t(i));
                    p[i] = uint8_t(0x48 | ((dest >> 24) & 3));
                    p[i + 1] = uint8_t(dest >> 16);
                    p[i + 2] = uint8_t(dest >> 8);
                    p[i + 3] = uint8_t((p[i + 3] & 3) | dest);
                }
            }
            return i;
        }

        // IA-64: bundles of 16 bytes, a template of five bits naming which of
        // the three 41-bit slots hold branches (the table), each such slot's
        // IP-relative target of 21 bits in units of 16
        size_t _ia64(uint8_t* p, size_t n) noexcept {
            static constexpr uint8_t slots[32] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                  4, 4, 6, 6, 0, 0, 7, 7, 4, 4, 0, 0, 4, 4, 0, 0};
            size_t i = 0;
            for (; i + 16 <= n; i += 16) {
                uint32_t which = slots[p[i] & 0x1F];
                for (uint32_t slot = 0, bit = 5; slot < 3; ++slot, bit += 41) {
                    if (!((which >> slot) & 1)) {
                        continue;
                    }
                    size_t at = i + (bit >> 3);
                    uint32_t shift = bit & 7;
                    uint64_t word = 0;
                    for (int j = 0; j < 6; ++j) {
                        word |= uint64_t(p[at + j]) << (8 * j);
                    }
                    uint64_t ins = word >> shift;
                    if (((ins >> 37) & 0xF) != 0x5 || ((ins >> 9) & 0x7) != 0) {
                        continue;
                    }
                    uint32_t src = uint32_t((ins >> 13) & 0xFFFFF) | (uint32_t((ins >> 36) & 1) << 20);
                    src <<= 4;
                    uint32_t dest = _shift(src, _pos + uint32_t(i)) >> 4;
                    ins &= ~(uint64_t(0x8FFFFF) << 13);
                    ins |= uint64_t(dest & 0xFFFFF) << 13;
                    ins |= uint64_t(dest & 0x100000) << (36 - 20);
                    word = (word & ((uint64_t(1) << shift) - 1)) | (ins << shift);
                    for (int j = 0; j < 6; ++j) {
                        p[at + j] = uint8_t(word >> (8 * j));
                    }
                }
            }
            return i;
        }

        // ARM: "bl" (condition always, EB in the top byte), a target of 24
        // bits in words from the instruction + 8
        size_t _arm(uint8_t* p, size_t n) noexcept {
            size_t i = 0;
            for (; i + 4 <= n; i += 4) {
                if (p[i + 3] == 0xEB) {
                    uint32_t src = (uint32_t(p[i + 2]) << 16 | uint32_t(p[i + 1]) << 8 | p[i]) << 2;
                    uint32_t dest = _shift(src, _pos + uint32_t(i) + 8) >> 2;
                    p[i + 2] = uint8_t(dest >> 16);
                    p[i + 1] = uint8_t(dest >> 8);
                    p[i] = uint8_t(dest);
                }
            }
            return i;
        }

        // ARM Thumb: the two halves of "bl" (F000 and F800 prefixes), a
        // target of 22 bits in half-words from the instruction + 4
        size_t _armt(uint8_t* p, size_t n) noexcept {
            size_t i = 0;
            for (; i + 4 <= n; i += 2) {
                if ((p[i + 1] & 0xF8) == 0xF0 && (p[i + 3] & 0xF8) == 0xF8) {
                    uint32_t src = ((uint32_t(p[i + 1]) & 7) << 19 | uint32_t(p[i]) << 11 | (uint32_t(p[i + 3]) & 7) << 8 | p[i + 2]) << 1;
                    uint32_t dest = _shift(src, _pos + uint32_t(i) + 4) >> 1;
                    p[i + 1] = uint8_t(0xF0 | ((dest >> 19) & 7));
                    p[i] = uint8_t(dest >> 11);
                    p[i + 3] = uint8_t(0xF8 | ((dest >> 8) & 7));
                    p[i + 2] = uint8_t(dest);
                    i += 2;
                }
            }
            return i;
        }

        // SPARC: "call" whose 30-bit displacement is within ±8 MiB words
        // (the top bits all 0 or all 1), big-endian
        size_t _sparc(uint8_t* p, size_t n) noexcept {
            size_t i = 0;
            for (; i + 4 <= n; i += 4) {
                if ((p[i] == 0x40 && (p[i + 1] & 0xC0) == 0x00) || (p[i] == 0x7F && (p[i + 1] & 0xC0) == 0xC0)) {
                    uint32_t src = _be32(p + i) << 2;
                    uint32_t dest = _shift(src, _pos + uint32_t(i)) >> 2;
                    dest = (((0 - ((dest >> 22) & 1)) << 22) & 0x3FFFFFFF) | (dest & 0x3FFFFF) | 0x40000000;
                    _put_be32(p + i, dest);
                }
            }
            return i;
        }

        // ARM64: "bl" (26 bits of words) and "adrp" (21 bits of pages, only
        // targets within ±512 MiB)
        size_t _arm64(uint8_t* p, size_t n) noexcept {
            size_t i = 0;
            for (; i + 4 <= n; i += 4) {
                uint32_t ins = _le32(p + i);
                uint32_t pc = _pos + uint32_t(i);
                if ((ins >> 26) == 0x25) {
                    uint32_t words = pc >> 2;
                    if (!_encoder) {
                        words = 0 - words;
                    }
                    _put_le32(p + i, 0x94000000 | ((ins + words) & 0x03FFFFFF));
                } else if ((ins & 0x9F000000) == 0x90000000) {
                    uint32_t src = ((ins >> 29) & 3) | ((ins >> 3) & 0x001FFFFC);
                    if ((src + 0x00020000) & 0x001C0000) {
                        continue;
                    }
                    uint32_t pages = pc >> 12;
                    if (!_encoder) {
                        pages = 0 - pages;
                    }
                    uint32_t dest = src + pages;
                    ins &= 0x9000001F;
                    ins |= (dest & 3) << 29;
                    ins |= (dest & 0x0003FFFC) << 3;
                    ins |= (0 - (dest & 0x00020000)) & 0x00E00000;
                    _put_le32(p + i, ins);
                }
            }
            return i;
        }

        // RISC-V: "jal" with rd = ra or t0, and "auipc" followed by an
        // instruction taking its register as rs1 (the pair of a call or a
        // load of an address). The encoder turns the pair into an auipc of
        // rd = x2 carrying the second instruction's low bits, and the
        // absolute address big-endian; an auipc of x0 or x2 that would be
        // taken for such a pair is swapped into a form the decoder puts back.
        size_t _riscv_encode(uint8_t* p, size_t n) noexcept {
            if (n < 8) {
                return 0;
            }
            size_t last = n - 8;
            size_t i = 0;
            for (; i <= last; i += 2) {
                uint32_t ins = p[i];
                if (ins == 0xEF) {
                    uint32_t b1 = p[i + 1];
                    if (b1 & 0x0D) {
                        continue;
                    }
                    uint32_t b2 = p[i + 2];
                    uint32_t b3 = p[i + 3];
                    uint32_t addr = ((b1 & 0xF0) << 8) | ((b2 & 0x0F) << 16) | ((b2 & 0x10) << 7) | ((b2 & 0xE0) >> 4) | ((b3 & 0x7F) << 4) | ((b3 & 0x80) << 13);
                    addr += _pos + uint32_t(i);
                    p[i + 1] = uint8_t((b1 & 0x0F) | ((addr >> 13) & 0xF0));
                    p[i + 2] = uint8_t(addr >> 9);
                    p[i + 3] = uint8_t(addr >> 1);
                    i += 2;
                } else if ((ins & 0x7F) == 0x17) {
                    ins = _le32(p + i);
                    if (ins & 0xE80) {
                        uint32_t ins2 = _le32(p + i + 4);
                        if (((ins << 8) ^ (ins2 - 3)) & 0xF8003) {
                            i += 4;
                            continue;
                        }
                        uint32_t addr = (ins & 0xFFFFF000) + (ins2 >> 20) - ((ins2 >> 19) & 0x1000);
                        addr += _pos + uint32_t(i);
                        _put_le32(p + i, 0x17 | (2 << 7) | (ins2 << 12));
                        _put_be32(p + i + 4, addr);
                    } else {
                        uint32_t fake_rs1 = ins >> 27;
                        if (uint32_t((ins - 0x3117) << 18) >= (fake_rs1 & 0x1D)) {
                            i += 2;
                            continue;
                        }
                        uint32_t fake_addr = _le32(p + i + 4);
                        uint32_t fake_ins2 = (ins >> 12) | (fake_addr << 20);
                        _put_le32(p + i, 0x17 | (fake_rs1 << 7) | (fake_addr & 0xFFFFF000));
                        _put_le32(p + i + 4, fake_ins2);
                    }
                    i += 6;
                }
            }
            return i;
        }

        size_t _riscv_decode(uint8_t* p, size_t n) noexcept {
            if (n < 8) {
                return 0;
            }
            size_t last = n - 8;
            size_t i = 0;
            for (; i <= last; i += 2) {
                uint32_t ins = p[i];
                if (ins == 0xEF) {
                    uint32_t b1 = p[i + 1];
                    if (b1 & 0x0D) {
                        continue;
                    }
                    uint32_t addr = ((b1 & 0xF0) << 13) | (uint32_t(p[i + 2]) << 9) | (uint32_t(p[i + 3]) << 1);
                    addr -= _pos + uint32_t(i);
                    p[i + 1] = uint8_t((b1 & 0x0F) | ((addr >> 8) & 0xF0));
                    p[i + 2] = uint8_t(((addr >> 16) & 0x0F) | ((addr >> 7) & 0x10) | ((addr << 4) & 0xE0));
                    p[i + 3] = uint8_t(((addr >> 4) & 0x7F) | ((addr >> 13) & 0x80));
                    i += 2;
                } else if ((ins & 0x7F) == 0x17) {
                    ins = _le32(p + i);
                    if (ins & 0xE80) {
                        // a pair the encoder swapped (an auipc of x0 or x2): put back
                        uint32_t ins2 = _le32(p + i + 4);
                        if (((ins << 8) ^ (ins2 - 3)) & 0xF8003) {
                            i += 4;
                            continue;
                        }
                        _put_le32(p + i, (ins2 << 12) | 0x117);
                        _put_le32(p + i + 4, (ins & 0xFFFFF000) | (ins2 >> 20));
                    } else {
                        uint32_t rs1 = ins >> 27;
                        if (uint32_t((ins - 0x3117) << 18) >= (rs1 & 0x1D)) {
                            i += 2;
                            continue;
                        }
                        uint32_t addr = _be32(p + i + 4) - (_pos + uint32_t(i));
                        uint32_t ins2 = (ins >> 12) | (addr << 20);
                        _put_le32(p + i, 0x17 | (rs1 << 7) | ((addr + 0x800) & 0xFFFFF000));
                        _put_le32(p + i + 4, ins2);
                    }
                    i += 6;
                }
            }
            return i;
        }

        // Delta: each byte against the one `distance` before it, the last
        // 256 bytes of the plain data kept round a ring
        size_t _delta(uint8_t* p, size_t n) noexcept {
            uint8_t at = _delta_at;
            const uint8_t d = uint8_t(_distance);
            if (_encoder) {
                for (size_t i = 0; i < n; ++i) {
                    uint8_t plain = p[i];
                    p[i] = uint8_t(plain - _history[uint8_t(at - d)]);
                    _history[at++] = plain;
                }
            } else {
                for (size_t i = 0; i < n; ++i) {
                    uint8_t plain = uint8_t(p[i] + _history[uint8_t(at - d)]);
                    p[i] = plain;
                    _history[at++] = plain;
                }
            }
            _delta_at = at;
            return n;
        }

        SimpleKind _kind = SimpleKind::x86;
        bool _encoder = false;
        uint32_t _pos = 0;
        uint32_t _prev_mask = 0;
        uint32_t _prev_pos = 0;
        uint32_t _distance = 1;
        uint8_t _delta_at = 0;
        uint8_t _history[256];
    };

    // Filters one after another over one buffer: stage k has made final
    // the bytes up to mark[k], and works on the bytes the stage before it
    // made final; what the last stage made final is ready. flush() takes
    // the unconverted last bytes as they are (the end of the data).
    class FilterChain {
    public:
        static constexpr size_t Capacity = size_t(64) << 10;

        SGCL_INLINE_HOT void clear() noexcept {
            _stages.clear();
            _begin = _end = 0;
            std::fill(std::begin(_mark), std::end(_mark), 0);
        }

        SGCL_INLINE_HOT void add(const SimpleFilter& f) noexcept {
            _stages.push_back(f);
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _stages.empty();
        }

        // Room for more bytes (the ready ones handed out first)
        size_t room() noexcept {
            if (_buffer.size() < Capacity) {
                _buffer.resize(Capacity);
            }
            if (_begin == _last() && _begin > 0) {
                sgcl::detail::move_bytes(_buffer.data(), _buffer.data() + _begin, _end - _begin);
                for (size_t k = 0; k < _stages.size(); ++k) {
                    _mark[k] -= _begin;
                }
                _end -= _begin;
                _begin = 0;
            }
            return Capacity - _end;
        }

        SGCL_INLINE_HOT void push(const uint8_t* p, size_t n) noexcept {
            sgcl::detail::copy_bytes(_buffer.data() + _end, p, n);
            _end += n;
        }

        // Where the next bytes go, n of them written there: push without a copy
        SGCL_INLINE_HOT uint8_t* tail() noexcept {
            return _buffer.data() + _end;
        }

        SGCL_INLINE_HOT void pushed(size_t n) noexcept {
            _end += n;
        }

        void run(bool flush) noexcept {
            size_t upto = _end;
            for (size_t k = 0; k < _stages.size(); ++k) {
                size_t from = _mark[k];
                _mark[k] = from + _stages[k].run(_buffer.data() + from, upto - from);
                if (flush) {
                    _mark[k] = upto;
                }
                upto = _mark[k];
            }
        }

        SGCL_INLINE_HOT const uint8_t* ready() const noexcept {
            return _buffer.data() + _begin;
        }

        SGCL_INLINE_HOT size_t ready_size() const noexcept {
            return _last() - _begin;
        }

        SGCL_INLINE_HOT void take(size_t n) noexcept {
            _begin += n;
        }

        // Nothing held
        SGCL_INLINE_HOT bool drained() const noexcept {
            return _begin == _end;
        }

        // The stages over the whole of the data at once, in place
        void apply(uint8_t* p, size_t n) noexcept {
            for (auto& s : _stages) {
                s.run(p, n);
            }
        }

    private:
        SGCL_INLINE_HOT size_t _last() const noexcept {
            return _stages.empty() ? _end : _mark[_stages.size() - 1];
        }

        std::vector<SimpleFilter> _stages;
        std::vector<uint8_t> _buffer;
        size_t _begin = 0;   // the ready bytes not yet taken start here
        size_t _end = 0;
        size_t _mark[4] = {0, 0, 0, 0};
    };
}
