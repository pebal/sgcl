//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// WebP on any bytes, against libwebp's WebPAnimDecoder (its demuxer's checks
// of the container, then each frame decoded):
//
//   - whether the file is taken: frames() reads every frame exactly when
//     libwebp decodes every frame, and decode() gives the first exactly
//     when libwebp's demuxer takes the file and decodes its first frame;
//     the one refusal of what libwebp takes is data RFC 6386 or RFC 9649
//     make invalid, a partition or an alpha stream read past its end
//     (WebpReader::damage says so), which libwebp decodes on;
//   - a still image's pixels, lossless and lossy, those of libwebp's
//     WebPDecodeRGBA bit for bit (when it decodes the file: it applies an
//     ALPH chunk whatever VP8X's alpha flag says, where WebPAnimDecoder
//     leaves the image opaque); an animation's canvases within what
//     libwebp's fixed-point blending may be off the exact one (one in
//     alpha, about 255 / A in a color), equal where alpha is 0 on both
//     sides by alpha alone; pixels not compared where a lossy frame's
//     coefficients go past what an encoder makes (libwebp's C, NEON and
//     SSE2 paths differ from each other there: vp8::Decoder);
//   - the frames read from memory and through a stream of pieces alike;
//   - the loop count libwebp's, known before the first frame and the same
//     after the last.
//
// Canvases past 4 million pixels are left to the module's limit (neither
// side decodes them). A difference aborts.
//
//   SGCL_FUZZ_LIBS="-I/opt/homebrew/opt/webp/include /opt/homebrew/opt/webp/lib/libwebpdemux.a \
//       /opt/homebrew/opt/webp/lib/libwebp.a /opt/homebrew/opt/webp/lib/libsharpyuv.a \
//       -framework ImageIO -framework CoreGraphics -framework CoreFoundation -framework Accelerate" \
//       tests/fuzz/run.sh tests/codec/fuzz/webp_decode_fuzz.cpp 300
//
// (the frameworks for codec.h's HEIF on macOS; Homebrew's libwebp is
// arm64 alone, so the harness runs there)
//
// Seeds: seeds/webp_decode/ (libwebp-test-data's small lossless files and
// animations made by tests/codec/webp_builder.h); the dictionary
// tests/fuzz/dict/webp_decode.dict.
#include "sgcl/codec/codec.h"

