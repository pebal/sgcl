//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "apple_imageio.h"
#include "input.h"
#include "pixels.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../core/aliases.h"
#include "../../core/detail/os.h"
#include "../../core/expected.h"
#include "../../core/string.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>

// Decoding through the system's codec, ImageIO on Apple's systems: the
// first image of a file the system reads and the module does not (HEIF and
// AVIF, heif_apple.h; JPEG XL, jxl_apple.h), its pixels straight (not
// premultiplied), 16 bits a channel when the file has more than 8. Each
// format names itself by an ImageioKind: the prefix of its messages, the
// types ImageIO knows it by, and a guard against files the system's
// decoder does not survive.
namespace sgcl::codec::detail {
#if defined(__APPLE__)
    struct ImageioKind {
        const char* name;                                     // "heif", "jxl"
        bool (*type_ok)(apple::Ref type) noexcept;            // the UTIs of the format
        optional<error> (*guard)(const uint8_t*, size_t) noexcept;   // null for none
        const char* not_type;                                 // a file ImageIO takes for another format
        bool (*gray_alpha_broken)() noexcept = nullptr;       // whether the system decodes gray with alpha wrong
    };

    inline error imageio_error(errc code, const ImageioKind& k, const char* what) noexcept {
        return error(code, 0, string::concat(k.name, ": ", what));
    }

    // Whether ImageIO names a type by one of the UTIs
    inline bool imageio_type_in(apple::Ref type, std::initializer_list<const char*> names) noexcept {
        if (!type) {
            return false;
        }
        for (const char* name : names) {
            apple::Owned s(apple::string_create(nullptr, name, apple::Utf8));
            if (s && apple::equal(type, s.get())) {
                return true;
            }
        }
        return false;
    }

    inline error imageio_status_error(apple::Status st, const ImageioKind& k) noexcept {
        switch (st) {
            case apple::StatusUnexpectedEof:
            case apple::StatusIncomplete:
            case apple::StatusReadingHeader:
                return imageio_error(errc::unexpected_end, k, "the file ends before its image");
            case apple::StatusUnknownType:
                return imageio_error(errc::unsupported, k, "a kind of file this system's ImageIO does not read");
            default:
                return imageio_error(errc::corrupt, k, "ImageIO refuses the file");
        }
    }

    // The pixels of a CGImage in format f (gray or RGB, alpha straight, 16
    // bits in the machine's order) into dst by vImage, in the image's own
    // color space (no color conversion); false for a layout it does not read
    inline bool imageio_vimage(apple::Ref cg, apple::Ref space, pixel_format f, std::byte* dst, size_t stride, size_t w, size_t h) noexcept {
        using namespace apple;
        const bool deep = f == pixel_format::gray16 || f == pixel_format::gray_alpha16 || f == pixel_format::rgb16 || f == pixel_format::rgba16;
        const bool alpha = f == pixel_format::gray_alpha8 || f == pixel_format::gray_alpha16 || f == pixel_format::rgba8 || f == pixel_format::rgba16;
        VImageFormat format{};
        format.bits_per_component = deep ? 16 : 8;
        format.bits_per_pixel = uint32_t(bytes_per_pixel(f)) * 8;
        format.color_space = space;
        format.bitmap_info = (alpha ? AlphaLast : AlphaNone) | (deep ? ByteOrder16Little : 0);
        VImageBuffer buffer{dst, h, w, stride};
        return vimage_init_with_image(&buffer, &format, nullptr, cg, VImageNoAllocate) == VImageNoError;
    }

