//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "qr_kanji.h"
#include "../../core/detail/os.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>
#include <vector>

// QR code generation, ISO/IEC 18004:2015 (Model 2): the data cut into
// segments of the four modes, the bit stream, Reed-Solomon over GF(256)
// in blocks interleaved, the symbol's function patterns, the data placed
// in its zigzag, the eight masks scored by the four penalty rules, the
// format and version information with their BCH codes.
namespace sgcl::codec::detail {
    enum class QrLevel : uint8_t { low, medium, quartile, high };

    struct QrOptions {
        QrLevel level = QrLevel::medium;
        int min_version = 1;
        int max_version = 40;
        int mask = -1;
        bool boost_level = true;
        bool kanji = true;
        bool eci = true;
    };

    enum class QrMode : uint8_t { numeric, alphanumeric, byte, kanji, eci };

    // A segment: its mode, how many characters (bytes for byte mode) and
    // its data bits
    struct QrSegment {
        QrMode mode;
        uint32_t count;
        std::vector<bool> bits;
    };

    struct QrTables {
        // The error correction codewords of a block, and the number of
        // blocks, by level (L, M, Q, H) and version (ISO/IEC 18004 table 9)
        static constexpr int8_t ecc_per_block[4][41] = {
            {-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
             28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
            {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
             26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
            {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
             28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
            {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
             30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
        };
        static constexpr int8_t blocks[4][41] = {
            {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8,
             8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
            {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
             17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
            {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
             23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
            {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
             25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
        };
    };

    // The modules of a version that hold data and error correction: the
    // whole square less the function patterns (6.4.10)
    SGCL_INLINE_HOT constexpr int qr_raw_modules(int v) noexcept {
        int n = (16 * v + 128) * v + 64;
        if (v >= 2) {
            const int align = v / 7 + 2;
            n -= (25 * align - 10) * align - 55;
            if (v >= 7) {
                n -= 36;
            }
        }
        return n;
    }

    // The data codewords of a version at a level
    SGCL_INLINE_HOT constexpr int qr_data_codewords(int v, QrLevel l) noexcept {
        return qr_raw_modules(v) / 8 - QrTables::ecc_per_block[int(l)][v] * QrTables::blocks[int(l)][v];
    }

    // The bits of a segment's character count, by mode and version group
    SGCL_INLINE_HOT constexpr int qr_count_bits(QrMode m, int v) noexcept {
        const int g = v <= 9 ? 0 : v <= 26 ? 1 : 2;
        switch (m) {
            case QrMode::numeric: return 10 + 2 * g;
            case QrMode::alphanumeric: return 9 + 2 * g;
            case QrMode::byte: return g == 0 ? 8 : 16;
            case QrMode::kanji: return 8 + 2 * g;
            default: return 0;
        }
    }

    // The value of a character in alphanumeric mode, -1 for one it lacks
    SGCL_INLINE_HOT constexpr int qr_alnum(uint32_t c) noexcept {
        if (c >= '0' && c <= '9') {
            return int(c - '0');
        }
        if (c >= 'A' && c <= 'Z') {
            return int(c - 'A' + 10);
        }
        switch (c) {
            case ' ': return 36;
            case '$': return 37;
            case '%': return 38;
            case '*': return 39;
            case '+': return 40;
            case '-': return 41;
            case '.': return 42;
            case '/': return 43;
            case ':': return 44;
            default: return -1;
        }
    }

    // The 13-bit value of a character in Kanji mode, -1 for one it lacks
    inline int qr_kanji(uint32_t c) noexcept {
        const auto* first = std::begin(QrKanjiTable);
        const auto* last = std::end(QrKanjiTable);
        const auto* it = std::lower_bound(first, last, c, [](const QrKanji& k, uint32_t v) { return k.code_point < v; });
        return it != last && it->code_point == c ? int(it->value) : -1;
    }

    SGCL_INLINE_HOT void qr_append(std::vector<bool>& bits, uint32_t value, int n) {
        for (int i = n - 1; i >= 0; --i) {
            bits.push_back(((value >> i) & 1) != 0);
        }
    }

    // A character of the text: its code point and the bytes it takes
    struct QrChar {
        uint32_t code_point;
        uint32_t at;       // its first byte in the text
        uint8_t length;    // its bytes in UTF-8 (or 1, for text that is not UTF-8)
    };

    // The segments of a text for versions of one group, the fewest bits:
    // a dynamic programme over the characters, the cost of each mode's
    // characters in sixths of a bit (numeric 10/3 bits, alphanumeric 11/2,
    // byte 8 a byte, Kanji 13), a segment's header (4 bits and its count)
    // where a mode starts, a numeric or alphanumeric segment's fraction
    // rounded up where it ends
    inline std::vector<QrSegment> qr_segments(const std::string_view& text, const std::vector<QrChar>& chars, int version, bool kanji) {
        const size_t n = chars.size();
        constexpr int Modes = 4;
        constexpr uint32_t Infinite = 0x3FFFFFFF;
        std::vector<std::array<uint32_t, Modes>> cost(n);
        std::vector<std::array<uint8_t, Modes>> from(n);
        uint32_t header[Modes];
        for (int m = 0; m < Modes; ++m) {
            header[m] = uint32_t(4 + qr_count_bits(QrMode(m), version)) * 6;
        }
        for (size_t i = 0; i < n; ++i) {
            const uint32_t c = chars[i].code_point;
            uint32_t each[Modes] = {Infinite, Infinite, uint32_t(chars[i].length) * 48, Infinite};
            if (c >= '0' && c <= '9') {
                each[0] = 20;
            }
            if (qr_alnum(c) >= 0) {
                each[1] = 33;
            }
            if (kanji && chars[i].length > 1 && qr_kanji(c) >= 0) {
                each[3] = 78;
            }
            for (int m = 0; m < Modes; ++m) {
                if (each[m] == Infinite) {
                    cost[i][m] = Infinite;
                    continue;
                }
                if (i == 0) {
                    cost[i][m] = header[m] + each[m];
                    from[i][m] = uint8_t(m);
                    continue;
                }
                uint32_t best = Infinite;
                uint8_t via = 0;
                for (int k = 0; k < Modes; ++k) {
                    if (cost[i - 1][k] == Infinite) {
                        continue;
                    }
                    // the same mode goes on; another ends here, its bits
                    // whole, and this one starts with its header
                    const uint32_t c2 = k == m ? cost[i - 1][k] : (cost[i - 1][k] + 5) / 6 * 6 + header[m];
                    if (c2 < best) {
                        best = c2;
                        via = uint8_t(k);
                    }
                }
                cost[i][m] = best + each[m];
                from[i][m] = via;
            }
        }
        std::vector<QrSegment> out;
        if (n == 0) {
            return out;
        }
        // the cheapest end, then back to the start
        int m = 0;
        for (int k = 1; k < Modes; ++k) {
            if ((cost[n - 1][k] + 5) / 6 < (cost[n - 1][m] + 5) / 6) {
                m = k;
            }
        }
        std::vector<uint8_t> modes(n);
        for (size_t i = n; i-- > 0;) {
            modes[i] = uint8_t(m);
            m = from[i][m];
        }
        // the runs of one mode, encoded
        for (size_t i = 0; i < n;) {
            size_t j = i;
            while (j < n && modes[j] == modes[i]) {
                ++j;
            }
            QrSegment s{QrMode(modes[i]), 0, {}};
            switch (s.mode) {
                case QrMode::numeric:
                    s.count = uint32_t(j - i);
                    for (size_t k = i; k < j; k += 3) {
                        const size_t len = std::min<size_t>(3, j - k);
                        uint32_t v = 0;
                        for (size_t t = 0; t < len; ++t) {
                            v = v * 10 + (chars[k + t].code_point - '0');
                        }
                        qr_append(s.bits, v, int(len * 3 + 1));
                    }
                    break;
                case QrMode::alphanumeric:
                    s.count = uint32_t(j - i);
                    for (size_t k = i; k < j; k += 2) {
                        if (k + 1 < j) {
                            qr_append(s.bits, uint32_t(qr_alnum(chars[k].code_point) * 45 + qr_alnum(chars[k + 1].code_point)), 11);
                        } else {
                            qr_append(s.bits, uint32_t(qr_alnum(chars[k].code_point)), 6);
                        }
                    }
                    break;
                case QrMode::byte:
                    for (size_t k = i; k < j; ++k) {
                        for (uint32_t t = 0; t < chars[k].length; ++t) {
                            qr_append(s.bits, uint8_t(text[chars[k].at + t]), 8);
                            ++s.count;
                        }
                    }
                    break;
                default:
                    s.count = uint32_t(j - i);
                    for (size_t k = i; k < j; ++k) {
                        qr_append(s.bits, uint32_t(qr_kanji(chars[k].code_point)), 13);
                    }
                    break;
            }
            out.push_back(std::move(s));
            i = j;
        }
        return out;
    }

    // The bits segments take at a version: their headers and data; -1 for
    // a count past what its field holds
    inline long long qr_bits_of(const std::vector<QrSegment>& segs, int version) noexcept {
        long long n = 0;
        for (const auto& s : segs) {
            if (s.mode == QrMode::eci) {
                n += 4 + static_cast<long long>(s.bits.size());
                continue;
            }
            const int cb = qr_count_bits(s.mode, version);
            if (s.count >= (1u << cb)) {
                return -1;
            }
            n += 4 + cb + static_cast<long long>(s.bits.size());
        }
        return n;
    }

    // ------------------------------------------------------------------
    // Reed-Solomon over GF(2^8), the polynomial x^8 + x^4 + x^3 + x^2 + 1

    // Powers of the generator 2 and their logarithms
    struct QrGf {
        uint8_t exp[512] = {};
        uint8_t log[256] = {};

        constexpr QrGf() noexcept {
            uint32_t v = 1;
            for (int i = 0; i < 255; ++i) {
                exp[i] = uint8_t(v);
                log[v] = uint8_t(i);
                v <<= 1;
                if (v & 0x100) {
                    v ^= 0x11D;
                }
            }
            for (int i = 255; i < 512; ++i) {
                exp[i] = exp[i - 255];
            }
        }
    };

    inline constexpr QrGf QrGfTables{};

    SGCL_INLINE_HOT uint8_t qr_gf_mul(uint8_t a, uint8_t b) noexcept {
        if (a == 0 || b == 0) {
            return 0;
        }
        return QrGfTables.exp[QrGfTables.log[a] + QrGfTables.log[b]];
    }

    // The generator polynomial of degree n, its coefficients from the
    // highest power down, the leading 1 left out
    inline std::vector<uint8_t> qr_rs_generator(int n) {
        std::vector<uint8_t> g(size_t(n), 0);
        g[size_t(n) - 1] = 1;
        uint8_t root = 1;
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                g[size_t(j)] = qr_gf_mul(g[size_t(j)], root);
                if (j + 1 < n) {
                    g[size_t(j)] ^= g[size_t(j) + 1];
                }
            }
            root = qr_gf_mul(root, 0x02);
        }
        return g;
    }

    inline void qr_rs_remainder(const uint8_t* data, size_t n, const std::vector<uint8_t>& g, uint8_t* out) noexcept {
        const size_t d = g.size();
        std::fill(out, out + d, uint8_t(0));
        for (size_t i = 0; i < n; ++i) {
            const uint8_t factor = data[i] ^ out[0];
            std::copy(out + 1, out + d, out);
            out[d - 1] = 0;
            for (size_t j = 0; j < d; ++j) {
                out[j] ^= qr_gf_mul(g[j], factor);
            }
        }
    }

    // ------------------------------------------------------------------
    // The symbol

    class QrSymbol {
    public:
        int version = 1;
        QrLevel level = QrLevel::medium;
        int mask = 0;
        int size = 21;
        std::vector<uint8_t> dark;   // size × size, row by row

        // The symbol of the data codewords (padded to the version's
        // capacity) at a version and level, the mask given or chosen (-1)
        void build(const std::vector<uint8_t>& data, int v, QrLevel l, int forced_mask) {
            version = v;
            level = l;
            size = 17 + 4 * v;
            dark.assign(size_t(size) * size, 0);
            _function.assign(size_t(size) * size, 0);
            _function_patterns();
            const std::vector<uint8_t> all = _interleaved(data);
            _place(all);
            // the data modules and where each falls in the masks' period
            // (x mod 6, y mod 12 decide every pattern)
            _data.clear();
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    const size_t at = size_t(y) * size + size_t(x);
                    if (!_function[at]) {
                        _data.push_back(uint32_t(at) << 7 | uint32_t(y % 12) << 3 | uint32_t(x % 6));
                    }
                }
            }
            if (forced_mask >= 0) {
                mask = forced_mask;
            } else {
                const std::vector<uint8_t> unmasked = dark;
                long best = -1;
                for (int m = 0; m < 8; ++m) {
                    _apply_mask(m);
                    _format(m);
                    const long p = penalty();
                    if (best < 0 || p < best) {
                        best = p;
                        mask = m;
                    }
                    dark = unmasked;
                }
            }
            _apply_mask(mask);
            _format(mask);
        }

        // The four penalty rules of 7.8.3 over the masked symbol: N1 runs of
        // five or more modules of one color in a row or a column, 3 and 1
        // for each module past five; N2 blocks of 2 x 2 of one color, 3
        // each; N3 the finder's 1:1:3:1:1 with four light modules of the
        // symbol on either side (the quiet zone not counted, as the
        // standard draws the pattern and as CoreImage's generator scores
        // it), 40 each; N4
        // the share of dark modules, 10 for every 5 % away from half
        long penalty() const {
            const size_t n = size_t(size);
            // the columns as rows, so that both passes read memory in order
            std::vector<uint8_t> columns(n * n);
            for (size_t y = 0; y < n; ++y) {
                for (size_t x = 0; x < n; ++x) {
                    columns[x * n + y] = dark[y * n + x];
                }
            }
            long result = 0;
            const std::vector<uint8_t>* passes[2] = {&dark, &columns};
            for (const std::vector<uint8_t>* lines : passes) {
                for (size_t a = 0; a < n; ++a) {
                    const uint8_t* line = lines->data() + a * n;
                    // N1: runs; N3 in a window of the last 15 modules, the
                    // newest in bit 0: a core 1011101 that ends 4 modules
                    // back with 0000 after it, or one that ends here with
                    // 0000 before it, once when both
                    uint32_t window = 0;
                    size_t run = 1;
                    for (size_t i = 0; i < n; ++i) {
                        const uint8_t c = line[i];
                        if (i > 0) {
                            if (c == line[i - 1]) {
                                ++run;
                            } else {
                                if (run >= 5) {
                                    result += long(run) - 2;
                                }
                                run = 1;
                            }
                        }
                        window = (window << 1 | c) & 0x7FFF;
                        // the core ending here, light before it
                        const bool before = i >= 10 && (window & 0x7FF) == 0x05D;
                        // the core ending 4 back, light after it
                        const bool after = i >= 10 && (window & 0x7FF) == 0x5D0;
                        const bool both = i >= 14 && window == 0x05D0;
                        result += (long(before) + long(after) - long(both)) * 40;
                    }
                    if (run >= 5) {
                        result += long(run) - 2;
                    }
                }
            }
            // N2: blocks of 2 x 2 of one color
            for (size_t y = 0; y + 1 < n; ++y) {
                const uint8_t* r0 = dark.data() + y * n;
                const uint8_t* r1 = r0 + n;
                for (size_t x = 0; x + 1 < n; ++x) {
                    const unsigned sum = unsigned(r0[x]) + r0[x + 1] + r1[x] + r1[x + 1];
                    result += (sum == 0 || sum == 4) ? 3 : 0;
                }
            }
            // N4
            long dark_count = 0;
            for (uint8_t d : dark) {
                dark_count += d;
            }
            const long total = long(n * n);
            result += std::labs(dark_count * 20 - total * 10) / total * 10;
            return result;
        }

        SGCL_INLINE_HOT int at(int x, int y) const noexcept {
            return dark[size_t(y) * size + size_t(x)];
        }

    private:
        std::vector<uint8_t> _function;
        std::vector<uint32_t> _data;   // the data modules: index << 7 | (y mod 12) << 3 | x mod 6

        SGCL_INLINE_HOT void _set(int x, int y, bool d) noexcept {
            dark[size_t(y) * size + size_t(x)] = d ? 1 : 0;
            _function[size_t(y) * size + size_t(x)] = 1;
        }

        void _finder(int cx, int cy) noexcept {
            for (int dy = -4; dy <= 4; ++dy) {
                for (int dx = -4; dx <= 4; ++dx) {
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= size || y >= size) {
                        continue;
                    }
                    const int dist = std::max(std::abs(dx), std::abs(dy));
                    _set(x, y, dist != 2 && dist != 4);
                }
            }
        }

        void _alignment(int cx, int cy) noexcept {
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    _set(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
                }
            }
        }

        // The centres of the alignment patterns on one axis (annex E)
        std::vector<int> _alignment_positions() const {
            std::vector<int> out;
            if (version == 1) {
                return out;
            }
            const int count = version / 7 + 2;
            const int step = (version * 8 + count * 3 + 5) / (count * 4 - 4) * 2;
            out.resize(size_t(count));
            out[0] = 6;
            for (int i = count - 1, pos = size - 7; i >= 1; --i, pos -= step) {
                out[size_t(i)] = pos;
            }
            return out;
        }

        void _function_patterns() {
            // timing patterns
            for (int i = 0; i < size; ++i) {
                _set(6, i, i % 2 == 0);
                _set(i, 6, i % 2 == 0);
            }
            // finders with their separators
            _finder(3, 3);
            _finder(size - 4, 3);
            _finder(3, size - 4);
            // alignment patterns, but where a finder is
            const std::vector<int> pos = _alignment_positions();
            const size_t n = pos.size();
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = 0; j < n; ++j) {
                    if ((i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0)) {
                        continue;
                    }
                    _alignment(pos[i], pos[j]);
                }
            }
            // the format areas reserved (written last) and the version
            _format(0);
            _version();
        }

