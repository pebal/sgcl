//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "codec_stream.h"
#include "lz4_block.h"
#include "../../hash/crc32.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

// Snappy (format_description.txt and framing_format.txt of google/snappy).
//
// The block: the length of the data as a varint, then elements, each a tag
// byte whose low two bits say what follows:
//   00 a literal of up to 60 bytes (the length less one in the high six
//      bits), or longer: 60..63 say that one to four bytes of the length
//      less one follow, little-endian;
//   01 a copy of 4..11 bytes (three bits) from an offset of 11 bits, its
//      high three in the tag, its low eight in the next byte;
//   10 a copy of 1..64 bytes from an offset of 16 bits;
//   11 a copy of 1..64 bytes from an offset of 32 bits.
// The compressor cuts the data into fragments of 64 KB and compresses each
// on its own, with a table of 4-byte prefixes per fragment, so its offsets
// fit 16 bits; a match longer than 64 bytes is several copies.
//
// The framing: chunks of a type byte, a 3-byte length and the body. The
// stream identifier (0xFF, "sNaPpY") comes first and may come again; a
// compressed chunk (0x00) and an uncompressed one (0x01) hold the masked
// CRC-32C of the data they make (the CRC rotated right by 15 bits plus
// 0xA282EAD8) and up to 64 KB of it; 0xFE is padding, 0x80..0xFD are
// skipped, 0x02..0x7F may not be skipped.
namespace sgcl::compress::detail {
    inline constexpr size_t SnappyFragment = 65536;
    inline constexpr size_t SnappyOutSlack = Lz4OutSlack;
    inline constexpr uint8_t SnappyIdentifier[10] = {0xFF, 0x06, 0x00, 0x00, 's', 'N', 'a', 'P', 'p', 'Y'};

    // The most a block of n bytes compresses to
    SGCL_INLINE_HOT constexpr size_t snappy_bound(size_t n) noexcept {
        return 32 + n + n / 6;
    }

    SGCL_INLINE_HOT uint32_t snappy_mask(uint32_t crc) noexcept {
        return ((crc >> 15) | (crc << 17)) + 0xA282EAD8u;
    }

    SGCL_INLINE_HOT uint32_t snappy_crc(const uint8_t* p, size_t n) noexcept {
        hash::crc32c c;
        c.update(slice<const byte>(reinterpret_cast<const byte*>(p), n));
        return snappy_mask(c.value());
    }

    // The varint at p (at most 5 bytes, a value below 2^32): its bytes, 0
    // when cut short or too long
    SGCL_INLINE_HOT size_t snappy_varint(const uint8_t* p, size_t n, uint64_t& value) noexcept {
        value = 0;
        for (size_t i = 0; i < 5 && i < n; ++i) {
            value |= uint64_t(p[i] & 0x7F) << (7 * i);
            if (!(p[i] & 0x80)) {
                return value >> 32 ? 0 : i + 1;
            }
        }
        return 0;
    }

    SGCL_INLINE_HOT uint8_t* snappy_put_varint(uint8_t* op, uint64_t v) noexcept {
        while (v >= 0x80) {
            *op++ = uint8_t(v | 0x80);
            v >>= 7;
        }
        *op++ = uint8_t(v);
        return op;
    }

    // A literal: its tag, its length bytes, its bytes (8 at a time when
    // more of the input follows it, which `wide` says)
    SGCL_INLINE_HOT uint8_t* snappy_put_literal(uint8_t* op, const uint8_t* p, size_t n, bool wide) noexcept {
        const size_t m = n - 1;
        if (m < 60) {
            *op++ = uint8_t(m << 2);
        } else {
            const int bytes = m < (1u << 8) ? 1 : m < (1u << 16) ? 2 : m < (1u << 24) ? 3 : 4;
            *op++ = uint8_t((59 + bytes) << 2);
            for (int i = 0; i < bytes; ++i) {
                *op++ = uint8_t(m >> (8 * i));
            }
        }
        if (wide && n <= 16) {
            lz4_copy16(op, p);   // the input has 16 bytes here, and the output its bound's room
        } else {
            sgcl::detail::copy_bytes(op, p, n);
        }
        return op + n;
    }

