//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// detail::copy_bytes, fill_bytes and move_bytes: every length to 9000
// bytes and the lengths on both sides of each threshold where the work
// goes from the inline ladder to the call out of line, from our own blocks
// to the base loop and to libc (and, for zero fills on arm64, to dc zva
// and back), at unaligned addresses; each run checked byte for byte and the
// guard bytes around it checked untouched. move_bytes over overlapping runs
// in both directions, against std::memmove on a copy.
#include "tests/types.h"
#include "sgcl/core/detail/bytes.h"

#include <cstdint>
#include <cstring>
#include <vector>

using sgcl::detail::copy_bytes;
using sgcl::detail::fill_bytes;
using sgcl::detail::move_bytes;

namespace {
    constexpr size_t Guard = 128;

    // Every length to 9000 (past CopyLibcFrom twice over), then the lengths
    // around the zero-line thresholds and the large ones
    std::vector<size_t> lengths() {
        std::vector<size_t> v;
        for (size_t n = 0; n <= 9000; ++n) {
            v.push_back(n);
        }
        for (size_t at : {sgcl::detail::ZeroLinesFrom, sgcl::detail::ZeroLinesBelow, size_t(65536)}) {
            for (size_t d = 0; d < 70; ++d) {
                v.push_back(at - 35 + d);
            }
        }
        for (size_t n : {size_t(5000), size_t(12345), size_t(20000), size_t(24576 + 63), size_t(100003),
                         size_t(1 << 20) + 3}) {
            v.push_back(n);
        }
        return v;
    }

    bool near(size_t n, size_t at) {
        return n + 3 >= at && n <= at + 3;
    }

    // The addresses: every offset in a 64-byte block for the short lengths,
    // the edge where copies go to libc and the edges of the zero-line range
    // (where the head and the tail of the blocks fall), a few elsewhere
    std::vector<size_t> offsets(size_t n) {
        if (n <= 300 || near(n, sgcl::detail::CopyLibcFrom) || near(n, sgcl::detail::ZeroLinesFrom)
            || near(n, sgcl::detail::ZeroLinesBelow) || n == 12345) {
            std::vector<size_t> v;
            for (size_t o = 0; o < 64; ++o) {
                v.push_back(o);
            }
            return v;
        }
        return {0, 1, 7, 15, 33, 63};
    }

    void pattern(unsigned char* p, size_t n, unsigned seed) {
        for (size_t i = 0; i < n; ++i) {
            p[i] = (unsigned char)(i * 131 + seed * 7 + (i >> 8));
        }
    }

    bool all_of(const unsigned char* p, size_t n, unsigned char v) {
        for (size_t i = 0; i < n; ++i) {
            if (p[i] != v) {
                return false;
            }
        }
        return true;
    }
}

TEST(Bytes_Tests, CopyEveryLengthAndAlignment) {
    std::vector<unsigned char> src((1 << 20) + 4096), dst((1 << 20) + 4096);
    pattern(src.data(), src.size(), 1);
    for (size_t n : lengths()) {
        for (size_t o : offsets(n)) {
            size_t so = (o * 5 + 3) % 64;
            std::memset(dst.data(), 0xC3, n + 2 * Guard + 64);
            unsigned char* d = dst.data() + Guard + o;
            const unsigned char* s = src.data() + so;
            copy_bytes(d, s, n);
            ASSERT_EQ(std::memcmp(d, s, n), 0) << "n=" << n << " offset=" << o;
            ASSERT_TRUE(all_of(dst.data(), Guard + o, 0xC3)) << "before, n=" << n << " offset=" << o;
            ASSERT_TRUE(all_of(d + n, Guard, 0xC3)) << "after, n=" << n << " offset=" << o;
        }
    }
}

TEST(Bytes_Tests, FillEveryLengthAndAlignment) {
    std::vector<unsigned char> dst((1 << 20) + 4096);
    for (unsigned char value : {(unsigned char)0, (unsigned char)' ', (unsigned char)0xA5}) {
        unsigned char guard = (unsigned char)(value ^ 0xFF);
        for (size_t n : lengths()) {
            for (size_t o : offsets(n)) {
                std::memset(dst.data(), guard, n + 2 * Guard + 64);
                unsigned char* d = dst.data() + Guard + o;
                if (value == 0) {
                    fill_bytes(d, 0, n);   // a zero the compiler sees: the zero lines when optimized
                } else {
                    fill_bytes(d, value, n);
                }
                ASSERT_TRUE(all_of(d, n, value)) << "n=" << n << " offset=" << o << " value=" << int(value);
                ASSERT_TRUE(all_of(dst.data(), Guard + o, guard)) << "before, n=" << n << " offset=" << o;
                ASSERT_TRUE(all_of(d + n, Guard, guard)) << "after, n=" << n << " offset=" << o;
            }
        }
    }
}