        // The format information: the level's two bits and the mask's
        // three, BCH (15, 5), XOR 101010000010010 (7.9)
        void _format(int m) noexcept {
            static constexpr int level_bits[4] = {1, 0, 3, 2};
            const uint32_t data = uint32_t(level_bits[int(level)]) << 3 | uint32_t(m);
            uint32_t rem = data;
            for (int i = 0; i < 10; ++i) {
                rem = (rem << 1) ^ ((rem >> 9) * 0x537);
            }
            const uint32_t bits = ((data << 10) | rem) ^ 0x5412;
            auto bit = [&](int i) { return ((bits >> i) & 1) != 0; };
            // around the top left finder
            for (int i = 0; i <= 5; ++i) {
                _set(8, i, bit(i));
            }
            _set(8, 7, bit(6));
            _set(8, 8, bit(7));
            _set(7, 8, bit(8));
            for (int i = 9; i < 15; ++i) {
                _set(14 - i, 8, bit(i));
            }
            // the copy beside the other two
            for (int i = 0; i < 8; ++i) {
                _set(size - 1 - i, 8, bit(i));
            }
            for (int i = 8; i < 15; ++i) {
                _set(8, size - 15 + i, bit(i));
            }
            _set(8, size - 8, true);   // the dark module
        }

        // The version information of versions 7 and up: BCH (18, 6) (7.10)
        void _version() noexcept {
            if (version < 7) {
                return;
            }
            uint32_t rem = uint32_t(version);
            for (int i = 0; i < 12; ++i) {
                rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
            }
            const uint32_t bits = uint32_t(version) << 12 | rem;
            for (int i = 0; i < 18; ++i) {
                const bool b = ((bits >> i) & 1) != 0;
                const int a = size - 11 + i % 3, c = i / 3;
                _set(a, c, b);
                _set(c, a, b);
            }
        }