    // A match of n bytes at the offset: copies of up to 64 bytes, the last
    // at least 4 (a 1-byte offset where it fits)
    SGCL_INLINE_HOT uint8_t* snappy_put_copy(uint8_t* op, size_t offset, size_t n) noexcept {
        while (n >= 68) {
            op[0] = uint8_t(((64 - 1) << 2) | 2);
            op[1] = uint8_t(offset);
            op[2] = uint8_t(offset >> 8);
            op += 3;
            n -= 64;
        }
        if (n > 64) {
            op[0] = uint8_t(((60 - 1) << 2) | 2);
            op[1] = uint8_t(offset);
            op[2] = uint8_t(offset >> 8);
            op += 3;
            n -= 60;
        }
        if (n < 12 && offset < 2048) {
            op[0] = uint8_t(1 | ((n - 4) << 2) | ((offset >> 8) << 5));
            op[1] = uint8_t(offset);
            return op + 2;
        }
        op[0] = uint8_t(((n - 1) << 2) | 2);
        op[1] = uint8_t(offset);
        op[2] = uint8_t(offset >> 8);
        return op + 3;
    }

    SGCL_INLINE_HOT uint32_t snappy_hash(uint32_t v, unsigned shift) noexcept {
        return (v * 0x1E35A7BDu) >> shift;
    }

    // The compressor: a table of positions per fragment, cleared for each
    class SnappyCompressor {
    public:
        static constexpr unsigned MaxTableBits = 14;

        SnappyCompressor() noexcept
        : _table(new uint16_t[size_t(1) << MaxTableBits]) {
        }

        // The whole block: the varint, then every fragment; dst has
        // snappy_bound(n) bytes and returns past what was written
        uint8_t* compress(const uint8_t* p, size_t n, uint8_t* dst) noexcept {
            uint8_t* op = snappy_put_varint(dst, n);
            for (size_t at = 0; at < n; at += SnappyFragment) {
                op = _fragment(p + at, std::min(SnappyFragment, n - at), op);
            }
            return op;
        }

    private:
        uint8_t* _fragment(const uint8_t* src, size_t n, uint8_t* op) noexcept {
            const uint8_t* ip = src;
            const uint8_t* const iend = src + n;
            const uint8_t* anchor = src;
            constexpr size_t Margin = 15;   // no match starts in the last 15 bytes
            if (n < Margin + 1) {
                return n ? snappy_put_literal(op, src, n, false) : op;
            }
            unsigned bits = 8;
            while (bits < MaxTableBits && (size_t(1) << bits) < n) {
                ++bits;
            }
            const unsigned shift = 32 - bits;
            std::fill(_table.get(), _table.get() + (size_t(1) << bits), uint16_t(0));
            const uint8_t* const limit = iend - Margin;
            uint32_t next_hash = snappy_hash(lz4_read32(++ip), shift);
            for (;;) {
                // find a match: after misses, step over more bytes
                const uint8_t* match;
                {
                    uint32_t skip = 32;
                    const uint8_t* next = ip;
                    for (;;) {
                        ip = next;
                        const uint32_t h = next_hash;
                        next = ip + (skip++ >> 5);
                        if (next > limit) {
                            goto last;
                        }
                        next_hash = snappy_hash(lz4_read32(next), shift);
                        match = src + _table[h];
                        _table[h] = uint16_t(ip - src);
                        if (lz4_read32(ip) == lz4_read32(match) && match < ip) {
                            break;
                        }
                    }
                }
                op = ip > anchor ? snappy_put_literal(op, anchor, size_t(ip - anchor), true) : op;
                // the match, and a match again right after it
                for (;;) {
                    const size_t len = 4 + lz4_count(ip + 4, match + 4, iend);
                    op = snappy_put_copy(op, size_t(ip - match), len);
                    ip += len;
                    anchor = ip;
                    if (ip >= limit) {
                        goto last;
                    }
                    _table[snappy_hash(lz4_read32(ip - 1), shift)] = uint16_t(ip - 1 - src);
                    const uint32_t h = snappy_hash(lz4_read32(ip), shift);
                    match = src + _table[h];
                    _table[h] = uint16_t(ip - src);
                    if (lz4_read32(ip) != lz4_read32(match)) {
                        break;
                    }
                }
                next_hash = snappy_hash(lz4_read32(++ip), shift);
            }
        last:
            if (anchor < iend) {
                op = snappy_put_literal(op, anchor, size_t(iend - anchor), false);
            }
            return op;
        }

