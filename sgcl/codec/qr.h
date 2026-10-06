//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "image.h"
#include "detail/qr_encoder.h"
#include "../core/aliases.h"
#include "../core/detail/bytes.h"
#include "../core/detail/handle_word.h"
#include "../core/expected.h"
#include "../core/make_tracked.h"
#include "../core/slice.h"
#include "../core/string.h"
#include "../core/tracked_ptr.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sgcl::codec {
    namespace detail {
        struct QrState {
            QrSymbol symbol;
        };

        // The characters of a text: code points of UTF-8, or each byte one
        // character when the text is not UTF-8
        inline std::vector<QrChar> qr_chars(std::string_view s, bool utf8) {
            std::vector<QrChar> out;
            out.reserve(s.size());
            for (size_t i = 0; i < s.size();) {
                const uint8_t c = uint8_t(s[i]);
                if (!utf8 || c < 0x80) {
                    out.push_back({c, uint32_t(i), 1});
                    ++i;
                    continue;
                }
                const int n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
                uint32_t cp = c & (0xFF >> (n + 1));
                for (int k = 1; k < n; ++k) {
                    cp = cp << 6 | (uint8_t(s[i + size_t(k)]) & 0x3F);
                }
                out.push_back({cp, uint32_t(i), uint8_t(n)});
                i += size_t(n);
            }
            return out;
        }

        inline void qr_check(const QrOptions& o) {
            if (int(o.level) > 3) {
                throw invalid_argument("sgcl::codec::qr: options.level outside the list");
            }
            if (o.min_version < 1 || o.max_version > 40 || o.min_version > o.max_version) {
                throw invalid_argument("sgcl::codec::qr: versions outside 1 <= min_version <= max_version <= 40");
            }
            if (o.mask < -1 || o.mask > 7) {
                throw invalid_argument("sgcl::codec::qr: options.mask outside -1..7");
            }
        }

        // The symbol of segments made for each group of versions: the
        // smallest version that holds them, the level raised where it
        // still does, the bit stream padded to its codewords
        inline expected<QrSymbol, error> qr_make(const QrOptions& o, auto&& segments_for) {
            int version = 0;
            std::vector<QrSegment> segs;
            long long bits = 0;
            int group = -1;
            for (int v = o.min_version; v <= o.max_version; ++v) {
                const int g = v <= 9 ? 0 : v <= 26 ? 1 : 2;
                if (g != group) {
                    segs = segments_for(v);
                    group = g;
                }
                bits = qr_bits_of(segs, v);
                if (bits >= 0 && bits <= qr_data_codewords(v, o.level) * 8LL) {
                    version = v;
                    break;
                }
            }
            if (version == 0) {
                return unexpected(error(errc::too_large, 0, "qr: more data than a symbol of max_version holds at its level"));
            }
            QrLevel level = o.level;
            if (o.boost_level) {
                for (int l = int(o.level) + 1; l <= 3; ++l) {
                    if (bits <= qr_data_codewords(version, QrLevel(l)) * 8LL) {
                        level = QrLevel(l);
                    }
                }
            }
            const size_t capacity = size_t(qr_data_codewords(version, level)) * 8;
            std::vector<bool> stream;
            stream.reserve(capacity);
            for (const auto& s : segs) {
                static constexpr uint32_t indicator[5] = {1, 2, 4, 8, 7};
                qr_append(stream, indicator[int(s.mode)], 4);
                if (s.mode != QrMode::eci) {
                    qr_append(stream, s.count, qr_count_bits(s.mode, version));
                }
                stream.insert(stream.end(), s.bits.begin(), s.bits.end());
            }
            qr_append(stream, 0, int(std::min<size_t>(4, capacity - stream.size())));   // the terminator
            qr_append(stream, 0, int((8 - stream.size() % 8) % 8));
            for (uint32_t pad = 0xEC; stream.size() < capacity; pad ^= 0xEC ^ 0x11) {
                qr_append(stream, pad, 8);
            }
            std::vector<uint8_t> data(capacity / 8, 0);
            for (size_t i = 0; i < capacity; ++i) {
                data[i >> 3] = uint8_t(data[i >> 3] | (stream[i] ? 0x80 >> (i & 7) : 0));
            }
            QrSymbol sym;
            sym.build(data, version, level, o.mask);
            return sym;
        }
    }

    // A QR code (ISO/IEC 18004:2015, Model 2): the data in the fewest bits
    // of the four modes, the smallest symbol that holds it at the level of
    // error correction asked, the mask of least penalty; its modules,
    // drawn into an image or written as SVG. Generation only.
    //
    //     codec::qr::encode("https://example.com")->to_image().save("qr.png");
    //
    // A handle of one tracked word: a copy shares the modules, which never
    // change once made.
    class qr {
    public:
        using level = detail::QrLevel;
        using options = detail::QrOptions;

        // A text, UTF-8: cut into numeric, alphanumeric, byte and Kanji
        // segments for the fewest bits, an ECI of UTF-8 in front when a
        // byte segment holds what is not ASCII; errc::too_large when a
        // symbol of max_version does not hold it. invalid_argument for
        // options outside their ranges
        static expected<qr, error> encode(const string& text, const options& o = {}) {
            detail::qr_check(o);
            const std::string_view s(text.data(), text.size());
            const bool utf8 = text.is_valid_utf8();
            const std::vector<detail::QrChar> chars = detail::qr_chars(s, utf8);
            auto made = detail::qr_make(o, [&](int v) {
                std::vector<detail::QrSegment> segs = detail::qr_segments(s, chars, v, o.kanji && utf8);
                bool wide = false;
                for (const auto& g : segs) {
                    if (g.mode == detail::QrMode::byte) {
                        for (size_t i = 0; i < g.bits.size(); i += 8) {
                            wide |= g.bits[i];   // a byte of 0x80 or more
                        }
                    }
                }
                if (wide && utf8 && o.eci) {
                    detail::QrSegment eci{detail::QrMode::eci, 0, {}};
                    detail::qr_append(eci.bits, 26, 8);   // ECI 000026: UTF-8
                    segs.insert(segs.begin(), std::move(eci));
                }
                return segs;
            });
            if (!made) {
                return unexpected(made.error());
            }
            return qr(std::move(*made));
        }

        // A literal: the text it is (a slice of bytes would take it too)
        SGCL_INLINE_HOT static expected<qr, error> encode(const char* text, const options& o = {}) {
            return encode(string(text), o);
        }

        // Bytes as they are, in one byte segment
        static expected<qr, error> encode(const slice<const byte>& data, const options& o = {}) {
            detail::qr_check(o);
            auto made = detail::qr_make(o, [&](int) {
                detail::QrSegment s{detail::QrMode::byte, uint32_t(data.size()), {}};
                s.bits.reserve(data.size() * 8);
                for (byte b : data) {
                    detail::qr_append(s.bits, uint8_t(b), 8);
                }
                return std::vector<detail::QrSegment>{std::move(s)};
            });
            if (!made) {
                return unexpected(made.error());
            }
            return qr(std::move(*made));
        }

        // Modules a side: 17 + 4 · version
        SGCL_INLINE_HOT uint32_t size() const noexcept {
            return uint32_t(_s->symbol.size);
        }

        SGCL_INLINE_HOT int version() const noexcept {
            return _s->symbol.version;
        }

        // The level written: the one asked, or higher where boost_level
        // found room for it
        SGCL_INLINE_HOT level correction() const noexcept {
            return _s->symbol.level;
        }

        SGCL_INLINE_HOT int mask() const noexcept {
            return _s->symbol.mask;
        }

        // Whether the module at column x, row y is dark
        SGCL_INLINE_HOT bool dark(uint32_t x, uint32_t y) const {
            if (x >= size() || y >= size()) {
                throw out_of_range("sgcl::codec::qr::dark: a module past the symbol");
            }
            return _s->symbol.at(int(x), int(y)) != 0;
        }

        // The symbol as gray8, each module scale × scale pixels, dark 0 and
        // light 255, with a light border of `border` modules (the quiet zone
        // the standard asks for is 4). invalid_argument for a scale of 0
        image to_image(uint32_t scale = 8, uint32_t border = 4) const {
            if (scale == 0) {
                throw invalid_argument("sgcl::codec::qr::to_image: a scale of 0");
            }
            const uint64_t side = (uint64_t(size()) + 2 * uint64_t(border)) * scale;
            if (side > 0x7FFFFFFF) {
                throw length_error("sgcl::codec::qr::to_image: larger than an image holds");
            }
            image out(uint32_t(side), uint32_t(side), pixel_format::gray8);
            const auto& sym = _s->symbol;
            std::vector<uint8_t> row(static_cast<size_t>(side));
            for (uint32_t my = 0; my < uint32_t(side / scale); ++my) {
                std::fill(row.begin(), row.end(), uint8_t(255));
                if (my >= border && my < border + size()) {
                    for (uint32_t mx = 0; mx < size(); ++mx) {
                        if (sym.at(int(mx), int(my - border))) {
                            std::fill_n(row.begin() + ptrdiff_t((mx + border) * scale), scale, uint8_t(0));
                        }
                    }
                }
                for (uint32_t k = 0; k < scale; ++k) {
                    sgcl::detail::copy_bytes(out.row(my * scale + k).data(), row.data(), row.size());
                }
            }
            return out;
        }

        // The symbol as SVG: a light square and one path of the dark
        // modules, a row's run of them a rectangle, one unit a module
        string to_svg(uint32_t border = 4) const {
            const uint64_t side = uint64_t(size()) + 2 * uint64_t(border);
            std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " + std::to_string(side) + " " + std::to_string(side) +
                            "\" shape-rendering=\"crispEdges\"><path fill=\"#ffffff\" d=\"M0 0h" + std::to_string(side) + "v" + std::to_string(side) +
                            "H0z\"/><path fill=\"#000000\" d=\"";
            const auto& sym = _s->symbol;
            for (int y = 0; y < sym.size; ++y) {
                for (int x = 0; x < sym.size;) {
                    if (!sym.at(x, y)) {
                        ++x;
                        continue;
                    }
                    int end = x;
                    while (end < sym.size && sym.at(end, y)) {
                        ++end;
                    }
                    s += "M" + std::to_string(uint64_t(x) + border) + " " + std::to_string(uint64_t(y) + border) + "h" + std::to_string(end - x) + "v1h-" +
                         std::to_string(end - x) + "z";
                    x = end;
                }
            }
            s += "\"/></svg>\n";
            return string(s.data(), s.size());
        }

    private:
        friend struct sgcl::detail::HandleWord;

        explicit qr(detail::QrSymbol&& sym)
        : _s(make_tracked<detail::QrState>()) {
            _s->symbol = std::move(sym);
        }

        SGCL_INLINE_HOT qr(sgcl::detail::FromWord, const tracked_ptr<detail::QrState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::QrState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::QrState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::QrState> _s;
    };
}