        // The data codewords cut into blocks, each with its error
        // correction, interleaved (7.6)
        std::vector<uint8_t> _interleaved(const std::vector<uint8_t>& data) const {
            const int blocks = QrTables::blocks[int(level)][version];
            const int ecc = QrTables::ecc_per_block[int(level)][version];
            const int raw = qr_raw_modules(version) / 8;
            const int short_blocks = blocks - raw % blocks;
            const int short_len = raw / blocks;   // codewords of a short block, ecc included
            const std::vector<uint8_t> g = qr_rs_generator(ecc);
            std::vector<std::vector<uint8_t>> parts(static_cast<size_t>(blocks));
            size_t k = 0;
            for (int i = 0; i < blocks; ++i) {
                const size_t len = size_t(short_len - ecc + (i < short_blocks ? 0 : 1));
                std::vector<uint8_t>& b = parts[size_t(i)];
                b.assign(data.begin() + ptrdiff_t(k), data.begin() + ptrdiff_t(k + len));
                k += len;
                std::vector<uint8_t> e(static_cast<size_t>(ecc));
                qr_rs_remainder(b.data(), b.size(), g, e.data());
                if (i < short_blocks) {
                    b.push_back(0);   // a gap where the long blocks have one more data codeword
                }
                b.insert(b.end(), e.begin(), e.end());
            }
            std::vector<uint8_t> out;
            out.reserve(size_t(raw));
            for (size_t i = 0; i < parts[0].size(); ++i) {
                for (int j = 0; j < blocks; ++j) {
                    if (i != size_t(short_len - ecc) || j >= short_blocks) {
                        out.push_back(parts[size_t(j)][i]);
                    }
                }
            }
            return out;
        }