        std::unique_ptr<uint16_t[]> _table;
    };

    // The compressor of a whole call, lent by the thread (no allocation for
    // a small block); a call made while it is out gets one of its own
    class LentSnappy {
    public:
        LentSnappy() noexcept {
            auto& k = _kept();
            if (k.lent) {
                _own = std::make_unique<SnappyCompressor>();
                _c = _own.get();
            } else {
                if (!k.compressor) {
                    k.compressor = std::make_unique<SnappyCompressor>();
                }
                k.lent = true;
                _kept_by = &k;
                _c = k.compressor.get();
            }
        }

        LentSnappy(const LentSnappy&) = delete;
        LentSnappy& operator=(const LentSnappy&) = delete;

        SGCL_INLINE_HOT ~LentSnappy() {
            if (_kept_by) {
                _kept_by->lent = false;
            }
        }

        SGCL_INLINE_HOT SnappyCompressor* operator->() const noexcept {
            return _c;
        }

    private:
        struct Kept {
            std::unique_ptr<SnappyCompressor> compressor;
            bool lent = false;
        };

        static Kept& _kept() noexcept {
            thread_local Kept kept;
            return kept;
        }

        SnappyCompressor* _c = nullptr;
        std::unique_ptr<SnappyCompressor> _own;
        Kept* _kept_by = nullptr;
    };

    enum class SnappyDecoded : uint8_t {
        ok,
        truncated,   // the elements end inside one
        overflow,    // more data than the length says
        offset,      // a copy of offset 0, or before the start
        short_data   // less data than the length says
    };

    struct SnappyDecodeResult {
        SnappyDecoded status;
        size_t at;   // the byte of the elements where it failed
    };

    // Decodes the elements [src, src + n) into exactly `length` bytes at dst,
    // which has SnappyOutSlack bytes of room past them
    inline SnappyDecodeResult snappy_decode(const uint8_t* src, size_t n, uint8_t* dst, size_t length) noexcept {
        const uint8_t* ip = src;
        const uint8_t* const iend = src + n;
        uint8_t* op = dst;
        uint8_t* const oend = dst + length;
        auto fail = [&](SnappyDecoded why) noexcept {
            return SnappyDecodeResult {why, size_t(ip - src)};
        };
        while (ip < iend) {
            const unsigned tag = *ip++;
            const unsigned kind = tag & 3;
            if (kind == 0) {
                size_t len = (tag >> 2) + 1;
                if (len <= 16 && iend - ip >= 16 && oend - op >= 16) {
                    lz4_copy16(op, ip);   // the short literal: one wide copy
                    op += len;
                    ip += len;
                    continue;
                }
                if (len > 60) {
                    const size_t bytes = len - 60;
                    if (size_t(iend - ip) < bytes) {
                        return fail(SnappyDecoded::truncated);
                    }
                    size_t m = 0;
                    for (size_t i = 0; i < bytes; ++i) {
                        m |= size_t(ip[i]) << (8 * i);
                    }
                    ip += bytes;
                    len = m + 1;
                }
                if (len > size_t(iend - ip)) {
                    return fail(SnappyDecoded::truncated);
                }
                if (len > size_t(oend - op)) {
                    return fail(SnappyDecoded::overflow);
                }
                sgcl::detail::copy_bytes(op, ip, len);
                op += len;
                ip += len;
                continue;
            }
            size_t len, offset;
            if (kind == 1) {
                if (ip >= iend) {
                    return fail(SnappyDecoded::truncated);
                }
                len = ((tag >> 2) & 7) + 4;
                offset = (size_t(tag >> 5) << 8) | *ip++;
            } else if (kind == 2) {
                if (iend - ip < 2) {
                    return fail(SnappyDecoded::truncated);
                }
                len = (tag >> 2) + 1;
                offset = size_t(ip[0]) | size_t(ip[1]) << 8;
                ip += 2;
            } else {
                if (iend - ip < 4) {
                    return fail(SnappyDecoded::truncated);
                }
                len = (tag >> 2) + 1;
                offset = size_t(ip[0]) | size_t(ip[1]) << 8 | size_t(ip[2]) << 16 | size_t(ip[3]) << 24;
                ip += 4;
            }
            if (offset == 0 || offset > size_t(op - dst)) {
                return fail(SnappyDecoded::offset);
            }
            if (len > size_t(oend - op)) {
                return fail(SnappyDecoded::overflow);
            }
            const uint8_t* m = op - offset;
            uint8_t* const e = op + len;
            if (offset >= 16) {
                do {
                    lz4_copy16(op, m);
                    op += 16;
                    m += 16;
                } while (op < e);
            } else if (offset >= 8) {
                do {
                    lz4_copy8(op, m);
                    op += 8;
                    m += 8;
                } while (op < e);
            } else {
                // the pattern of `offset` bytes, the first eight one by one,
                // then from the nearest multiple of it eight or more back
                for (int i = 0; i < 8; ++i) {
                    op[i] = m[i];
                }
                static constexpr uint8_t step[8] = {0, 8, 8, 9, 8, 10, 12, 14};
                uint8_t* q = op + 8;
                const uint8_t* s = q - step[offset];
                while (q < e) {
                    lz4_copy8(q, s);
                    q += 8;
                    s += 8;
                }
            }
            op = e;
        }
        if (op != oend) {
            return fail(SnappyDecoded::short_data);
        }
        return {SnappyDecoded::ok, 0};
    }

