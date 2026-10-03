//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/bytes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace sgcl::codec {
    // How the pixels of an image lie in its rows: the channels in the
    // order of the name, each a byte (…8) or a 16-bit value in the byte
    // order of the machine (…16), with no padding between the pixels or
    // at the end of a row. Alpha is straight (not premultiplied), as PNG,
    // GIF and WebP store it. cmyk8 is what an Adobe CMYK or YCCK JPEG
    // holds: the ink of each channel, 0 for none and 255 for full.
    enum class pixel_format : uint8_t {
        gray8,
        gray_alpha8,
        rgb8,
        rgba8,
        gray16,
        gray_alpha16,
        rgb16,
        rgba16,
        cmyk8
    };
}

namespace sgcl::codec::detail {
    inline constexpr unsigned PixelFormatCount = 9;

    constexpr bool valid(pixel_format f) noexcept {
        return static_cast<unsigned>(f) < PixelFormatCount;
    }

    constexpr unsigned channels(pixel_format f) noexcept {
        switch (f) {
            case pixel_format::gray8: case pixel_format::gray16: return 1;
            case pixel_format::gray_alpha8: case pixel_format::gray_alpha16: return 2;
            case pixel_format::rgb8: case pixel_format::rgb16: return 3;
            case pixel_format::rgba8: case pixel_format::rgba16: case pixel_format::cmyk8: return 4;
        }
        return 0;
    }

    constexpr bool wide(pixel_format f) noexcept {
        return f >= pixel_format::gray16 && f <= pixel_format::rgba16;
    }

    constexpr bool gray(pixel_format f) noexcept {
        return f == pixel_format::gray8 || f == pixel_format::gray_alpha8 || f == pixel_format::gray16 || f == pixel_format::gray_alpha16;
    }

    constexpr bool alpha(pixel_format f) noexcept {
        return f == pixel_format::gray_alpha8 || f == pixel_format::rgba8 || f == pixel_format::gray_alpha16 || f == pixel_format::rgba16;
    }

    constexpr unsigned bytes_per_pixel(pixel_format f) noexcept {
        return channels(f) * (wide(f) ? 2 : 1);
    }

    // 8 bits to 16 and back: v * 257 is exact (255 to 65535), the way
    // back the nearest 8-bit value, round(v / 257)
    constexpr uint16_t widen(uint8_t v) noexcept {
        return static_cast<uint16_t>(v * 257u);
    }

    constexpr uint8_t narrow(uint16_t v) noexcept {
        return static_cast<uint8_t>((v * 255u + 32895u) >> 16);
    }

    // The luma of Rec. 601 (0.299, 0.587, 0.114 in 16-bit fixed point,
    // the weights summing to 65536 so that a gray pixel keeps its value),
    // for 8- and 16-bit channels alike: 65535 * 65536 + 32768 fits 32 bits
    template<class T>
    constexpr T luma(T r, T g, T b) noexcept {
        return static_cast<T>((19595u * r + 38470u * g + 7471u * b + 32768u) >> 16);
    }

    // CMYK to RGB without a profile: each channel is what the ink and the
    // black leave of the white, (1 - c)(1 - k), to the nearest value; and
    // the way back, black as what the brightest channel lacks
    constexpr uint8_t cmyk_channel(uint8_t ink, uint8_t k) noexcept {
        return static_cast<uint8_t>(((255u - ink) * (255u - k) + 127u) / 255u);
    }

    template<class T>
    struct Rgba {
        T r, g, b, a;
    };

    template<class T>
    constexpr T channel_max = std::is_same_v<T, uint8_t> ? T(255) : T(65535);

    // A channel of the source at the depth T of the conversion
    template<class T>
    inline T read_channel8(const std::byte* p) noexcept {
        auto v = static_cast<uint8_t>(*p);
        if constexpr (std::is_same_v<T, uint8_t>) {
            return v;
        } else {
            return widen(v);
        }
    }

    template<class T>
    inline T read_channel16(const std::byte* p) noexcept {
        uint16_t v;
        std::memcpy(&v, p, 2);
        if constexpr (std::is_same_v<T, uint16_t>) {
            return v;
        } else {
            return narrow(v);
        }
    }

    template<class T>
    inline void write_channel8(std::byte* p, T v) noexcept {
        if constexpr (std::is_same_v<T, uint8_t>) {
            *p = static_cast<std::byte>(v);
        } else {
            *p = static_cast<std::byte>(narrow(v));
        }
    }

    template<class T>
    inline void write_channel16(std::byte* p, T v) noexcept {
        uint16_t w;
        if constexpr (std::is_same_v<T, uint16_t>) {
            w = v;
        } else {
            w = widen(v);
        }
        std::memcpy(p, &w, 2);
    }