    // The pixels of a CGImage drawn by CoreGraphics into RGBA of 8 or 16
    // bits in an RGB space: premultiplied as a bitmap context holds them,
    // straightened here (what premultiplying lost stays lost); opaque when
    // the image has no alpha
    inline bool imageio_draw(apple::Ref cg, apple::Ref space, bool deep, bool alpha, std::byte* dst, size_t stride, size_t w, size_t h) noexcept {
        using namespace apple;
        Owned context(bitmap_context_create(dst, w, h, deep ? 16 : 8, stride, space,
                                            (alpha ? AlphaPremultipliedLast : AlphaNoneSkipLast) | (deep ? ByteOrder16Little : 0)));
        if (!context) {
            return false;
        }
        context_draw_image(context.get(), Rect{0, 0, double(w), double(h)}, cg);
        for (size_t y = 0; y < h; ++y) {
            std::byte* row = dst + y * stride;
            for (size_t x = 0; x < w; ++x) {
                if (deep) {
                    uint16_t px[4];
                    std::memcpy(px, row + x * 8, 8);
                    if (!alpha) {
                        px[3] = 0xFFFF;
                    } else if (px[3] != 0xFFFF) {
                        for (int c = 0; c < 3; ++c) {
                            px[c] = px[3] ? uint16_t(std::min<uint32_t>(0xFFFF, (uint32_t(px[c]) * 0xFFFF + px[3] / 2) / px[3])) : 0;
                        }
                    }
                    std::memcpy(row + x * 8, px, 8);
                } else {
                    auto* px = reinterpret_cast<uint8_t*>(row + x * 4);
                    if (!alpha) {
                        px[3] = 0xFF;
                    } else if (px[3] != 0xFF) {
                        for (int c = 0; c < 3; ++c) {
                            px[c] = px[3] ? uint8_t(std::min<uint32_t>(0xFF, (uint32_t(px[c]) * 0xFF + px[3] / 2) / px[3])) : 0;
                        }
                    }
                }
            }
        }
        return true;
    }

    // Rows of format f into the image, converted to its format
    inline void imageio_rows(const std::byte* rows, size_t stride, pixel_format f, image& im, size_t w, size_t h) noexcept {
        auto& s = ImageAccess::state(im);
        const auto run = converter(f, s.format);
        for (size_t y = 0; y < h; ++y) {
            run(rows + y * stride, s.pixels.data() + y * s.stride, w);
        }
    }