    SGCL_INLINE_HOT const char* snappy_why(SnappyDecoded d) noexcept {
        switch (d) {
            case SnappyDecoded::truncated: return "snappy: an element cut short";
            case SnappyDecoded::overflow: return "snappy: more data than the block's length";
            case SnappyDecoded::offset: return "snappy: a copy before the start of the data";
            case SnappyDecoded::short_data: return "snappy: less data than the block's length";
            case SnappyDecoded::ok: break;
        }
        return "snappy: corrupt data";
    }

    // A whole block in memory: its length checked against the limit before
    // anything is made
    inline expected<vector<byte>, error> snappy_decompress_block(const uint8_t* p, size_t n, const limits& l) noexcept {
        uint64_t length;
        const size_t head = snappy_varint(p, n, length);
        if (!head) {
            return unexpected<error>(error(n < 5 ? errc::unexpected_end : errc::corrupt, 0, string("snappy: a block's length that cannot be read")));
        }
        if (length > l.max_size) {
            return unexpected<error>(error(errc::too_large, 0, string("snappy: decompressed data past the limit")));
        }
        vector<byte> result;
        sgcl::detail::VectorOverwrite::resize(result, size_t(length) + SnappyOutSlack);   // every byte of length written, or the call fails
        auto r = snappy_decode(p + head, n - head, reinterpret_cast<uint8_t*>(result.data()), size_t(length));
        if (r.status != SnappyDecoded::ok) {
            const errc code = r.status == SnappyDecoded::truncated ? errc::unexpected_end : errc::corrupt;
            return unexpected<error>(error(code, head + r.at, string(snappy_why(r.status))));
        }
        result.resize(size_t(length));
        return result;
    }

    // A chunk of the framing format out: compressed, or as it is when that
    // saves less than an eighth
    template<class Out>
    void snappy_put_chunk(Out& out, SnappyCompressor& c, uint8_t* scratch, const uint8_t* p, size_t n) noexcept {
        const uint32_t crc = snappy_crc(p, n);
        uint8_t* const body = scratch + 8;
        const size_t size = size_t(c.compress(p, n, body) - body);
        const bool stored = size >= n - n / 8;
        const size_t len = (stored ? n : size) + 4;
        scratch[0] = stored ? 0x01 : 0x00;
        scratch[1] = uint8_t(len);
        scratch[2] = uint8_t(len >> 8);
        scratch[3] = uint8_t(len >> 16);
        scratch[4] = uint8_t(crc);
        scratch[5] = uint8_t(crc >> 8);
        scratch[6] = uint8_t(crc >> 16);
        scratch[7] = uint8_t(crc >> 24);
        if (stored) {
            append_bytes(out, scratch, 8);
            append_bytes(out, p, n);
        } else {
            append_bytes(out, scratch, 8 + size);
        }
    }

    // A whole compress of the framing format in memory
    template<class Out>
    void snappy_compress_framed(Out& out, const uint8_t* p, size_t n) noexcept {
        append_bytes(out, SnappyIdentifier, sizeof SnappyIdentifier);
        LentSnappy c;
        std::unique_ptr<uint8_t[]> scratch(new uint8_t[8 + snappy_bound(SnappyFragment) + SnappyOutSlack]);
        for (size_t at = 0; at < n; at += SnappyFragment) {
            snappy_put_chunk(out, *c.operator->(), scratch.get(), p + at, std::min(SnappyFragment, n - at));
        }
    }

