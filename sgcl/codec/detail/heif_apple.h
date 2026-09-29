//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "apple_imageio.h"
#include "input.h"
#include "output.h"
#include "pixels.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../core/aliases.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/vector.h"
#include "../../io/stream.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

// HEIF (HEIC) and AVIF through the system's codec: ImageIO on Apple's
// systems (HEVC and AV1 are the platform's, as video is), unsupported
// elsewhere. The first image of the file; its pixels straight (not
// premultiplied), 16 bits a channel when the file has more than 8.
namespace sgcl::codec::detail {
    inline error heif_unsupported() {
        return error(errc::unsupported, 0, "heif: needs the system's codec (macOS ImageIO)");
    }

#if defined(__APPLE__)
    inline error heif_status_error(apple::Status st) {
        switch (st) {
            case apple::StatusUnexpectedEof:
            case apple::StatusIncomplete:
            case apple::StatusReadingHeader:
                return error(errc::unexpected_end, 0, "heif: the file ends before its image");
            case apple::StatusUnknownType:
                return error(errc::unsupported, 0, "heif: a kind of file this system's ImageIO does not read");
            default:
                return error(errc::corrupt, 0, "heif: ImageIO refuses the file");
        }
    }

    // HEIF, HEIC and AVIF, still or a sequence: the types ImageIO names
    // them by, and nothing else is decoded here (sniff chose HEIF; a file
    // ImageIO takes for another format is not one)
    inline bool heif_type(apple::Ref type) noexcept {
        if (!type) {
            return false;
        }
        for (const char* name : {"public.heic", "public.heif", "public.heics", "public.heifs", "public.avif", "public.avis"}) {
            apple::Owned s(apple::string_create(nullptr, name, apple::Utf8));
            if (s && apple::equal(type, s.get())) {
                return true;
            }
        }
        return false;
    }

