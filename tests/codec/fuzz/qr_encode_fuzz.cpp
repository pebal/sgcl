//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The QR encoder (qr.h) on any input: the first two bytes pick the options
// (level, boost, Kanji, ECI, a mask or none, a range of versions), the
// third whether the rest is a text or bytes. What is made must be a
// symbol of a version in the range, of 17 + 4v modules a side; its two
// copies of the format information equal and a valid BCH word naming the
// level and mask the symbol reports; its finders in place; the image and
// the SVG of the same modules; the same input the same symbol. A refusal
// is only too_large, and only where the range's last version is short of
// 40 or the input is long. The bytes are encoded from libFuzzer's own
// buffer (ASan sees a read past it).
#include "sgcl/codec/codec.h"

#include <cstdlib>
#include <cstring>

namespace {
    using namespace sgcl;

    void check(bool ok) {
        if (!ok) {
            std::abort();
        }
    }

    uint32_t format_at(const codec::qr& q, bool copy) {
        const uint32_t n = q.size();
        uint32_t v = 0;
        if (!copy) {
            for (int i = 14; i >= 9; --i) {
                v = v << 1 | q.dark(uint32_t(14 - i), 8);
            }
            v = v << 1 | q.dark(7, 8);
            v = v << 1 | q.dark(8, 8);
            v = v << 1 | q.dark(8, 7);
            for (int i = 5; i >= 0; --i) {
                v = v << 1 | q.dark(8, uint32_t(i));
            }
        } else {
            for (int i = 14; i >= 8; --i) {
                v = v << 1 | q.dark(8, n - 15 + uint32_t(i));
            }
            for (int i = 7; i >= 0; --i) {
                v = v << 1 | q.dark(n - 1 - uint32_t(i), 8);
            }
        }
        return v;
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 3) {
        return 0;
    }
    codec::qr::options o;
    o.level = codec::qr::level(data[0] & 3);
    o.boost_level = (data[0] >> 2) & 1;
    o.kanji = (data[0] >> 3) & 1;
    o.eci = (data[0] >> 4) & 1;
    o.mask = (data[0] >> 5) == 0 ? -1 : int(data[0] >> 5);
    o.min_version = 1 + data[1] % 40;
    o.max_version = o.min_version + (data[1] >> 6) * 13 > 40 ? 40 : o.min_version + (data[1] >> 6) * 13;
    const bool text = data[2] & 1;
    const slice<const byte> rest(reinterpret_cast<const byte*>(data + 3), size - 3);
    auto q = text ? codec::qr::encode(string(reinterpret_cast<const char*>(data + 3), size - 3), o) : codec::qr::encode(rest, o);
    if (!q) {
        check(q.error().code() == codec::errc::too_large);
        check(o.max_version < 40 || size > 1000);
        return 0;
    }
    const int v = q->version();
    check(v >= o.min_version && v <= o.max_version);
    check(q->size() == uint32_t(17 + 4 * v));
    check(int(q->correction()) >= int(o.level) && (o.boost_level || q->correction() == o.level));
    check(o.mask < 0 || q->mask() == o.mask);
    // the format information: both copies, a BCH (15, 5) word of the level and mask
    const uint32_t f = format_at(*q, false);
    check(f == format_at(*q, true));
    static constexpr uint32_t level_bits[4] = {1, 0, 3, 2};
    const uint32_t word = level_bits[int(q->correction())] << 3 | uint32_t(q->mask());
    uint32_t rem = word;
    for (int i = 0; i < 10; ++i) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    check(f == (((word << 10) | rem) ^ 0x5412));
    // the finders' corners and the dark module
    const uint32_t n = q->size();
    check(q->dark(0, 0) && q->dark(n - 1, 0) && q->dark(0, n - 1) && !q->dark(n - 1, 7) && q->dark(8, n - 8));
    // the image of the modules
    const codec::image im = q->to_image(1, 1);
    for (uint32_t y = 0; y < n; y += 7) {
        for (uint32_t x = 0; x < n; x += 5) {
            check((uint8_t(im.row(y + 1)[x + 1]) == 0) == q->dark(x, y));
        }
    }
    const string svg = q->to_svg(0);
    check(svg.size() > 7 && std::memcmp(svg.data() + svg.size() - 7, "</svg>\n", 7) == 0);
    // the same input, the same symbol
    auto again = text ? codec::qr::encode(string(reinterpret_cast<const char*>(data + 3), size - 3), o) : codec::qr::encode(rest, o);
    check(again && again->size() == n && again->mask() == q->mask());
    for (uint32_t y = 0; y < n; ++y) {
        for (uint32_t x = 0; x < n; ++x) {
            check(again->dark(x, y) == q->dark(x, y));
        }
    }
    return 0;
}