    template<pixel_format S, class T>
    inline Rgba<T> load_pixel(const std::byte* p) noexcept {
        constexpr T full = channel_max<T>;
        if constexpr (S == pixel_format::cmyk8) {
            auto k = static_cast<uint8_t>(p[3]);
            uint8_t r = cmyk_channel(static_cast<uint8_t>(p[0]), k);
            uint8_t g = cmyk_channel(static_cast<uint8_t>(p[1]), k);
            uint8_t b = cmyk_channel(static_cast<uint8_t>(p[2]), k);
            if constexpr (std::is_same_v<T, uint8_t>) {
                return {r, g, b, full};
            } else {
                return {widen(r), widen(g), widen(b), full};
            }
        } else {
            constexpr unsigned size = wide(S) ? 2 : 1;
            auto at = [&](unsigned i) {
                if constexpr (wide(S)) {
                    return read_channel16<T>(p + i * size);
                } else {
                    return read_channel8<T>(p + i * size);
                }
            };
            if constexpr (gray(S)) {
                T v = at(0);
                return {v, v, v, alpha(S) ? at(1) : full};
            } else {
                return {at(0), at(1), at(2), alpha(S) ? at(3) : full};
            }
        }
    }

    // A gray source into a gray destination keeps its value as it is; a
    // color source into gray takes its luma
    template<pixel_format D, bool GraySource, class T>
    inline void store_pixel(std::byte* p, const Rgba<T>& c) noexcept {
        if constexpr (D == pixel_format::cmyk8) {
            uint8_t r, g, b;
            if constexpr (std::is_same_v<T, uint8_t>) {
                r = c.r; g = c.g; b = c.b;
            } else {
                r = narrow(c.r); g = narrow(c.g); b = narrow(c.b);
            }
            unsigned w = r > g ? r : g;
            w = w > b ? w : b;
            if (w == 0) {
                p[0] = p[1] = p[2] = std::byte{0};
                p[3] = std::byte{255};
                return;
            }
            p[0] = static_cast<std::byte>(((w - r) * 255u + w / 2) / w);
            p[1] = static_cast<std::byte>(((w - g) * 255u + w / 2) / w);
            p[2] = static_cast<std::byte>(((w - b) * 255u + w / 2) / w);
            p[3] = static_cast<std::byte>(255u - w);
        } else {
            constexpr unsigned size = wide(D) ? 2 : 1;
            auto put = [&](unsigned i, T v) {
                if constexpr (wide(D)) {
                    write_channel16<T>(p + i * size, v);
                } else {
                    write_channel8<T>(p + i * size, v);
                }
            };
            if constexpr (gray(D)) {
                put(0, GraySource ? c.r : luma<T>(c.r, c.g, c.b));
                if constexpr (alpha(D)) {
                    put(1, c.a);
                }
            } else {
                put(0, c.r);
                put(1, c.g);
                put(2, c.b);
                if constexpr (alpha(D)) {
                    put(3, c.a);
                }
            }
        }
    }

    using ConvertRow = void (*)(const std::byte* src, std::byte* dst, size_t count) noexcept;

    template<pixel_format S, pixel_format D>
    void convert_pixels(const std::byte* src, std::byte* dst, size_t count) noexcept {
        if constexpr (S == D) {
            sgcl::detail::copy_bytes(dst, src, count * bytes_per_pixel(S));
        } else {
            using T = std::conditional_t<wide(S) || wide(D), uint16_t, uint8_t>;
            constexpr unsigned sb = bytes_per_pixel(S);
            constexpr unsigned db = bytes_per_pixel(D);
            for (size_t i = 0; i < count; ++i) {
                store_pixel<D, gray(S)>(dst + i * db, load_pixel<S, T>(src + i * sb));
            }
        }
    }

    template<size_t... I>
    constexpr auto make_convert_table(std::index_sequence<I...>) noexcept {
        struct Table {
            ConvertRow f[PixelFormatCount * PixelFormatCount];
        };
        return Table{{convert_pixels<static_cast<pixel_format>(I / PixelFormatCount), static_cast<pixel_format>(I % PixelFormatCount)>...}};
    }

    inline constexpr auto ConvertTable = make_convert_table(std::make_index_sequence<PixelFormatCount * PixelFormatCount>());

    // The conversion of `count` pixels of format s into format d: what
    // image::convert does row by row, and what a decoder does with each
    // row it makes when decode_options.want asks for another format
    inline ConvertRow converter(pixel_format s, pixel_format d) noexcept {
        return ConvertTable.f[static_cast<unsigned>(s) * PixelFormatCount + static_cast<unsigned>(d)];
    }
}