    // The first image of a file in memory, of the kind k. Its native pixel format: gray
    // or RGB (any other color model converted to sRGB), alpha when it has
    // any, 16 bits when its components have more than 8. Cannot throw: a
    // CFData the system does not make (it has no other reason to refuse
    // one) ends the program (os::memory_refused)
    inline expected<image, error> imageio_decode_memory(const uint8_t* data, size_t size, const decode_options& o, const ImageioKind& k) noexcept {
        using namespace apple;
        if (o.want && !valid(*o.want)) {
            return unexpected(imageio_error(errc::invalid_argument, k, "the pixel format outside the list"));
        }
        // what the system's decoder does not survive, refused before it
        // sees the file (heif_guard.h)
        if (k.guard) {
            if (auto e = k.guard(data, size)) {
                return unexpected(*e);
            }
        }
        Owned bytes(data_create_no_copy(nullptr, data, Index(size), allocator_null));
        if (!bytes) [[unlikely]] {
            sgcl::detail::os::memory_refused("a CFData over the file to decode");
        }
        const Ref keys[] = {source_should_cache};
        const Ref values[] = {boolean_false};
        Owned no_cache(dictionary(keys, values, 1));
        Owned source(source_create_with_data(bytes.get(), no_cache.get()));
        if (!source) {
            return unexpected(imageio_error(errc::corrupt, k, "ImageIO refuses the file"));
        }
        if (Status st = source_status(source.get()); st != StatusComplete) {
            return unexpected(imageio_status_error(st, k));
        }
        if (!k.type_ok(source_type(source.get()))) {
            return unexpected(imageio_error(errc::corrupt, k, k.not_type));
        }
        if (source_count(source.get()) == 0) {
            return unexpected(imageio_error(errc::corrupt, k, "no image in the file"));
        }
        if (Status st = source_status_at(source.get(), 0); st != StatusComplete) {
            return unexpected(imageio_status_error(st, k));
        }
        // the size from the properties, checked before anything is decoded
        Owned props(source_copy_properties_at(source.get(), 0, nullptr));
        const long long pw = number_of(props.get(), property_pixel_width, 0), ph = number_of(props.get(), property_pixel_height, 0);
        if (pw < 0 || ph < 0 || pw > 0xFFFFFFFFll || ph > 0xFFFFFFFFll) {
            return unexpected(imageio_error(errc::corrupt, k, "a size ImageIO gives out of range"));
        }
        if (pw && ph) {
            if (auto e = check_size(uint32_t(pw), uint32_t(ph), o.limits, 0)) {
                return unexpected(*e);
            }
        }
        Owned cg(source_create_image_at(source.get(), 0, no_cache.get()));
        if (!cg) {
            return unexpected(imageio_status_error(source_status_at(source.get(), 0), k));
        }
        const size_t w = image_width(cg.get()), h = image_height(cg.get());
        if (w > 0xFFFFFFFFu || h > 0xFFFFFFFFu) {
            return unexpected(error(errc::too_large, 0));
        }
        if (auto e = check_size(uint32_t(w), uint32_t(h), o.limits, 0)) {
            return unexpected(*e);
        }
        // the layout: the image's color model and alpha, its depth
        Ref space = image_color_space(cg.get());
        const ColorSpaceModel model = space ? color_space_model(space) : ModelRgb;
        const AlphaInfo alpha_info = image_alpha_info(cg.get()) & AlphaInfoMask;
        const bool alpha = alpha_info != AlphaNone && alpha_info != AlphaNoneSkipFirst && alpha_info != AlphaNoneSkipLast;
        const bool deep = image_bits_per_component(cg.get()) > 8;
        const bool gray = model == ModelMonochrome && alpha_info != AlphaOnly;
        Owned srgb;
        if (!space || (model != ModelMonochrome && model != ModelRgb) || alpha_info == AlphaOnly) {
            srgb = Owned(color_space_create_with_name(color_space_srgb));
            space = srgb.get();
        }
        const pixel_format native = gray ? (alpha ? (deep ? pixel_format::gray_alpha16 : pixel_format::gray_alpha8) : (deep ? pixel_format::gray16 : pixel_format::gray8))
                                         : (alpha ? (deep ? pixel_format::rgba16 : pixel_format::rgba8) : (deep ? pixel_format::rgb16 : pixel_format::rgb8));
        const pixel_format out = o.want.value_or(native);
        image result(uint32_t(w), uint32_t(h), out);
        auto& s = ImageAccess::state(result);
        // the pixels by vImage: straight into the image, or into rows of
        // the native format when another is asked for (converter, row by row)
        std::vector<std::byte> rows;
        bool done;
        if ((native == pixel_format::gray_alpha8 || native == pixel_format::gray_alpha16) && k.gray_alpha_broken && k.gray_alpha_broken()) {
            return unexpected(imageio_error(errc::unsupported, k, "gray with alpha, which this system's ImageIO decodes wrong"));
        }
        if (out == native) {
            done = imageio_vimage(cg.get(), space, native, s.pixels.data(), s.stride, w, h);
        } else {
            const size_t stride = row_bytes(uint32_t(w), native);
            rows.resize(stride * h);
            done = imageio_vimage(cg.get(), space, native, rows.data(), stride, w, h);
            if (done) {
                imageio_rows(rows.data(), stride, native, result, w, h);
            }
        }
        if (!done) {
            // a layout vImage does not read (the packed 10 bits of an HDR
            // HEIC): drawn by CoreGraphics into RGBA in an RGB space, then
            // straightened and converted as the rows above
            Owned rgb;
            if (gray) {
                rgb = Owned(color_space_create_with_name(color_space_srgb));
            }
            const pixel_format drawn = deep ? pixel_format::rgba16 : pixel_format::rgba8;
            const size_t stride = row_bytes(uint32_t(w), drawn);
            rows.assign(stride * h, std::byte{0});
            if (!imageio_draw(cg.get(), gray ? rgb.get() : space, deep, alpha, rows.data(), stride, w, h)) {
                return unexpected(imageio_error(errc::corrupt, k, "ImageIO's pixels could not be converted"));
            }
            imageio_rows(rows.data(), stride, drawn, result, w, h);
        }
        if (o.metadata) {
            // the orientation (ImageIO's reading of irot and imir) and the
            // profile of the pixels' color space; EXIF's own bytes are not
            // given by ImageIO
            ImageAccess::set_orientation(result, number_of(props.get(), property_orientation, 1));
            Owned icc(color_space_copy_icc(space));
            if (icc) {
                const size_t n = size_t(data_length(icc.get()));
                if (n > o.limits.max_metadata) {
                    return unexpected(imageio_error(errc::too_large, k, "the ICC profile past limits.max_metadata"));
                }
                const auto* p = reinterpret_cast<const byte*>(data_bytes(icc.get()));
                s.icc.insert(s.icc.end(), p, p + n);
            }
        }
        return result;
    }

    // A stream read to its end first: ImageIO reads a file whole (the items
    // of a HEIF file lie where its iloc box says, anywhere in it)
    template<class Input>
    expected<image, error> imageio_decode_input(Input& in, const decode_options& o, const ImageioKind& k) {
        std::vector<uint8_t> file;
        for (;;) {
            const uint8_t* p;
            size_t got;
            if (!in.peek(size_t(1) << 20, p, got)) {
                return unexpected(*in.failure);
            }
            if (got == 0) {
                break;
            }
            file.insert(file.end(), p, p + got);
            in.consume(got);
        }
        return imageio_decode_memory(file.data(), file.size(), o, k);
    }
#endif
}