#if defined(SGCL_BYTES_ZVA)
// The dc zva path called directly, so that it runs unoptimized as well
// (where fill_bytes does not see a constant zero): every alignment in a
// 64-byte block, lengths from the threshold across a few blocks and up to
// where it stops
TEST(Bytes_Tests, ZeroLinesEveryAlignment) {
    std::vector<unsigned char> dst(sgcl::detail::ZeroLinesBelow + 2 * Guard + 64);
    std::vector<size_t> ns;
    for (size_t d = 0; d < 130; ++d) {
        ns.push_back(sgcl::detail::ZeroLinesFrom + d);
    }
    for (size_t d = 1; d < 70; ++d) {
        ns.push_back(sgcl::detail::ZeroLinesBelow - d);
    }
    ns.push_back(20000);
    for (size_t n : ns) {
        for (size_t o = 0; o < 64; ++o) {
            std::memset(dst.data(), 0x5A, n + 2 * Guard + 64);
            unsigned char* d = dst.data() + Guard + o;
            sgcl::detail::zero_lines(d, n);
            ASSERT_TRUE(all_of(d, n, 0)) << "n=" << n << " offset=" << o;
            ASSERT_TRUE(all_of(dst.data(), Guard + o, 0x5A)) << "before, n=" << n << " offset=" << o;
            ASSERT_TRUE(all_of(d + n, Guard, 0x5A)) << "after, n=" << n << " offset=" << o;
        }
    }
}

// The zero fill out of line called directly, so that it runs unoptimized
// as well: every length from where the call starts across its blocks, the
// edges of the zero lines, libc above
TEST(Bytes_Tests, ZeroLongEveryLength) {
    std::vector<unsigned char> dst(65536 + 2 * Guard + 64);
    std::vector<size_t> ns;
    for (size_t n = 32; n <= 300; ++n) {
        ns.push_back(n);
    }
    for (size_t at : {sgcl::detail::ZeroLinesFrom, sgcl::detail::ZeroLinesBelow}) {
        for (size_t d = 0; d < 8; ++d) {
            ns.push_back(at - 4 + d);
        }
    }
    ns.push_back(65536);
    for (size_t n : ns) {
        for (size_t o = 0; o < 64; ++o) {
            std::memset(dst.data(), 0x5A, n + 2 * Guard + 64);
            unsigned char* d = dst.data() + Guard + o;
            sgcl::detail::zero_long(d, n);
            ASSERT_TRUE(all_of(d, n, 0)) << "n=" << n << " offset=" << o;
            ASSERT_TRUE(all_of(dst.data(), Guard + o, 0x5A)) << "before, n=" << n << " offset=" << o;
            ASSERT_TRUE(all_of(d + n, Guard, 0x5A)) << "after, n=" << n << " offset=" << o;
        }
    }
}
#endif

TEST(Bytes_Tests, MoveOverlappingBothWays) {
    // Every length to 300 by every shift either way (the own blocks end at
    // MoveOwnUpTo), then longer runs at a few shifts
    std::vector<unsigned char> buf(9400 + 2 * Guard), ref(9400 + 2 * Guard);
    auto check = [&](size_t n, size_t from, size_t to) {
        pattern(buf.data(), buf.size(), unsigned(n + from));
        ref = buf;
        std::memmove(ref.data() + to, ref.data() + from, n);
        move_bytes(buf.data() + to, buf.data() + from, n);
        ASSERT_EQ(buf, ref) << "n=" << n << " from=" << from << " to=" << to;
    };
    for (size_t n = 0; n <= 300; ++n) {
        for (size_t shift = 0; shift <= n + 1; ++shift) {
            check(n, Guard, Guard + shift);
            check(n, Guard + shift, Guard);
        }
    }
    for (size_t n : {size_t(1000), size_t(4095), size_t(4096), size_t(4097), size_t(7777), size_t(9000)}) {
        for (size_t shift : {size_t(1), size_t(8), size_t(63), size_t(64), size_t(129), size_t(300)}) {
            check(n, Guard, Guard + shift);
            check(n, Guard + shift, Guard);
        }
    }
}

TEST(Bytes_Tests, MoveDisjointEveryLength) {
    std::vector<unsigned char> src(9000 + 64), dst(9000 + 2 * Guard + 64);
    pattern(src.data(), src.size(), 9);
    for (size_t n = 0; n <= 9000; ++n) {
        for (size_t o : {size_t(0), size_t(1), size_t(13), size_t(31)}) {
            std::memset(dst.data(), 0x3C, dst.size());
            unsigned char* d = dst.data() + Guard + o;
            move_bytes(d, src.data() + (o * 3) % 32, n);
            ASSERT_EQ(std::memcmp(d, src.data() + (o * 3) % 32, n), 0) << "n=" << n;
            ASSERT_TRUE(all_of(dst.data(), Guard + o, 0x3C)) << "before, n=" << n;
            ASSERT_TRUE(all_of(d + n, Guard, 0x3C)) << "after, n=" << n;
        }
    }
}