    // The framing format's writer: what is written gathers in a chunk of
    // 64 KB, which goes out when full, at a flush and at the close
    class SnappyFramedEncoder {
    public:
        static constexpr const char* name = "snappy";

        template<class O>
        explicit SnappyFramedEncoder(const O&) noexcept
        : _data(new uint8_t[SnappyFragment])
        , _scratch(new uint8_t[8 + snappy_bound(SnappyFragment) + SnappyOutSlack]) {
        }

        SGCL_INLINE_HOT const char* setup_error() const noexcept {
            return nullptr;
        }

        template<class Out>
        void start(Out& out) noexcept {
            append_bytes(out, SnappyIdentifier, sizeof SnappyIdentifier);
        }

        template<class Out>
        void write(const uint8_t* p, size_t n, Out& out) noexcept {
            while (n) {
                const size_t k = std::min(n, SnappyFragment - _fill);
                sgcl::detail::copy_bytes(_data.get() + _fill, p, k);
                _fill += k;
                p += k;
                n -= k;
                if (_fill == SnappyFragment) {
                    flush(out);
                }
            }
        }

        template<class Out>
        void flush(Out& out) noexcept {
            if (_fill) {
                snappy_put_chunk(out, _c, _scratch.get(), _data.get(), _fill);
                _fill = 0;
            }
        }

        template<class Out>
        void finish(Out& out) noexcept {
            flush(out);
        }

        void reset() noexcept {
            _fill = 0;
        }

    private:
        SnappyCompressor _c;
        std::unique_ptr<uint8_t[]> _data;
        std::unique_ptr<uint8_t[]> _scratch;
        size_t _fill = 0;
    };

    // One chunk read: what it is and what to do (the framing's rules)
    enum class SnappyChunk : uint8_t {
        identifier,
        compressed,
        uncompressed,
        skip,
        bad
    };

    SGCL_INLINE_HOT SnappyChunk snappy_chunk_kind(uint8_t type) noexcept {
        if (type == 0xFF) {
            return SnappyChunk::identifier;
        }
        if (type == 0x00) {
            return SnappyChunk::compressed;
        }
        if (type == 0x01) {
            return SnappyChunk::uncompressed;
        }
        if (type >= 0x80) {
            return SnappyChunk::skip;   // 0x80..0xFD skippable, 0xFE padding
        }
        return SnappyChunk::bad;        // 0x02..0x7F: reserved, not to be skipped
    }

    // A chunk's body checked and decoded into dst (64 KB and the slack):
    // the bytes made, or the failure's code and words
    struct SnappyChunkResult {
        size_t made = 0;
        errc code = errc::corrupt;
        const char* text = nullptr;
        size_t at = 0;   // within the body
    };

    inline SnappyChunkResult snappy_chunk(SnappyChunk kind, const uint8_t* body, size_t len, uint8_t* dst) noexcept {
        SnappyChunkResult r;
        if (len < 4) {
            r.text = "snappy: a chunk too short for its checksum";
            return r;
        }
        const uint32_t crc = uint32_t(body[0]) | uint32_t(body[1]) << 8 | uint32_t(body[2]) << 16 | uint32_t(body[3]) << 24;
        const uint8_t* data = body + 4;
        const size_t n = len - 4;
        if (kind == SnappyChunk::uncompressed) {
            if (n > SnappyFragment) {
                r.text = "snappy: an uncompressed chunk of more than 64 KB";
                return r;
            }
            sgcl::detail::copy_bytes(dst, data, n);
            r.made = n;
        } else {
            uint64_t length;
            const size_t head = snappy_varint(data, n, length);
            if (!head) {
                r.text = "snappy: a chunk's length that cannot be read";
                r.at = 4;
                return r;
            }
            if (length > SnappyFragment) {
                r.text = "snappy: a compressed chunk of more than 64 KB of data";
                r.at = 4;
                return r;
            }
            auto d = snappy_decode(data + head, n - head, dst, size_t(length));
            if (d.status != SnappyDecoded::ok) {
                r.text = snappy_why(d.status);
                r.at = 4 + head + d.at;
                return r;
            }
            r.made = size_t(length);
        }
        if (snappy_crc(dst, r.made) != crc) {
            r.made = 0;
            r.code = errc::checksum;
            r.text = "snappy: a chunk's checksum does not match";
            return r;
        }
        r.text = nullptr;
        return r;
    }