#include <webp/decode.h>
#include <webp/demux.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
    using namespace sgcl;

    constexpr uint64_t MaxPixels = uint64_t(1) << 22;

    void check(bool ok, const char* what) {
        if (!ok) {
            std::fprintf(stderr, "webp_decode_fuzz: %s\n", what);
            std::abort();
        }
    }

    struct pieces {
        const uint8_t* data;
        size_t size;
        size_t step;
        size_t at = 0;

        expected<size_t, io::error> read(const slice<std::byte>& b) {
            size_t n = std::min({step, b.size(), size - at});
            std::memcpy(b.data(), data + at, n);
            at += n;
            return n;
        }
    };

    // libwebp's reading: taken or not, the canvases RGBA
    struct Theirs {
        bool demuxed = false;       // the demuxer took the file
        bool first = false;         // and the first frame decoded
        bool all = false;           // and every frame
        bool too_large = false;     // a canvas past MaxPixels: not decoded
        uint32_t width = 0, height = 0, loops = 0;
        std::vector<std::string> canvases;
    };

    Theirs libwebp(const uint8_t* data, size_t size) {
        Theirs t;
        WebPData wd = {data, size};
        WebPDemuxer* dm = WebPDemux(&wd);
        if (!dm) {
            return t;
        }
        const uint64_t w = WebPDemuxGetI(dm, WEBP_FF_CANVAS_WIDTH), h = WebPDemuxGetI(dm, WEBP_FF_CANVAS_HEIGHT);
        WebPDemuxDelete(dm);
        if (w * h > MaxPixels) {
            t.too_large = true;
            return t;
        }
        WebPAnimDecoderOptions o;
        WebPAnimDecoderOptionsInit(&o);
        o.color_mode = MODE_RGBA;
        o.use_threads = 0;
        WebPAnimDecoder* d = WebPAnimDecoderNew(&wd, &o);
        if (!d) {
            return t;
        }
        t.demuxed = true;
        WebPAnimInfo info;
        WebPAnimDecoderGetInfo(d, &info);
        t.width = info.canvas_width;
        t.height = info.canvas_height;
        t.loops = info.loop_count;
        t.all = true;
        const size_t canvas = size_t(t.width) * t.height * 4;
        while (WebPAnimDecoderHasMoreFrames(d)) {
            uint8_t* buf;
            int ts;
            if (!WebPAnimDecoderGetNext(d, &buf, &ts)) {
                t.all = false;
                break;
            }
            t.canvases.emplace_back(reinterpret_cast<const char*>(buf), canvas);
        }
        t.first = !t.canvases.empty();
        WebPAnimDecoderDelete(d);
        return t;
    }

    bool unsupported(const codec::error& e) {
        return e.code() == codec::errc::unsupported;
    }

    // Our canvas against theirs: exact, or within the blend's error
    bool near(const std::string& ours, const std::string& theirs, bool animated) {
        if (ours.size() != theirs.size()) {
            return false;
        }
        if (!animated) {
            return ours == theirs;
        }
        for (size_t p = 0; p < ours.size(); p += 4) {
            const auto* x = reinterpret_cast<const uint8_t*>(ours.data() + p);
            const auto* y = reinterpret_cast<const uint8_t*>(theirs.data() + p);
            if (std::memcmp(x, y, 4) == 0 || (x[3] == 0 && y[3] == 0)) {
                continue;
            }
            if (std::abs(int(x[3]) - int(y[3])) > 1) {
                return false;
            }
            const int a = std::max(1, std::min(int(x[3]), int(y[3])));
            for (int c = 0; c < 3; ++c) {
                if (std::abs(int(x[c]) - int(y[c])) > 1 + 255 / a) {
                    return false;
                }
            }
        }
        return true;
    }

    // Whether a lossy frame has coefficients past what an encoder makes
    // (vp8::Decoder::beyond_encoders): there libwebp's own paths give
    // pixels unlike each other, and its pixels are not compared
    // Why the module refuses the file, when its data is invalid where
    // libwebp decodes on (WebpDamage); none for any other refusal
    codec::detail::WebpDamage damage_of(const slice<const byte>& bytes) {
        using Reader = codec::detail::WebpReader<codec::detail::MemoryInput>;
        codec::detail::MemoryInput in(bytes);
        codec::decode_options o;
        Reader r(in, o);
        codec::detail::WebpFrame f;
        if (!r.start()) {
            return r.damage();
        }
        while (r.next(f) == Reader::Step::frame) {
            if (!r.decode(f)) {
                break;
            }
        }
        return r.damage();
    }

    bool beyond_encoders(const slice<const byte>& bytes) {
        using Reader = codec::detail::WebpReader<codec::detail::MemoryInput>;
        codec::detail::MemoryInput in(bytes);
        codec::decode_options o;
        Reader r(in, o);
        codec::detail::WebpFrame f;
        if (!r.start()) {
            return false;
        }
        while (r.next(f) == Reader::Step::frame) {
            if (!r.decode(f)) {
                return false;
            }
            if (f.lossy && r.lossy_decoder().beyond_encoders()) {
                return true;
            }
        }
        return false;
    }

    std::string pixels_of(const codec::image& im) {
        auto px = im.pixels();
        return std::string(reinterpret_cast<const char*>(px.data()), px.size());
    }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const slice<const byte> bytes(reinterpret_cast<const byte*>(data), size);
    const codec::decode_options d{.want = codec::pixel_format::rgba8, .limits = {.max_pixels = MaxPixels}};

    // every frame, from memory and through a stream of pieces
    std::vector<std::string> ours;
    bool ours_all = true, lossy = false, too_large = false;
    uint32_t loops = 1;   // the loop count, known before the first frame
    // VP8X's animation flag: canvases blended, compared within the blend's error
    const bool animated = size >= 21 && std::memcmp(data + 12, "VP8X", 4) == 0 && (data[20] & 0x02);
    {
        auto f = codec::decode_frames(bytes, d);
        if (!f) {
            ours_all = false;
            lossy = unsupported(f.error());
            too_large = f.error().code() == codec::errc::too_large;
        } else {
            loops = f->loop_count();
            for (int i = 0; i < 256; ++i) {
                auto n = f->next();
                if (!n) {
                    ours_all = false;
                    lossy = unsupported(n.error());
                    too_large = n.error().code() == codec::errc::too_large;
                    break;
                }
                if (!*n) {
                    break;
                }
                ours.push_back(pixels_of((*n)->picture));
            }
            check(!ours_all || f->loop_count() == loops, "the loop count changes as the frames are read");
        }
        // the stream: memory's frames, and where memory reads them all, its
        // end (memory refuses a file shorter than its RIFF size at once, a
        // stream when it gets there)
        pieces p{data, size, size ? size_t(data[size - 1]) % 61 + 1 : 1};
        auto g = codec::decode_frames(io::reader(p), d);
        check(bool(g) || !ours_all, "a stream refuses what memory reads");
        if (g) {
            for (size_t i = 0; i < 256; ++i) {
                auto n = g->next();
                if (!n || !*n) {
                    check(!ours_all || (n && i == ours.size()), "a stream ends elsewhere than memory");
                    break;
                }
                if (i < ours.size()) {
                    check(pixels_of((*n)->picture) == ours[i], "a stream's frame is not memory's");
                } else {
                    check(!ours_all, "a stream has more frames than memory");
                }
            }
        }
    }
    auto first = codec::decode(bytes, d);
    if (first) {
        check(!ours.empty() && pixels_of(*first) == ours[0], "decode is not the first frame");
    }
    if (lossy || too_large || (!first && (unsupported(first.error()) || first.error().code() == codec::errc::too_large))) {
        return 0;
    }

    const Theirs t = libwebp(data, size);
    if (t.too_large) {
        return 0;
    }
    const bool damaged = ((!ours_all && t.all) || (!first && t.demuxed && t.first)) && damage_of(bytes) != codec::detail::WebpDamage::none;
    check(ours_all == t.all || damaged, ours_all ? "frames takes what libwebp refuses" : "frames refuses what libwebp takes");
    check(bool(first) == (t.demuxed && t.first) || damaged, first ? "decode takes what libwebp refuses" : "decode refuses what libwebp takes");
    const bool wide = (ours_all || first) && beyond_encoders(bytes);
    if (ours_all && t.all && animated) {
        check(loops == t.loops, "a loop count unlike libwebp's");
    }
    if (ours_all && animated && !wide) {
        check(ours.size() == t.canvases.size(), "another count of frames");
        for (size_t i = 0; i < ours.size(); ++i) {
            check(near(ours[i], t.canvases[i], true), "a canvas unlike libwebp's");
        }
    }
    if (first && !animated && !wide) {
        int w = 0, h = 0;
        uint8_t* rgba = WebPDecodeRGBA(data, size, &w, &h);
        if (rgba) {
            const std::string theirs(reinterpret_cast<const char*>(rgba), size_t(w) * size_t(h) * 4);
            WebPFree(rgba);
            check(theirs == pixels_of(*first), "a still image unlike WebPDecodeRGBA's");
        }
    }
    return 0;
}