        // The codewords into the modules that are not function patterns:
        // pairs of columns from the right, upward then downward, column 6
        // skipped (7.7.3); the remainder bits light
        void _place(const std::vector<uint8_t>& codewords) noexcept {
            size_t i = 0;
            const size_t total = codewords.size() * 8;
            for (int right = size - 1; right >= 1; right -= 2) {
                if (right == 6) {
                    right = 5;
                }
                for (int v = 0; v < size; ++v) {
                    for (int j = 0; j < 2; ++j) {
                        const int x = right - j;
                        const bool upward = ((right + 1) & 2) == 0;
                        const int y = upward ? size - 1 - v : v;
                        if (_function[size_t(y) * size + size_t(x)]) {
                            continue;
                        }
                        if (i < total) {
                            dark[size_t(y) * size + size_t(x)] = (codewords[i >> 3] >> (7 - (i & 7))) & 1;
                            ++i;
                        }
                    }
                }
            }
        }

        // Whether mask m flips the module at column x, row y (7.8.2)
        static constexpr bool _flips(int m, int x, int y) noexcept {
            switch (m) {
                case 0: return (x + y) % 2 == 0;
                case 1: return y % 2 == 0;
                case 2: return x % 3 == 0;
                case 3: return (x + y) % 3 == 0;
                case 4: return (x / 3 + y / 2) % 2 == 0;
                case 5: return x * y % 2 + x * y % 3 == 0;
                case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
                default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
            }
        }

        // The eight patterns over their period: row y mod 12, column x mod 6
        struct MaskTable {
            uint8_t flips[8][12 * 8] = {};

            constexpr MaskTable() noexcept {
                for (int m = 0; m < 8; ++m) {
                    for (int y = 0; y < 12; ++y) {
                        for (int x = 0; x < 6; ++x) {
                            flips[m][y << 3 | x] = _flips(m, x, y) ? 1 : 0;
                        }
                    }
                }
            }
        };

        // The mask's pattern XORed into the data modules
        void _apply_mask(int m) noexcept {
            static constexpr MaskTable table;
            const uint8_t* f = table.flips[m];
            for (uint32_t d : _data) {
                dark[d >> 7] ^= f[d & 0x7F];
            }
        }
    };
}
