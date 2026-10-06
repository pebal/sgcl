//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "apple_imageio.h"
#include "heif_guard.h"
#include "imageio_decode.h"
#include "input.h"
#include "output.h"
#include "pixels.h"
#include "../error.h"
#include "../image.h"
#include "../options.h"
#include "../../core/aliases.h"
#include "../../core/detail/os.h"
#include "../../core/expected.h"
#include "../../core/slice.h"
#include "../../core/vector.h"
#include "../../io/stream.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <vector>

// HEIF (HEIC) and AVIF through the system's codec: ImageIO on Apple's
// systems (HEVC and AV1 are the platform's, as video is), unsupported
// elsewhere. The first image of the file; its pixels straight (not
// premultiplied), 16 bits a channel when the file has more than 8. A file
// the system's decoder would hang on is refused before ImageIO sees it
// (heif_guard.h).
namespace sgcl::codec::detail {
    inline error heif_unsupported() noexcept {
        return error(errc::unsupported, 0, "heif: needs the system's codec (macOS ImageIO)");
    }

#if defined(__APPLE__)
    // HEIF, HEIC and AVIF, still or a sequence: the types ImageIO names
    // them by, and nothing else is decoded here (sniff chose HEIF; a file
    // ImageIO takes for another format is not one)
    inline bool heif_type(apple::Ref type) noexcept {
        return imageio_type_in(type, {"public.heic", "public.heif", "public.heics", "public.heifs", "public.avif", "public.avis"});
    }

    inline constexpr ImageioKind HeifKind{"heif", heif_type, heif_guard, "not a HEIF or AVIF file to ImageIO"};

    // The image as a CGImage over its own pixels (no copy): gray or RGB,
    // alpha straight, 16 bits in the machine's order. Its color space from
    // the ICC profile when that has the image's model, else sRGB or gray
    // of gamma 2.2
    inline apple::Owned heif_cgimage(const image& im, apple::Owned& space, apple::Owned& provider) noexcept {

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
    // A write that throws is called from inside the system's encoder, C
    // code an exception must not unwind: it is caught there, the write
    // refused, and thrown again once the encoder has returned
    struct HeifConsumer {
        WriterSink sink;
        bool failed = false;
        std::exception_ptr thrown;

        static size_t put(void* info, const void* buffer, size_t count) noexcept {
            auto* self = static_cast<HeifConsumer*>(info);
            if (self->failed) {
                return 0;
            }
            try {
                if (!self->sink.put(static_cast<const uint8_t*>(buffer), count)) {
                    self->failed = true;
                    return 0;
                }
            } catch (...) {
                self->thrown = std::current_exception();
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
        // CMYK through RGB (its profile left behind with the ink); 16-bit
        // gray with alpha, which the system's encoder does not write (it
        // writes the 8-bit one and 16-bit gray), through RGBA of 16 bits,
        // its gray profile left behind
        const pixel_format f = original.format();
        const image im = f == pixel_format::cmyk8 ? original.convert(pixel_format::rgb8)
                         : f == pixel_format::gray_alpha16 ? original.convert(pixel_format::rgba16)
                                                           : original;
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

    // Into bytes: cannot throw, a CFData the system does not make ends the
    // program (os::memory_refused). Into a stream: what its write throws,
    // and a consumer the system does not make ends the program likewise
    inline expected<vector<byte>, error> heif_encode(const image& im, int quality) noexcept {
        using namespace apple;
        Owned data(data_create_mutable(nullptr, 0));
        if (!data) [[unlikely]] {
            sgcl::detail::os::memory_refused("a CFData for the encoded file");
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
        HeifConsumer to{WriterSink{out, 0, nullopt}, false, nullptr};
        const ConsumerCallbacks callbacks{&HeifConsumer::put, nullptr};
        Owned consumer(consumer_create(&to, &callbacks));
        if (!consumer) [[unlikely]] {
            sgcl::detail::os::memory_refused("a CGDataConsumer for the stream");
        }
        auto r = heif_encode_into(im, quality, [&](Ref type) {
            return Owned(destination_create_with_consumer(consumer.get(), type, 1, nullptr));
        });
        if (to.thrown) {
            std::rethrow_exception(to.thrown);
        }
        if (to.sink.failure) {
            return unexpected(*to.sink.failure);
        }
        return r;
    }
#endif

    // The first image of a HEIF or AVIF file in memory, read in place
    SGCL_INLINE_HOT expected<image, error> heif_decode_bytes(const slice<const byte>& data, const decode_options& o) noexcept {
#if defined(__APPLE__)
        return imageio_decode_memory(reinterpret_cast<const uint8_t*>(data.data()), data.size(), o, HeifKind);
#else
        (void)data;
        (void)o;
        return unexpected(heif_unsupported());
#endif
    }

    // The same from an input (a stream, its head already held)
    template<class Input>
    SGCL_INLINE_HOT expected<image, error> heif_decode_stream(Input& in, const decode_options& o) {
#if defined(__APPLE__)
        return imageio_decode_input(in, o, HeifKind);
#else
        (void)in;
        (void)o;
        return unexpected(heif_unsupported());
#endif
    }
}