    // The pixels of a CGImage in format f (gray or RGB, alpha straight, 16
    // bits in the machine's order) into dst by vImage, in the image's own
    // color space (no color conversion); false for a layout it does not read
    inline bool heif_vimage(apple::Ref cg, apple::Ref space, pixel_format f, std::byte* dst, size_t stride, size_t w, size_t h) {
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
    inline bool heif_draw(apple::Ref cg, apple::Ref space, bool deep, bool alpha, std::byte* dst, size_t stride, size_t w, size_t h) {
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
    inline void heif_rows(const std::byte* rows, size_t stride, pixel_format f, image& im, size_t w, size_t h) {
        auto& s = ImageAccess::state(im);
        const auto run = converter(f, s.format);
        for (size_t y = 0; y < h; ++y) {
            run(rows + y * stride, s.pixels.data() + y * s.stride, w);
        }
    }

    // The first image of a file in memory. Its native pixel format: gray
    // or RGB (any other color model converted to sRGB), alpha when it has
    // any, 16 bits when its components have more than 8
    inline expected<image, error> heif_decode_memory(const uint8_t* data, size_t size, const decode_options& o) {
        using namespace apple;
        if (o.want && !valid(*o.want)) {
            return unexpected(error(errc::invalid_argument, 0, "heif: the pixel format outside the list"));
        }
        Owned bytes(data_create_no_copy(nullptr, data, Index(size), allocator_null));
        if (!bytes) {
            throw std::bad_alloc();
        }
        const Ref keys[] = {source_should_cache};
        const Ref values[] = {boolean_false};
        Owned no_cache(dictionary(keys, values, 1));
        Owned source(source_create_with_data(bytes.get(), no_cache.get()));
        if (!source) {
            return unexpected(error(errc::corrupt, 0, "heif: ImageIO refuses the file"));
        }
        if (Status st = source_status(source.get()); st != StatusComplete) {
            return unexpected(heif_status_error(st));
        }
        if (!heif_type(source_type(source.get()))) {
            return unexpected(error(errc::corrupt, 0, "heif: not a HEIF or AVIF file to ImageIO"));
        }
        if (source_count(source.get()) == 0) {
            return unexpected(error(errc::corrupt, 0, "heif: no image in the file"));
        }
        if (Status st = source_status_at(source.get(), 0); st != StatusComplete) {
            return unexpected(heif_status_error(st));
        }
        // the size from the properties, checked before anything is decoded
        Owned props(source_copy_properties_at(source.get(), 0, nullptr));
        const long long pw = number_of(props.get(), property_pixel_width, 0), ph = number_of(props.get(), property_pixel_height, 0);
        if (pw < 0 || ph < 0 || pw > 0xFFFFFFFFll || ph > 0xFFFFFFFFll) {
            return unexpected(error(errc::corrupt, 0, "heif: a size ImageIO gives out of range"));
        }
        if (pw && ph) {
            if (auto e = check_size(uint32_t(pw), uint32_t(ph), o.limits, 0)) {
                return unexpected(*e);
            }
        }
        Owned cg(source_create_image_at(source.get(), 0, no_cache.get()));
        if (!cg) {
            return unexpected(heif_status_error(source_status_at(source.get(), 0)));
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
        if (out == native) {
            done = heif_vimage(cg.get(), space, native, s.pixels.data(), s.stride, w, h);
        } else {
            const size_t stride = row_bytes(uint32_t(w), native);
            rows.resize(stride * h);
            done = heif_vimage(cg.get(), space, native, rows.data(), stride, w, h);
            if (done) {
                heif_rows(rows.data(), stride, native, result, w, h);
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
            if (!heif_draw(cg.get(), gray ? rgb.get() : space, deep, alpha, rows.data(), stride, w, h)) {
                return unexpected(error(errc::corrupt, 0, "heif: ImageIO's pixels could not be converted"));
            }
            heif_rows(rows.data(), stride, drawn, result, w, h);
        }
        if (o.metadata) {
            // the orientation (ImageIO's reading of irot and imir) and the
            // profile of the pixels' color space; EXIF's own bytes are not
            // given by ImageIO
            ImageAccess::set_orientation(result, unsigned(number_of(props.get(), property_orientation, 1)));
            Owned icc(color_space_copy_icc(space));
            if (icc) {
                const size_t n = size_t(data_length(icc.get()));
                if (n > o.limits.max_metadata) {
                    return unexpected(error(errc::too_large, 0, "heif: the ICC profile past limits.max_metadata"));
                }
                const auto* p = reinterpret_cast<const byte*>(data_bytes(icc.get()));
                s.icc.insert(s.icc.end(), p, p + n);
            }
        }
        return result;
    }

    // A stream read to its end first: the items of a HEIF file lie where
    // its iloc box says, anywhere in it, and ImageIO reads them so
    template<class Input>
    expected<image, error> heif_decode_input(Input& in, const decode_options& o) {
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
        return heif_decode_memory(file.data(), file.size(), o);
    }

    // The image as a CGImage over its own pixels (no copy): gray or RGB,
    // alpha straight, 16 bits in the machine's order. Its color space from
    // the ICC profile when that has the image's model, else sRGB or gray
    // of gamma 2.2
    inline apple::Owned heif_cgimage(const image& im, apple::Owned& space, apple::Owned& provider) {
        using namespace apple;
        const pixel_format f = im.format();
        const bool gray = f == pixel_format::gray8 || f == pixel_format::gray16 || f == pixel_format::gray_alpha8 || f == pixel_format::gray_alpha16;
        const bool alpha = f == pixel_format::gray_alpha8 || f == pixel_format::gray_alpha16 || f == pixel_format::rgba8 || f == pixel_format::rgba16;
        const bool deep = f == pixel_format::gray16 || f == pixel_format::gray_alpha16 || f == pixel_format::rgb16 || f == pixel_format::rgba16;
        const auto icc = im.icc();
        if (!icc.empty()) {
            Owned data(data_create(nullptr, reinterpret_cast<const uint8_t*>(icc.data()), Index(icc.size())));
            Owned from_icc(data ? color_space_create_with_icc(data.get()) : nullptr);
            if (from_icc && color_space_model(from_icc.get()) == (gray ? ModelMonochrome : ModelRgb)) {
                space = std::move(from_icc);
            }
        }
        if (!space) {
            space = Owned(color_space_create_with_name(gray ? color_space_gray : color_space_srgb));
        }
        const auto px = im.pixels();
        provider = Owned(provider_create_with_data(nullptr, px.data(), px.size(), nullptr));
        const size_t bpc = deep ? 16 : 8;
        const size_t channels = bytes_per_pixel(f) / (deep ? 2 : 1);
        return Owned(image_create(im.width(), im.height(), bpc, bpc * channels, im.stride(), space.get(),
                                  (alpha ? AlphaLast : AlphaNone) | (deep ? ByteOrder16Little : 0), provider.get(), nullptr, false, 0));
    }

    // Where the encoded file goes: a CFData to copy into a vector, or a
    // writer the destination's consumer writes to as ImageIO gives bytes
    struct HeifConsumer {
        WriterSink sink;
        bool failed = false;

        static size_t put(void* info, const void* buffer, size_t count) {
            auto* self = static_cast<HeifConsumer*>(info);
            if (self->failed || !self->sink.put(static_cast<const uint8_t*>(buffer), count)) {
                self->failed = true;
                return 0;
            }
            return count;
        }
    };

    // The image as HEIC at quality 1..100 into the destination made by
    // make(type); errc::unsupported when the system has no HEVC encoder
    template<class Make>
    expected<void, error> heif_encode_into(const image& original, int quality, Make make) {
        using namespace apple;
        if (quality < 1 || quality > 100) {
            return unexpected(error(errc::invalid_argument, 0, "heif: quality outside 1..100"));
        }
        // CMYK through RGB (its profile left behind with the ink)
        const image im = original.format() == pixel_format::cmyk8 ? original.convert(pixel_format::rgb8) : original;
        Owned space, provider;
        Owned cg = heif_cgimage(im, space, provider);
        if (!cg) {
            return unexpected(error(errc::invalid_argument, 0, "heif: CoreGraphics does not take the image"));
        }
        Owned type(string_create(nullptr, "public.heic", Utf8));
        Owned destination = make(type.get());
        if (!destination) {
            return unexpected(error(errc::unsupported, 0, "heif: this system has no HEIC encoder"));
        }
        const double q = quality / 100.0;
        const int32_t orientation = int32_t(original.orientation());
        Owned q_number(number_create(nullptr, NumberDouble, &q));
        Owned o_number(number_create(nullptr, NumberSInt32, &orientation));
        const Ref keys[] = {destination_lossy_quality, property_orientation};
        const Ref values[] = {q_number.get(), o_number.get()};
        Owned properties(dictionary(keys, values, orientation != 1 ? 2 : 1));
        destination_add_image(destination.get(), cg.get(), properties.get());
        if (!destination_finalize(destination.get())) {
            return unexpected(error(errc::unsupported, 0, "heif: the system's encoder did not write the image"));
        }
        return {};
    }

    inline expected<vector<byte>, error> heif_encode(const image& im, int quality) {
        using namespace apple;
        Owned data(data_create_mutable(nullptr, 0));
        if (!data) {
            throw std::bad_alloc();
        }
        auto r = heif_encode_into(im, quality, [&](Ref type) {
            return Owned(destination_create_with_data(data.get(), type, 1, nullptr));
        });
        if (!r) {
            return unexpected(r.error());
        }
        const auto* p = reinterpret_cast<const byte*>(data_bytes(data.get()));
        const size_t n = size_t(data_length(data.get()));
        vector<byte> out;
        out.reserve(n);
        out.insert(out.end(), p, p + n);
        return out;
    }

    inline expected<void, error> heif_encode(const image& im, int quality, const io::writer& out) {
        using namespace apple;
        HeifConsumer to{WriterSink{out, 0, nullopt}};
        const ConsumerCallbacks callbacks{&HeifConsumer::put, nullptr};
        Owned consumer(consumer_create(&to, &callbacks));
        if (!consumer) {
            throw std::bad_alloc();
        }
        auto r = heif_encode_into(im, quality, [&](Ref type) {
            return Owned(destination_create_with_consumer(consumer.get(), type, 1, nullptr));
        });
        if (to.sink.failure) {
            return unexpected(*to.sink.failure);
        }
        return r;
    }
#endif

    // The first image of a HEIF or AVIF file in memory, read in place
    inline expected<image, error> heif_decode_bytes(const slice<const byte>& data, const decode_options& o) {
#if defined(__APPLE__)
        return heif_decode_memory(reinterpret_cast<const uint8_t*>(data.data()), data.size(), o);
#else
        (void)data;
        (void)o;
        return unexpected(heif_unsupported());
#endif
    }

    // The same from an input (a stream, its head already held)
    template<class Input>
    expected<image, error> heif_decode_stream(Input& in, const decode_options& o) {
#if defined(__APPLE__)
        return heif_decode_input(in, o);
#else
        (void)in;
        (void)o;
        return unexpected(heif_unsupported());
#endif
    }
}