    // A whole decompress of the framing format in memory
    inline expected<vector<byte>, error> snappy_decompress_framed(const uint8_t* p, size_t n, const limits& l) noexcept {
        auto fail = [](errc code, uint64_t at, const char* text) noexcept {
            return unexpected<error>(error(code, at, string(text)));
        };
        if (n == 0) {
            return fail(errc::unexpected_end, 0, "snappy: no stream identifier");
        }
        vector<byte> result;
        size_t capacity = 0;
        size_t total = 0;
        size_t at = 0;
        bool started = false;
        while (at < n) {
            if (n - at < 4) {
                return fail(errc::unexpected_end, n, "snappy: unexpected end in a chunk's header");
            }
            const uint8_t type = p[at];
            const size_t len = size_t(p[at + 1]) | size_t(p[at + 2]) << 8 | size_t(p[at + 3]) << 16;
            const SnappyChunk kind = snappy_chunk_kind(type);
            if (len > n - at - 4) {
                return fail(errc::unexpected_end, n, "snappy: unexpected end in a chunk");
            }
            const uint8_t* body = p + at + 4;
            if (!started && kind != SnappyChunk::identifier) {
                return fail(errc::invalid_header, at, "snappy: the stream does not begin with its identifier");
            }
            switch (kind) {
                case SnappyChunk::identifier:
                    if (len != 6 || std::memcmp(body, SnappyIdentifier + 4, 6) != 0) {
                        return fail(errc::invalid_header, at, "snappy: a wrong stream identifier");
                    }
                    started = true;
                    break;
                case SnappyChunk::skip:
                    break;
                case SnappyChunk::bad:
                    return fail(errc::corrupt, at, "snappy: a reserved chunk that may not be skipped");
                case SnappyChunk::compressed:
                case SnappyChunk::uncompressed: {
                    if (total + SnappyFragment + SnappyOutSlack > capacity) {
                        // a chunk makes 64 KB at most, and total stays within the limit
                        const size_t c = std::max<size_t>({capacity * 2, total + SnappyFragment + SnappyOutSlack, size_t(1) << 17});
                        vector<byte> grown;
                        sgcl::detail::VectorOverwrite::resize(grown, c);
                        copy_out(grown.data(), result.data(), total);
                        result = std::move(grown);
                        capacity = result.size();
                    }
                    auto r = snappy_chunk(kind, body, len, reinterpret_cast<uint8_t*>(result.data()) + total);
                    if (r.text) {
                        return fail(r.code, at + 4 + r.at, r.text);
                    }
                    total += r.made;
                    if (total > l.max_size) {
                        return fail(errc::too_large, at, "snappy: decompressed data past the limit");
                    }
                    break;
                }
            }
            at += 4 + len;
        }
        result.resize(total);
        return result;
    }

    // The framing format's streaming decoder: a chunk gathered whole (its
    // length at most 16 MB, the body of a compressed one at most what 64 KB
    // compresses to), decoded into a window of 64 KB and handed out
    class SnappyFramedDecoder {
    public:
        static constexpr const char* name = "snappy";

        errc error = errc::corrupt;
        const char* error_text = nullptr;

        template<class O>
        SnappyFramedDecoder(const O&, const limits&) noexcept
        : _window(new uint8_t[SnappyFragment + SnappyOutSlack])
        , _body(new uint8_t[snappy_bound(SnappyFragment) + 16]) {
            reset();
        }

        void reset() noexcept {
            _have = 0;
            _started = false;
            _in_chunk = false;
            _skip = 0;
            _out_begin = _out_end = 0;
            _taken = 0;
            error = errc::corrupt;
            error_text = nullptr;
        }

        SGCL_INLINE_HOT uint64_t offset() const noexcept {
            return _taken;
        }

