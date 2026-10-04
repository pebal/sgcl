//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bcj.h"
#include "lzma2.h"
#include "stream.h"
#include "../../crypto/sha256.h"
#include "../../hash/crc32.h"
#include "../../hash/crc64.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace sgcl::compress::detail {
    // The .xz container (xz-file-format 1.2): a stream is a header of 12
    // bytes (the magic, the flags naming the check, their CRC-32), blocks,
    // an index of the blocks' sizes, and a footer of 12 bytes (a CRC-32,
    // the index's size, the flags again, "YZ"); streams may follow one
    // another with zero bytes between them in fours. A block is a header
    // (its size in the first byte, the sizes of the block when known, up
    // to four filters with their properties, zeros, a CRC-32), the data
    // the filters make (LZMA2 last), zeros to a multiple of four, and the
    // check of the data decompressed.
    namespace xz_format {
        constexpr uint8_t Magic[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
        constexpr size_t StreamHeaderSize = 12;
        constexpr size_t BlockHeaderMax = 1024;
        constexpr uint64_t VliMax = (uint64_t(1) << 63) - 1;
        constexpr uint64_t FilterDelta = 0x03;
        constexpr uint64_t FilterX86 = 0x04;
        constexpr uint64_t FilterRiscv = 0x0B;
        constexpr uint64_t FilterLzma2 = 0x21;

        // The bytes of a check type's field (the types without a check
        // defined still have a size, so that a reader can skip them)
        inline size_t check_size(uint8_t type) noexcept {
            static constexpr uint8_t sizes[16] = {0, 4, 4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 32, 64, 64, 64};
            return sizes[type & 15];
        }

        SGCL_INLINE_HOT bool check_supported(uint8_t type) noexcept {
            return type == 0 || type == 1 || type == 4 || type == 10;
        }

        SGCL_INLINE_HOT uint32_t crc32(const uint8_t* p, size_t n) noexcept {
            return hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(p), n));
        }

        // A variable-length integer: 7 bits a byte, least significant first,
        // at most 9 bytes, the last not 0 unless it is the only one. The
        // bytes it took, 0 when it needs more than there are, -1 when invalid.
        inline int read_vli(const uint8_t* p, size_t n, uint64_t& v) noexcept {
            v = 0;
            for (size_t i = 0; i < 9; ++i) {
                if (i == n) {
                    return 0;
                }
                uint8_t b = p[i];
                v |= uint64_t(b & 0x7F) << (7 * i);
                if (!(b & 0x80)) {
                    if (b == 0 && i > 0) {
                        return -1;
                    }
                    return int(i + 1);
                }
            }
            return -1;
        }

        inline void put_vli(std::vector<uint8_t>& out, uint64_t v) noexcept {
            while (v >= 0x80) {
                out.push_back(uint8_t(v | 0x80));
                v >>= 7;
            }
            out.push_back(uint8_t(v));
        }

        SGCL_INLINE_HOT void put_le64(std::vector<uint8_t>& out, uint64_t v) noexcept {
            put_le32(out, uint32_t(v));
            put_le32(out, uint32_t(v >> 32));
        }

        // The check of a block, over its data decompressed
        class Check {
        public:
            SGCL_INLINE_HOT void reset(uint8_t type) noexcept {
                _type = type;
                _crc32 = hash::crc32();
                _crc64 = hash::crc64();
                _sha256 = crypto::sha256();
            }

            SGCL_INLINE_HOT uint8_t type() const noexcept {
                return _type;
            }

            void update(const uint8_t* p, size_t n) noexcept {
                if (!n) {
                    return;
                }
                slice<const byte> s(reinterpret_cast<const byte*>(p), n);
                switch (_type) {
                    case 1: _crc32.update(s); break;
                    case 4: _crc64.update(s); break;
                    case 10: _sha256.update(s); break;
                    default: break;
                }
            }

            // The field as the format stores it (CRCs little-endian)
            void value(uint8_t* out) const noexcept {
                switch (_type) {
                    case 1: {
                        uint32_t v = _crc32.value();
                        for (int i = 0; i < 4; ++i) out[i] = uint8_t(v >> (8 * i));
                        break;
                    }
                    case 4: {
                        uint64_t v = _crc64.value();
                        for (int i = 0; i < 8; ++i) out[i] = uint8_t(v >> (8 * i));
                        break;
                    }
                    case 10: {
                        auto d = _sha256.value();
                        std::memcpy(out, d.data(), 32);
                        break;
                    }
                    default:
                        break;
                }
            }

        private:
            uint8_t _type = 0;
            hash::crc32 _crc32;
            hash::crc64 _crc64;
            crypto::sha256 _sha256;
        };

        struct Filter {
            uint64_t id = 0;
            uint32_t value = 0;   // LZMA2: the dictionary; Delta: the distance; BCJ: the start offset
        };

        struct BlockHeader {
            size_t size = 0;                    // the header's bytes
            uint64_t compressed = UINT64_MAX;   // when the header gives them
            uint64_t uncompressed = UINT64_MAX;
            Filter filters[4];
            size_t count = 0;
        };

        // The simple filter of an ID (BCJ or Delta)
        inline bool simple_kind(uint64_t id, SimpleKind& k) noexcept {
            switch (id) {
                case FilterDelta: k = SimpleKind::delta; return true;
                case 0x04: k = SimpleKind::x86; return true;
                case 0x05: k = SimpleKind::powerpc; return true;
                case 0x06: k = SimpleKind::ia64; return true;
                case 0x07: k = SimpleKind::arm; return true;
                case 0x08: k = SimpleKind::armt; return true;
                case 0x09: k = SimpleKind::sparc; return true;
                case 0x0A: k = SimpleKind::arm64; return true;
                case 0x0B: k = SimpleKind::riscv; return true;
                default: return false;
            }
        }

        // A block header from p[0, n): Parsed::more when it needs the rest
        inline Parsed parse_block_header(const uint8_t* p, size_t n, BlockHeader& h) noexcept {
            if (n < 1) {
                return Parsed::need();
            }
            size_t size = (size_t(p[0]) + 1) * 4;
            if (n < size) {
                return Parsed::need();
            }
            if (crc32(p, size - 4) != le32(p + size - 4)) {
                return Parsed::fail(errc::checksum, "xz: block header CRC-32 mismatch");
            }
            uint8_t flags = p[1];
            if (flags & 0x3C) {
                return Parsed::fail(errc::unsupported, "xz: reserved block flags set");
            }
            h = BlockHeader();
            h.size = size;
            h.count = size_t(flags & 3) + 1;
            size_t at = 2;
            const size_t stop = size - 4;
            auto vli = [&](uint64_t& v) noexcept {
                int k = read_vli(p + at, stop - at, v);
                if (k <= 0) {
                    return false;
                }
                at += size_t(k);
                return true;
            };
            if (flags & 0x40) {
                if (!vli(h.compressed) || h.compressed == 0) {
                    return Parsed::fail(errc::corrupt, "xz: invalid compressed size in a block header");
                }
            }
            if (flags & 0x80) {
                if (!vli(h.uncompressed)) {
                    return Parsed::fail(errc::corrupt, "xz: invalid uncompressed size in a block header");
                }
            }
            for (size_t i = 0; i < h.count; ++i) {
                uint64_t id, props;
                if (!vli(id) || !vli(props) || props > stop - at) {
                    return Parsed::fail(errc::corrupt, "xz: invalid filter flags in a block header");
                }
                const uint8_t* q = p + at;
                at += size_t(props);
                bool last = i + 1 == h.count;
                Filter& f = h.filters[i];
                f.id = id;
                SimpleKind kind;
                if (id == FilterLzma2) {
                    if (!last) {
                        return Parsed::fail(errc::unsupported, "xz: LZMA2 before the last filter");
                    }
                    if (props != 1 || !lzma2_dictionary(q[0], f.value)) {
                        return Parsed::fail(errc::corrupt, "xz: invalid LZMA2 properties");
                    }
                } else if (simple_kind(id, kind)) {
                    if (last) {
                        return Parsed::fail(errc::unsupported, "xz: a last filter other than LZMA2");
                    }
                    if (kind == SimpleKind::delta) {
                        if (props != 1) {
                            return Parsed::fail(errc::corrupt, "xz: invalid Delta properties");
                        }
                        f.value = uint32_t(q[0]) + 1;
                    } else if (props == 4) {
                        f.value = le32(q);
                        if (f.value % SimpleFilter::alignment(kind)) {
                            return Parsed::fail(errc::unsupported, "xz: a BCJ start offset out of alignment");
                        }
                    } else if (props != 0) {
                        return Parsed::fail(errc::corrupt, "xz: invalid BCJ properties");
                    }
                } else {
                    return Parsed::fail(errc::unsupported, "xz: unsupported filter");
                }
            }
            for (; at < stop; ++at) {
                if (p[at]) {
                    return Parsed::fail(errc::unsupported, "xz: block header padding not zero");
                }
            }
            return Parsed::done(size);
        }

        // The decoding stages of a block's filters (before LZMA2, last first)
        inline void decoding_chain(const BlockHeader& h, FilterChain& chain) noexcept {
            chain.clear();
            for (size_t i = h.count - 1; i-- > 0;) {
                SimpleKind kind;
                simple_kind(h.filters[i].id, kind);
                SimpleFilter f;
                if (kind == SimpleKind::delta) {
                    f.init(kind, false, 0, h.filters[i].value);
                } else {
                    f.init(kind, false, h.filters[i].value);
                }
                chain.add(f);
            }
        }

        SGCL_INLINE_HOT void stream_header(std::vector<uint8_t>& out, uint8_t check) noexcept {
            out.insert(out.end(), Magic, Magic + 6);
            uint8_t flags[2] = {0, check};
            out.insert(out.end(), flags, flags + 2);
            put_le32(out, crc32(flags, 2));
        }

        // A header of the filters given (IDs and one-byte properties or
        // none), with the sizes when known
        inline void block_header(std::vector<uint8_t>& out, const Filter* filters, size_t count, uint64_t compressed, uint64_t uncompressed) noexcept {
            std::vector<uint8_t> h;
            h.push_back(0);
            uint8_t flags = uint8_t(count - 1);
            if (compressed != UINT64_MAX) flags |= 0x40;
            if (uncompressed != UINT64_MAX) flags |= 0x80;
            h.push_back(flags);
            if (compressed != UINT64_MAX) put_vli(h, compressed);
            if (uncompressed != UINT64_MAX) put_vli(h, uncompressed);
            for (size_t i = 0; i < count; ++i) {
                put_vli(h, filters[i].id);
                if (filters[i].id == FilterLzma2) {
                    put_vli(h, 1);
                    h.push_back(lzma2_dictionary_byte(filters[i].value));
                } else if (filters[i].id == FilterDelta) {
                    put_vli(h, 1);
                    h.push_back(uint8_t(filters[i].value - 1));
                } else {
                    put_vli(h, 0);
                }
            }
            while ((h.size() + 4) % 4) {
                h.push_back(0);
            }
            h[0] = uint8_t((h.size() + 4) / 4 - 1);
            put_le32(h, crc32(h.data(), h.size()));
            out.insert(out.end(), h.begin(), h.end());
        }

        struct Record {
            uint64_t unpadded;
            uint64_t uncompressed;
        };

        // The index and the footer of a stream of these blocks
        inline void index_and_footer(std::vector<uint8_t>& out, const std::vector<Record>& records, uint8_t check) noexcept {
            std::vector<uint8_t> x;
            x.push_back(0);
            put_vli(x, records.size());
            for (auto& r : records) {
                put_vli(x, r.unpadded);
                put_vli(x, r.uncompressed);
            }
            while (x.size() % 4) {
                x.push_back(0);
            }
            put_le32(x, crc32(x.data(), x.size()));
            out.insert(out.end(), x.begin(), x.end());
            uint8_t tail[6];
            uint32_t backward = uint32_t(x.size() / 4 - 1);
            for (int i = 0; i < 4; ++i) tail[i] = uint8_t(backward >> (8 * i));
            tail[4] = 0;
            tail[5] = check;
            put_le32(out, crc32(tail, 6));
            out.insert(out.end(), tail, tail + 6);
            out.push_back('Y');
            out.push_back('Z');
        }

        // The stream flags of a header (p: its 12 bytes): the check type
        inline Parsed parse_stream_header(const uint8_t* p, size_t n, uint8_t& check) noexcept {
            if (std::memcmp(p, Magic, std::min<size_t>(n, 6)) != 0) {
                return Parsed::fail(errc::invalid_header, "xz: not an xz stream");
            }
            if (n < StreamHeaderSize) {
                return Parsed::need();
            }
            if (crc32(p + 6, 2) != le32(p + 8)) {
                return Parsed::fail(errc::checksum, "xz: stream header CRC-32 mismatch");
            }
            if (p[6] != 0 || (p[7] & 0xF0)) {
                return Parsed::fail(errc::unsupported, "xz: reserved stream flags set");
            }
            check = p[7];
            if (!check_supported(check)) {
                return Parsed::fail(errc::unsupported, "xz: unsupported check type");
            }
            return Parsed::done(StreamHeaderSize);
        }

        // The footer (its 12 bytes), against the index's size and the header's flags
        inline Parsed parse_footer(const uint8_t* p, size_t n, uint64_t index_size, uint8_t check) noexcept {
            if (n < StreamHeaderSize) {
                return Parsed::need();
            }
            if (p[10] != 'Y' || p[11] != 'Z') {
                return Parsed::fail(errc::corrupt, "xz: stream footer magic missing");
            }
            if (crc32(p + 4, 6) != le32(p)) {
                return Parsed::fail(errc::checksum, "xz: stream footer CRC-32 mismatch");
            }
            if ((uint64_t(le32(p + 4)) + 1) * 4 != index_size) {
                return Parsed::fail(errc::corrupt, "xz: the footer's index size does not match the index");
            }
            if (p[8] != 0 || p[9] != check) {
                return Parsed::fail(errc::corrupt, "xz: the footer's flags differ from the header's");
            }
            return Parsed::done(StreamHeaderSize);
        }

        // An index read a piece at a time, against the blocks decoded
        class IndexReader {
        public:
            void start() noexcept {
                _phase = 0;
                _records = 0;
                _next = 0;
                _size = 1;   // the indicator, taken before
                _crc = hash::crc32();
                uint8_t zero = 0;
                _crc.update(slice<const byte>(reinterpret_cast<const byte*>(&zero), 1));
            }

            SGCL_INLINE_HOT uint64_t size() const noexcept {
                return _size;
            }

            // Takes what it can of p[0, n) into used; ok when the index is whole
            Parsed step(const uint8_t* p, size_t n, const std::vector<Record>& blocks, size_t& used) noexcept {
                used = 0;
                auto take = [&](size_t k) noexcept {
                    _crc.update(slice<const byte>(reinterpret_cast<const byte*>(p + used), k));
                    used += k;
                    _size += k;
                };
                for (;;) {
                    if (_phase == 0) {
                        uint64_t count;
                        int k = read_vli(p + used, n - used, count);
                        if (k == 0) {
                            return Parsed::need();
                        }
                        if (k < 0 || count != blocks.size()) {
                            return Parsed::fail(errc::corrupt, "xz: the index does not match the blocks");
                        }
                        take(size_t(k));
                        _records = count;
                        _phase = 1;
                    } else if (_phase == 1) {
                        if (_next == _records) {
                            _phase = 2;
                            continue;
                        }
                        uint64_t unpadded, uncompressed;
                        int a = read_vli(p + used, n - used, unpadded);
                        if (a == 0) {
                            return Parsed::need();
                        }
                        int b = a < 0 ? -1 : read_vli(p + used + size_t(a), n - used - size_t(a), uncompressed);
                        if (b == 0) {
                            return Parsed::need();
                        }
                        if (a < 0 || b < 0 || unpadded != blocks[_next].unpadded || uncompressed != blocks[_next].uncompressed) {
                            return Parsed::fail(errc::corrupt, "xz: the index does not match the blocks");
                        }
                        take(size_t(a + b));
                        ++_next;
                    } else if (_phase == 2) {
                        while (_size % 4) {
                            if (used == n) {
                                return Parsed::need();
                            }
                            if (p[used] != 0) {
                                return Parsed::fail(errc::corrupt, "xz: index padding not zero");
                            }
                            take(1);
                        }
                        _phase = 3;
                    } else {
                        if (n - used < 4) {
                            return Parsed::need();
                        }
                        if (le32(p + used) != _crc.value()) {
                            return Parsed::fail(errc::checksum, "xz: index CRC-32 mismatch");
                        }
                        used += 4;
                        _size += 4;
                        return Parsed::done(used);
                    }
                }
            }

        private:
            int _phase = 0;
            uint64_t _records = 0;
            uint64_t _next = 0;
            uint64_t _size = 0;
            hash::crc32 _crc;
        };
    }
}