        CodecStatus decode(const uint8_t*& in, const uint8_t* end, bool final, uint8_t* out, size_t& pos, size_t cap) noexcept {
            for (;;) {
                if (_out_begin < _out_end) {
                    const size_t k = std::min(_out_end - _out_begin, cap - pos);
                    copy_out(out + pos, _window.get() + _out_begin, k);
                    pos += k;
                    _out_begin += k;
                    if (_out_begin < _out_end) {
                        return CodecStatus::need_room;
                    }
                }
                if (_skip) {
                    const size_t k = size_t(std::min<uint64_t>(_skip, uint64_t(end - in)));
                    in += k;
                    _taken += k;
                    _skip -= k;
                    if (_skip) {
                        return _more(final, "snappy: unexpected end in a chunk");
                    }
                    continue;
                }
                if (!_in_chunk) {
                    // the chunk's header
                    if (in == end && _have == 0) {
                        if (final) {
                            if (_started) {
                                return CodecStatus::done;
                            }
                            return _fail(errc::unexpected_end, "snappy: no stream identifier");
                        }
                        return CodecStatus::need_input;
                    }
                    const size_t k = std::min(size_t(4) - _have, size_t(end - in));
                    sgcl::detail::copy_bytes(_head + _have, in, k);
                    _have += k;
                    in += k;
                    _taken += k;
                    if (_have < 4) {
                        return _more(final, "snappy: unexpected end in a chunk's header");
                    }
                    _have = 0;
                    _kind = snappy_chunk_kind(_head[0]);
                    _len = size_t(_head[1]) | size_t(_head[2]) << 8 | size_t(_head[3]) << 16;
                    _chunk_at = _taken - 4;
                    if (!_started && _kind != SnappyChunk::identifier) {
                        return _fail(errc::invalid_header, "snappy: the stream does not begin with its identifier");
                    }
                    if (_kind == SnappyChunk::bad) {
                        return _fail(errc::corrupt, "snappy: a reserved chunk that may not be skipped");
                    }
                    if (_kind == SnappyChunk::skip) {
                        _skip = _len;
                        continue;
                    }
                    if ((_kind == SnappyChunk::identifier && _len != 6) || _len > snappy_bound(SnappyFragment) + 4) {
                        return _fail(_kind == SnappyChunk::identifier ? errc::invalid_header : errc::corrupt,
                                     _kind == SnappyChunk::identifier ? "snappy: a wrong stream identifier" : "snappy: a chunk longer than 64 KB compresses to");
                    }
                    _in_chunk = true;
                    _body_have = 0;
                }
                // the chunk's body: from the input when it is all there, else gathered
                const uint8_t* body;
                if (_body_have == 0 && size_t(end - in) >= _len) {
                    body = in;
                    in += _len;
                    _taken += _len;
                } else {
                    const size_t k = std::min(_len - _body_have, size_t(end - in));
                    sgcl::detail::copy_bytes(_body.get() + _body_have, in, k);
                    _body_have += k;
                    in += k;
                    _taken += k;
                    if (_body_have < _len) {
                        return _more(final, "snappy: unexpected end in a chunk");
                    }
                    body = _body.get();
                }
                _in_chunk = false;
                if (_kind == SnappyChunk::identifier) {
                    if (std::memcmp(body, SnappyIdentifier + 4, 6) != 0) {
                        return _fail(errc::invalid_header, "snappy: a wrong stream identifier");
                    }
                    _started = true;
                    continue;
                }
                auto r = snappy_chunk(_kind, body, _len, _window.get());
                if (r.text) {
                    _taken = _chunk_at + 4 + r.at;
                    return _fail(r.code, r.text);
                }
                _out_begin = 0;
                _out_end = r.made;
            }
        }

    private:
        SGCL_INLINE_HOT CodecStatus _more(bool final, const char* text) noexcept {
            if (final) {
                return _fail(errc::unexpected_end, text);
            }
            return CodecStatus::need_input;
        }

        CodecStatus _fail(errc code, const char* text) noexcept {
            error = code;
            error_text = text;
            return CodecStatus::failed;
        }

        std::unique_ptr<uint8_t[]> _window;
        std::unique_ptr<uint8_t[]> _body;
        uint8_t _head[4];
        size_t _have = 0;
        bool _started = false;
        bool _in_chunk = false;
        SnappyChunk _kind = SnappyChunk::bad;
        size_t _len = 0;
        size_t _body_have = 0;
        uint64_t _skip = 0;
        uint64_t _taken = 0;
        uint64_t _chunk_at = 0;
        size_t _out_begin = 0;
        size_t _out_end = 0;
    };
}
