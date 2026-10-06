//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The codec module against the C libraries: one case and one side a process,
// timed for 0.4 s after 0.1 s thrown away; prints "case|ms/op|MP/s".
//
//   bench_codec <case> <sgcl|c> <data dir>
//   bench_codec prep <data dir>        the inputs made from <data dir>/src.png
//
// The cases, on the files prep makes (run.sh makes the directories):
//   png-rgb8, png-rgba8, png-rgb16, png-rgba16   decode, PNG of the module's
//                                                encoder (adaptive filters)
//   png-paeth-rgb8, png-paeth-rgba8              decode, every row Paeth
//   png-adam7-rgb8                               decode, interlaced (Paeth)
//   png-enc                                      encode rgb8 at each side's default level
//   jpeg-base, jpeg-prog                         decode q90 4:2:0, baseline and progressive
//   jpeg-enc, jpeg-enc-opt                       encode rgb8 q90 4:2:0, the typical tables and optimized ones
//   webp-lossy, webp-lossless                    decode, cwebp -q 90 and -lossless
//   gif                                          decode the first frame as RGBA
//   gif-enc                                      encode rgb8 as GIF, 256 colors, Floyd–Steinberg (the module's
//                                                median cut; Go's default, its Plan9 palette; no C side)
//   gif-enc-nodither                             the same without dithering (giflib's GifQuantizeBuffer,
//                                                median cut; Go's Plan9 palette drawn with draw.Src)
//   gif-enc-exact                                encode the 256 colors of image.gif again (the module from
//                                                rgba8, giflib's EGifSpew and Go's image.Paletted from indices)
//   webp-enc, webp-enc-lossless                  encode rgb8 as WebP: lossy at quality 75, lossless at effort 75
//                                                (libwebp's WebPEncode, method 4: cwebp's defaults; Go has no encoder)
//   tiff-dec, tiff-enc                           decode rgb8 TIFF of LZW and the horizontal predictor, and encode it
//                                                (libtiff's strips, SGCL_BENCH_LIBTIFF; Go's x/image/tiff decodes, has no
//                                                LZW encoder)
//   tiff-enc-deflate                             encode rgb8 TIFF of Deflate and the predictor (libtiff, Go)
//   bmp-dec, bmp-enc                             24-bit BMP (Go's x/image/bmp; no C side)
//   qoi-dec, qoi-enc                             QOI (no reference library on the machine)
//   jxl-dec                                      decode a JPEG XL of cjxl -d 1 (the module through the system's
//                                                ImageIO; libjxl's JxlDecoder with its thread runner, SGCL_BENCH_LIBJXL;
//                                                Go has none)
//   qr-enc                                       a QR code of a 300-character URL at level M, drawn a pixel
//                                                a module with a border of one (CoreImage's CIQRCodeGenerator into
//                                                a CGImage on macOS; Go's standard library has none)
//   meta-read                                    the EXIF and XMP of a camera's JPEG, typed (ImageIO's
//                                                CGImageSourceCopyPropertiesAtIndex on macOS; Go has none)
// The C side (when the libraries were found, SGCL_BENCH_CODEC_LIBS):
// libpng (png_read_row, 16 bits swapped to the machine's order as the module
// gives them; png_write_row with libpng's default filters and level),
// libjpeg-turbo 3 (tj3 with the accurate integer DCT and fancy upsampling,
// the module's own settings), libwebp (WebPDecodeRGBInto), giflib
// (DGifSlurp, the first frame expanded to RGBA through its colour map).
#if defined(SGCL_BENCH_CODEC_LIBS)
#include <gif_lib.h>
#include <png.h>
#include <turbojpeg.h>
#include <webp/decode.h>
#include <webp/encode.h>
#endif
#if defined(__APPLE__) && defined(SGCL_BENCH_CODEC_LIBS)
#include <ImageIO/ImageIO.h>
#include <objc/message.h>
#include <objc/runtime.h>
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void*);
#endif
#if defined(SGCL_BENCH_LIBJXL)
#include <jxl/decode.h>
#include <jxl/resizable_parallel_runner.h>
#endif
#if defined(SGCL_BENCH_LIBTIFF)
#include <tiffio.h>
// giflib's median cut, in the library and not in its header
extern "C" int GifQuantizeBuffer(unsigned int width, unsigned int height, int* color_map_size, GifByteType* red,
                                 GifByteType* green, GifByteType* blue, GifByteType* output, GifColorType* output_map);
#endif

#include "sgcl/codec/codec.h"
#include "sgcl/compress/zlib.h"
#include "sgcl/hash/crc32.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace sgcl;

namespace {
    volatile uint64_t sink;

    std::vector<unsigned char> load(const std::string& p) {
        std::ifstream f(p, std::ios::binary);
        std::vector<unsigned char> v{std::istreambuf_iterator<char>(f), {}};
        if (v.empty()) {
            std::fprintf(stderr, "%s: empty or missing\n", p.c_str());
            std::exit(1);
        }
        return v;
    }

    template<class F>
    void run(const std::string& name, double pixels, F f) {
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();
        while (std::chrono::duration<double>(clock::now() - t0).count() < 0.1) {
            sink += f();
        }
        uint64_t calls = 0;
        double wall = 0;
        t0 = clock::now();
        do {
            sink += f();
            ++calls;
            wall = std::chrono::duration<double>(clock::now() - t0).count();
        } while (wall < 0.4);
        const double ms = wall * 1e3 / double(calls);
        std::printf("%s|%.3f|%.1f\n", name.c_str(), ms, pixels / (ms * 1e3));
    }

    slice<const byte> as_bytes(const std::vector<unsigned char>& v) {
        return slice<const byte>(reinterpret_cast<const byte*>(v.data()), v.size());
    }

    // The file of a case: png-<name> is <name>.png
    std::string file_of(const std::string& c) {
        if (c == "jpeg-base") {
            return "base.jpg";
        }
        if (c == "jpeg-prog") {
            return "prog.jpg";
        }
        if (c == "webp-lossy") {
            return "lossy.webp";
        }
        if (c == "webp-lossless") {
            return "lossless.webp";
        }
        if (c == "tiff-dec") {
            return "lzw.tif";
        }
        if (c == "bmp-dec") {
            return "rgb8.bmp";
        }
        if (c == "qoi-dec") {
            return "rgb8.qoi";
        }
        if (c == "jxl-dec") {
            return "lossy.jxl";
        }
        if (c == "gif") {
            return "image.gif";
        }
        return c.substr(4) + ".png";
    }

    // The URL of qr-enc: 300 characters of a link with a query
    std::string qr_url() {
        std::string s = "https://example.com/search?q=";
        for (int i = 0; s.size() < 300; ++i) {
            s += char('a' + i * 7 % 26);
            if (i % 9 == 8) {
                s += "&p" + std::to_string(i) + "=";
            }
        }
        s.resize(300);
        return s;
    }

    bool encoding(const std::string& c) {
        return c == "png-enc" || c == "jpeg-enc" || c == "jpeg-enc-opt" || c == "gif-enc" || c == "gif-enc-nodither" || c == "webp-enc" ||
               c == "webp-enc-lossless" || c == "tiff-enc" || c == "tiff-enc-deflate" || c == "bmp-enc" || c == "qoi-enc";
    }

    int sgcl_side(const std::string& c, const std::string& dir) {
        if (encoding(c)) {
            codec::image rgb = codec::decode(as_bytes(load(dir + "/rgb8.png")));
            const double pixels = double(rgb.width()) * rgb.height();
            if (c == "png-enc") {
                run(c, pixels, [&] { return uint64_t(codec::png::encode(rgb)->size()); });
            } else if (c == "tiff-enc" || c == "tiff-enc-deflate") {
                const codec::tiff::options o{.compression = c == "tiff-enc" ? codec::tiff::compression::lzw : codec::tiff::compression::deflate};
                run(c, pixels, [&] { return uint64_t(codec::tiff::encode(rgb, o)->size()); });
            } else if (c == "bmp-enc") {
                run(c, pixels, [&] { return uint64_t(codec::bmp::encode(rgb)->size()); });
            } else if (c == "qoi-enc") {
                run(c, pixels, [&] { return uint64_t(codec::qoi::encode(rgb)->size()); });
            } else if (c == "webp-enc" || c == "webp-enc-lossless") {
                const codec::webp::options o{.lossless = c == "webp-enc-lossless", .quality = 75};
                run(c, pixels, [&] { return uint64_t(codec::webp::encode(rgb, o)->size()); });
            } else if (c == "gif-enc" || c == "gif-enc-nodither") {
                const codec::gif::options o{.dither = c == "gif-enc"};
                run(c, pixels, [&] { return uint64_t(codec::gif::encode(rgb, o)->size()); });
            } else {
                const codec::jpeg::options o{.quality = 90, .subsampling = codec::jpeg::subsampling::s420, .optimize = c == "jpeg-enc-opt"};
                run(c, pixels, [&] { return uint64_t(codec::jpeg::encode(rgb, o)->size()); });
            }
            return 0;
        }
        if (c == "qr-enc") {
            const string url(qr_url().c_str());
            run(c, 1, [&] {
                const codec::image im = codec::qr::encode(url)->to_image(1, 1);
                return uint64_t(im.width());
            });
            return 0;
        }
        if (c == "meta-read") {
            const auto file = load(dir + "/meta.jpg");
            run(c, 1, [&] {
                auto m = codec::metadata::read(as_bytes(file));
                return uint64_t(m->iso().value_or(0) + m->make()->size() + (m->location() ? 1 : 0));
            });
            return 0;
        }
        if (c == "gif-enc-exact") {
            codec::image rgba = codec::decode(as_bytes(load(dir + "/image.gif")));
            run(c, double(rgba.width()) * rgba.height(), [&] { return uint64_t(codec::gif::encode(rgba)->size()); });
            return 0;
        }
        const auto file = load(dir + "/" + file_of(c));
        codec::image first = codec::decode(as_bytes(file));
        run(c, double(first.width()) * first.height(), [&] {
            codec::image im = codec::decode(as_bytes(file));
            return uint64_t(im.pixels()[im.pixels().size() / 2]);
        });
        return 0;
    }

    // The inputs: the module's PNG encodings of src.png in four formats,
    // Paeth-filtered and interlaced PNGs made here (every row Paeth, zlib by
    // the compress module), and a baseline JPEG q90 4:2:0 (cjpeg's bytes);
    // run.sh adds the progressive JPEG (jpegtran), the WebPs (cwebp) and the
    // GIF (Go's encoder)
    void write(const std::string& path, const std::string& bytes) {
        std::ofstream(path, std::ios::binary).write(bytes.data(), std::streamsize(bytes.size()));
    }

    void write(const std::string& path, const vector<byte>& bytes) {
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    }

    std::string be32(uint32_t v) {
        return std::string{char(v >> 24), char(v >> 16), char(v >> 8), char(v)};
    }

    // rows of w pixels of bpp bytes, each Paeth-filtered, appended to raw
    void paeth_rows(std::string& raw, const std::vector<uint8_t>& px, size_t w, size_t h, size_t bpp) {
        const size_t row = w * bpp;
        for (size_t y = 0; y < h; ++y) {
            raw.push_back(char(4));
            const uint8_t* cur = px.data() + row * y;
            const uint8_t* up = y ? cur - row : nullptr;
            for (size_t x = 0; x < row; ++x) {
                const int a = x >= bpp ? cur[x - bpp] : 0, b = up ? up[x] : 0, cc = (up && x >= bpp) ? up[x - bpp] : 0;
                const int pa = std::abs(b - cc), pb = std::abs(a - cc), pc = std::abs(a + b - 2 * cc);
                const int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : cc);
                raw.push_back(char(cur[x] - pr));
            }
        }
    }

    // A PNG of 8-bit rgb8 (color 2) or rgba8 (color 6), every row Paeth,
    // Adam7-interlaced when asked
    void write_paeth(const std::string& path, const codec::image& im, int color, bool interlaced) {
        const size_t bpp = im.stride() / im.width(), W = im.width(), H = im.height();
        const auto* all = reinterpret_cast<const uint8_t*>(im.pixels().data());
        std::string raw;
        if (!interlaced) {
            paeth_rows(raw, std::vector<uint8_t>(all, all + W * H * bpp), W, H, bpp);
        } else {
            static const int x0[] = {0, 4, 0, 2, 0, 1, 0}, y0[] = {0, 0, 4, 0, 2, 0, 1};
            static const int dx[] = {8, 8, 4, 4, 2, 2, 1}, dy[] = {8, 8, 8, 4, 4, 2, 2};
            for (int k = 0; k < 7; ++k) {
                const size_t w = W > size_t(x0[k]) ? (W - x0[k] + dx[k] - 1) / dx[k] : 0;
                const size_t h = H > size_t(y0[k]) ? (H - y0[k] + dy[k] - 1) / dy[k] : 0;
                if (!w || !h) {
                    continue;
                }
                std::vector<uint8_t> sub(w * h * bpp);
                for (size_t y = 0; y < h; ++y) {
                    for (size_t x = 0; x < w; ++x) {
                        const uint8_t* from = all + ((y0[k] + y * dy[k]) * W + x0[k] + x * dx[k]) * bpp;
                        std::copy(from, from + bpp, sub.data() + (y * w + x) * bpp);
                    }
                }
                paeth_rows(raw, sub, w, h, bpp);
            }
        }
        const vector<byte> z = compress::zlib::compress(slice<const byte>(reinterpret_cast<const byte*>(raw.data()), raw.size()));
        std::string out("\x89PNG\r\n\x1a\n", 8);
        auto chunk = [&](const char* type, const std::string& body) {
            out += be32(uint32_t(body.size()));
            const std::string tb = std::string(type, 4) + body;
            out += tb;
            out += be32(hash::crc32::of(slice<const byte>(reinterpret_cast<const byte*>(tb.data()), tb.size())));
        };
        std::string ihdr = be32(uint32_t(W)) + be32(uint32_t(H));
        ihdr += char(8);
        ihdr += char(color);
        ihdr += std::string(2, '\0');
        ihdr += char(interlaced ? 1 : 0);
        chunk("IHDR", ihdr);
        chunk("IDAT", std::string(reinterpret_cast<const char*>(z.data()), z.size()));
        chunk("IEND", "");
        write(path, out);
    }

    // A camera's EXIF block (little-endian TIFF structure): IFD0, the Exif
    // IFD and the GPS IFD with the fields metadata types, as a camera
    // writes them
    std::string camera_exif() {
        struct Field {
            uint16_t tag, type;
            uint32_t count;
            std::string data;
        };
        auto u16 = [](uint32_t v) { return std::string{char(v), char(v >> 8)}; };
        auto u32 = [&](uint32_t v) { return u16(v & 0xFFFF) + u16(v >> 16); };
        auto ascii = [](uint16_t tag, const std::string& t) { return Field{tag, 2, uint32_t(t.size() + 1), t + '\0'}; };
        auto rational = [&](uint16_t tag, std::initializer_list<std::pair<uint32_t, uint32_t>> v) {
            std::string d;
            for (auto [a, b] : v) {
                d += u32(a) + u32(b);
            }
            return Field{tag, 5, uint32_t(v.size()), d};
        };
        auto shortv = [&](uint16_t tag, uint32_t v) { return Field{tag, 3, 1, u16(v)}; };
        std::vector<Field> ifd0 = {ascii(0x010F, "Canon"), ascii(0x0110, "Canon EOS R5"), shortv(0x0112, 1), ascii(0x0131, "Firmware 1.8.1"),
                                   ascii(0x0132, "2024:05:06 18:30:00"), ascii(0x013B, "Jan Kowalski"), ascii(0x8298, "(c) 2024")};
        std::vector<Field> exif = {rational(0x829A, {{1, 250}}), rational(0x829D, {{28, 10}}), shortv(0x8827, 400),
                                   ascii(0x9003, "2024:05:06 18:29:41"), ascii(0x9004, "2024:05:06 18:29:41"), ascii(0x9011, "+02:00"),
                                   ascii(0x9291, "25"), shortv(0x9209, 16), rational(0x920A, {{105, 1}}), shortv(0xA405, 105),
                                   ascii(0xA433, "Canon"), ascii(0xA434, "RF24-105mm F4 L IS USM")};
        std::vector<Field> gps = {ascii(1, "N"), rational(2, {{52, 1}, {13, 1}, {3012, 100}}), ascii(3, "E"),
                                  rational(4, {{21, 1}, {0, 1}, {3624, 100}}), rational(6, {{1105, 10}}), rational(7, {{16, 1}, {29, 1}, {40, 1}}),
                                  ascii(0x1D, "2024:05:06")};
        auto size = [](size_t n) { return 2 + 12 * n + 4; };
        const size_t at_exif = 8 + size(ifd0.size() + 2), at_gps = at_exif + size(exif.size());
        ifd0.push_back(Field{0x8769, 4, 1, u32(uint32_t(at_exif))});
        ifd0.push_back(Field{0x8825, 4, 1, u32(uint32_t(at_gps))});
        size_t data = at_gps + size(gps.size());
        std::string out = std::string("II\x2a\0", 4) + u32(8), area;
        for (auto* fields : {&ifd0, &exif, &gps}) {
            std::sort(fields->begin(), fields->end(), [](const Field& a, const Field& b) { return a.tag < b.tag; });
            out += u16(uint32_t(fields->size()));
            for (const Field& f : *fields) {
                out += u16(f.tag) + u16(f.type) + u32(f.count);
                if (f.data.size() <= 4) {
                    out += f.data + std::string(4 - f.data.size(), '\0');
                } else {
                    out += u32(uint32_t(data + area.size()));
                    area += f.data;
                }
            }
            out += u32(0);
        }
        return out + area;
    }

    // An XMP packet as Lightroom writes one beside the EXIF
    const char* camera_xmp =
        "<?xpacket begin=\"\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?><x:xmpmeta xmlns:x=\"adobe:ns:meta/\"><rdf:RDF "
        "xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\"><rdf:Description rdf:about=\"\" "
        "xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\" xmlns:aux=\"http://ns.adobe.com/exif/1.0/aux/\" "
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmp:CreatorTool=\"Adobe Lightroom 13\" xmp:Rating=\"4\" "
        "xmp:ModifyDate=\"2024-05-07T09:00:00+02:00\" aux:Lens=\"RF24-105mm F4 L IS USM\"><dc:creator><rdf:Seq><rdf:li>Jan "
        "Kowalski</rdf:li></rdf:Seq></dc:creator><dc:rights><rdf:Alt><rdf:li xml:lang=\"x-default\">(c) 2024</rdf:li></rdf:Alt>"
        "</dc:rights></rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end=\"w\"?>";

    int prep(const std::string& dir) {
        codec::image rgb = codec::decode(as_bytes(load(dir + "/src.png")), {.want = codec::pixel_format::rgb8});
        write(dir + "/rgb8.png", codec::png::encode(rgb));
        write(dir + "/rgba8.png", codec::png::encode(rgb.convert(codec::pixel_format::rgba8)));
        write(dir + "/rgb16.png", codec::png::encode(rgb.convert(codec::pixel_format::rgb16)));
        write(dir + "/rgba16.png", codec::png::encode(rgb.convert(codec::pixel_format::rgba16)));
        write_paeth(dir + "/paeth-rgb8.png", rgb, 2, false);
        write_paeth(dir + "/paeth-rgba8.png", rgb.convert(codec::pixel_format::rgba8), 6, false);
        write_paeth(dir + "/adam7-rgb8.png", rgb, 2, true);
        write(dir + "/base.jpg", codec::jpeg::encode(rgb, {.quality = 90}));
        write(dir + "/lzw.tif", codec::tiff::encode(rgb));
        write(dir + "/rgb8.bmp", codec::bmp::encode(rgb));
        write(dir + "/rgb8.qoi", codec::qoi::encode(rgb));
        {
            // a camera's JPEG: the EXIF block in APP1, an XMP packet after it
            codec::image photo = rgb;
            const std::string exif = camera_exif();
            photo = rgb.clone();
            photo.set_exif(slice<const byte>(reinterpret_cast<const byte*>(exif.data()), exif.size()));
            vector<byte> jpeg = *codec::jpeg::encode(photo, {.quality = 90});
            const std::string body = std::string("http://ns.adobe.com/xap/1.0/") + '\0' + camera_xmp;
            std::string seg = "\xFF\xE1";
            seg += char((body.size() + 2) >> 8);
            seg += char(body.size() + 2);
            seg += body;
            vector<byte> out;
            out.insert(out.end(), jpeg.begin(), jpeg.begin() + 2);
            out.insert(out.end(), reinterpret_cast<const byte*>(seg.data()), reinterpret_cast<const byte*>(seg.data()) + seg.size());
            out.insert(out.end(), jpeg.begin() + 2, jpeg.end());
            write(dir + "/meta.jpg", out);
        }
        std::printf("%ux%u\n", rgb.width(), rgb.height());
        return 0;
    }

#if defined(SGCL_BENCH_CODEC_LIBS)
    struct PngSource {
        const unsigned char* p;
        size_t n, at;
    };

    void png_read_bytes(png_structp png, png_bytep out, png_size_t len) {
        auto* r = static_cast<PngSource*>(png_get_io_ptr(png));
        if (r->at + len > r->n) {
            png_error(png, "short");
        }
        std::copy_n(r->p + r->at, len, out);
        r->at += len;
    }

    uint64_t png_decode_c(const std::vector<unsigned char>& file, std::vector<unsigned char>& pixels, double& count) {
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        png_infop info = png_create_info_struct(png);
        PngSource r{file.data(), file.size(), 0};
        png_set_read_fn(png, &r, png_read_bytes);
        png_read_info(png, info);
        if (png_get_bit_depth(png, info) == 16) {
            png_set_swap(png);
        }
        const int passes = png_set_interlace_handling(png);
        png_read_update_info(png, info);
        const png_uint_32 w = png_get_image_width(png, info), h = png_get_image_height(png, info);
        const size_t row = png_get_rowbytes(png, info);
        pixels.resize(row * h);
        for (int pass = 0; pass < passes; ++pass) {
            for (png_uint_32 y = 0; y < h; ++y) {
                png_read_row(png, pixels.data() + row * y, nullptr);
            }
        }
        png_read_end(png, nullptr);
        png_destroy_read_struct(&png, &info, nullptr);
        count = double(w) * h;
        return pixels[row * h / 2];
    }

    struct GifSource {
        const unsigned char* p;
        size_t n, at;
    };

    // A GIF of indices and a color map through giflib, into memory
    uint64_t gif_encode_c(int w, int h, const GifByteType* indices, const GifColorType* colors, int count) {
        std::vector<unsigned char> out;
        int err = 0;
        GifFileType* g = EGifOpen(&out, [](GifFileType* f, const GifByteType* b, int n) {
            auto* o = static_cast<std::vector<unsigned char>*>(f->UserData);
            o->insert(o->end(), b, b + n);
            return n;
        }, &err);
        ColorMapObject* map = GifMakeMapObject(256, nullptr);
        std::copy_n(colors, count, map->Colors);
        EGifPutScreenDesc(g, w, h, 8, 0, map);
        EGifPutImageDesc(g, 0, 0, w, h, false, nullptr);
        for (int y = 0; y < h; ++y) {
            EGifPutLine(g, const_cast<GifByteType*>(indices + size_t(y) * w), w);
        }
        EGifCloseFile(g, &err);
        GifFreeMapObject(map);
        return out.size();
    }

#if defined(SGCL_BENCH_LIBTIFF)
    // A TIFF in memory for libtiff, read and written through its client procedures
    struct TiffMemory {
        std::vector<unsigned char> data;
        toff_t at = 0;
    };

    tsize_t tiff_read(thandle_t h, tdata_t buf, tsize_t n) {
        auto* m = static_cast<TiffMemory*>(h);
        const tsize_t k = std::max<tsize_t>(0, std::min<tsize_t>(n, tsize_t(m->data.size()) - tsize_t(m->at)));
        std::copy_n(m->data.data() + m->at, k, static_cast<unsigned char*>(buf));
        m->at += toff_t(k);
        return k;
    }

    tsize_t tiff_write(thandle_t h, tdata_t buf, tsize_t n) {
        auto* m = static_cast<TiffMemory*>(h);
        if (m->at + toff_t(n) > m->data.size()) {
            m->data.resize(m->at + toff_t(n));
        }
        std::copy_n(static_cast<const unsigned char*>(buf), n, m->data.data() + m->at);
        m->at += toff_t(n);
        return n;
    }

    toff_t tiff_seek(thandle_t h, toff_t off, int whence) {
        auto* m = static_cast<TiffMemory*>(h);
        m->at = whence == SEEK_SET ? off : whence == SEEK_CUR ? m->at + off : m->data.size() + off;
        return m->at;
    }

    int tiff_close(thandle_t) {
        return 0;
    }

    toff_t tiff_size(thandle_t h) {
        return static_cast<TiffMemory*>(h)->data.size();
    }

    int tiff_map(thandle_t, tdata_t*, toff_t*) {
        return 0;
    }

    void tiff_unmap(thandle_t, tdata_t, toff_t) {
    }

    TIFF* tiff_open(TiffMemory& m, const char* mode) {
        return TIFFClientOpen("memory", mode, &m, tiff_read, tiff_write, tiff_seek, tiff_close, tiff_size, tiff_map, tiff_unmap);
    }
#endif

    int c_side(const std::string& c, const std::string& dir) {
        std::vector<unsigned char> pixels;
        double count = 0;
#if defined(SGCL_BENCH_LIBJXL)
        if (c == "jxl-dec") {
            // libjxl as djxl runs it: the resizable runner over the
            // machine's threads, RGB of 8 bits out
            const auto file = load(dir + "/lossy.jxl");
            void* runner = JxlResizableParallelRunnerCreate(nullptr);
            auto decode = [&] {
                JxlDecoder* d = JxlDecoderCreate(nullptr);
                JxlDecoderSubscribeEvents(d, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE);
                JxlDecoderSetParallelRunner(d, JxlResizableParallelRunner, runner);
                JxlDecoderSetInput(d, file.data(), file.size());
                JxlDecoderCloseInput(d);
                const JxlPixelFormat format{3, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0};
                JxlBasicInfo info{};
                for (;;) {
                    const JxlDecoderStatus st = JxlDecoderProcessInput(d);
                    if (st == JXL_DEC_BASIC_INFO) {
                        JxlDecoderGetBasicInfo(d, &info);
                        JxlResizableParallelRunnerSetThreads(runner, JxlResizableParallelRunnerSuggestThreads(info.xsize, info.ysize));
                    } else if (st == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
                        size_t n = 0;
                        JxlDecoderImageOutBufferSize(d, &format, &n);
                        pixels.resize(n);
                        JxlDecoderSetImageOutBuffer(d, &format, pixels.data(), n);
                    } else if (st == JXL_DEC_FULL_IMAGE || st == JXL_DEC_SUCCESS) {
                        break;
                    } else {
                        std::exit(3);
                    }
                }
                JxlDecoderDestroy(d);
                count = double(info.xsize) * info.ysize;
                return uint64_t(pixels[pixels.size() / 2]);
            };
            decode();
            run(c, count, decode);
            JxlResizableParallelRunnerDestroy(runner);
            return 0;
        }
#endif
#if defined(__APPLE__)
        if (c == "qr-enc") {
            // CIQRCodeGenerator of the same URL's UTF-8 at M, rendered by a
            // CIContext made once into a CGImage of a pixel a module (its own
            // border of one), through the Objective-C runtime
            const std::string url = qr_url();
            auto send = [](auto f) { return reinterpret_cast<decltype(f)>(objc_msgSend); };
            id context = send((id(*)(id, SEL)) nullptr)(reinterpret_cast<id>(objc_getClass("CIContext")), sel_registerName("context"));
            CFDataRef message = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(url.data()), CFIndex(url.size()));
            run(c, 1, [&] {
                void* pool = objc_autoreleasePoolPush();
                id filter = send((id(*)(id, SEL, id)) nullptr)(reinterpret_cast<id>(objc_getClass("CIFilter")), sel_registerName("filterWithName:"),
                                                                reinterpret_cast<id>(const_cast<__CFString*>(CFSTR("CIQRCodeGenerator"))));
                send((void (*)(id, SEL, id, id)) nullptr)(filter, sel_registerName("setValue:forKey:"), reinterpret_cast<id>(const_cast<__CFData*>(message)),
                                                          reinterpret_cast<id>(const_cast<__CFString*>(CFSTR("inputMessage"))));
                send((void (*)(id, SEL, id, id)) nullptr)(filter, sel_registerName("setValue:forKey:"), reinterpret_cast<id>(const_cast<__CFString*>(CFSTR("M"))),
                                                          reinterpret_cast<id>(const_cast<__CFString*>(CFSTR("inputCorrectionLevel"))));
                id image = send((id(*)(id, SEL)) nullptr)(filter, sel_registerName("outputImage"));
                const CGRect extent = send((CGRect(*)(id, SEL)) nullptr)(image, sel_registerName("extent"));
                CGImageRef cg = send((CGImageRef(*)(id, SEL, id, CGRect)) nullptr)(context, sel_registerName("createCGImage:fromRect:"), image, extent);
                const uint64_t w = CGImageGetWidth(cg);
                CGImageRelease(cg);
                objc_autoreleasePoolPop(pool);
                return w;
            });
            CFRelease(message);
            return 0;
        }
        if (c == "meta-read") {
            // ImageIO's reading of the same file's properties (EXIF, GPS,
            // XMP merged in), the image never decoded
            const auto file = load(dir + "/meta.jpg");
            CFDataRef data = CFDataCreateWithBytesNoCopy(nullptr, file.data(), CFIndex(file.size()), kCFAllocatorNull);
            const void* keys[] = {kCGImageSourceShouldCache};
            const void* values[] = {kCFBooleanFalse};
            CFDictionaryRef opts = CFDictionaryCreate(nullptr, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            run(c, 1, [&] {
                CGImageSourceRef src = CGImageSourceCreateWithData(data, opts);
                CFDictionaryRef props = CGImageSourceCopyPropertiesAtIndex(src, 0, opts);
                const uint64_t n = props ? uint64_t(CFDictionaryGetCount(props)) : 0;
                if (props) {
                    CFRelease(props);
                }
                CFRelease(src);
                return n;
            });
            CFRelease(opts);
            CFRelease(data);
            return 0;
        }
#endif
#if defined(SGCL_BENCH_LIBTIFF)
        if (c == "tiff-dec") {
            TiffMemory m;
            m.data = load(dir + "/lzw.tif");
            uint32_t w = 0, h = 0;
            auto decode = [&] {
                m.at = 0;
                TIFF* t = tiff_open(m, "r");
                TIFFGetField(t, TIFFTAG_IMAGEWIDTH, &w);
                TIFFGetField(t, TIFFTAG_IMAGELENGTH, &h);
                pixels.resize(size_t(w) * h * 3);
                const tstrip_t strips = TIFFNumberOfStrips(t);
                const tsize_t strip = TIFFStripSize(t);
                tsize_t at = 0;
                for (tstrip_t s = 0; s < strips; ++s) {
                    const tsize_t n = TIFFReadEncodedStrip(t, s, pixels.data() + at, std::min<tsize_t>(strip, tsize_t(pixels.size()) - at));
                    if (n < 0) {
                        std::exit(3);
                    }
                    at += n;
                }
                TIFFClose(t);
                return uint64_t(pixels[pixels.size() / 2]);
            };
            decode();
            run(c, double(w) * h, decode);
            return 0;
        }
        if (c == "tiff-enc" || c == "tiff-enc-deflate") {
            const auto file = load(dir + "/rgb8.png");
            png_decode_c(file, pixels, count);
            const uint32_t w = uint32_t(codec::decode(as_bytes(file))->width());
            const uint32_t h = uint32_t(pixels.size() / (3 * size_t(w)));
            const uint32_t rows = std::max<uint32_t>(1, 65536 / (w * 3));
            run(c, double(w) * h, [&] {
                TiffMemory m;
                TIFF* t = tiff_open(m, "w");
                TIFFSetField(t, TIFFTAG_IMAGEWIDTH, w);
                TIFFSetField(t, TIFFTAG_IMAGELENGTH, h);
                TIFFSetField(t, TIFFTAG_SAMPLESPERPIXEL, 3);
                TIFFSetField(t, TIFFTAG_BITSPERSAMPLE, 8);
                TIFFSetField(t, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
                TIFFSetField(t, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
                TIFFSetField(t, TIFFTAG_COMPRESSION, c == "tiff-enc" ? COMPRESSION_LZW : COMPRESSION_ADOBE_DEFLATE);
                TIFFSetField(t, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
                TIFFSetField(t, TIFFTAG_ROWSPERSTRIP, rows);
                for (uint32_t y = 0; y < h; y += rows) {
                    const uint32_t n = std::min(rows, h - y);
                    TIFFWriteEncodedStrip(t, y / rows, pixels.data() + size_t(y) * w * 3, tsize_t(n) * w * 3);
                }
                TIFFClose(t);
                return uint64_t(m.data.size());
            });
            return 0;
        }
#endif
        if (c == "webp-enc" || c == "webp-enc-lossless") {
            const auto file = load(dir + "/rgb8.png");
            png_decode_c(file, pixels, count);
            const int w = int(codec::decode(as_bytes(file))->width());
            const int h = int(pixels.size() / (3 * size_t(w)));
            const bool lossless = c == "webp-enc-lossless";
            run(c, double(w) * h, [&] {
                WebPConfig config;
                WebPConfigInit(&config);
                config.quality = 75;
                config.method = 4;
                config.lossless = lossless;
                WebPPicture picture;
                WebPPictureInit(&picture);
                picture.width = w;
                picture.height = h;
                picture.use_argb = lossless;
                WebPMemoryWriter writer;
                WebPMemoryWriterInit(&writer);
                picture.writer = WebPMemoryWrite;
                picture.custom_ptr = &writer;
                if (!WebPPictureImportRGB(&picture, pixels.data(), w * 3) || !WebPEncode(&config, &picture)) {
                    std::exit(3);
                }
                const uint64_t size = writer.size;
                WebPPictureFree(&picture);
                WebPMemoryWriterClear(&writer);
                return size;
            });
            return 0;
        }
        if (c == "gif-enc-nodither") {
            const auto file = load(dir + "/rgb8.png");
            png_decode_c(file, pixels, count);
            const int w = int(codec::decode(as_bytes(file))->width());
            const int h = int(pixels.size() / (3 * size_t(w)));
            std::vector<GifByteType> r(size_t(w) * h), g(r.size()), b(r.size()), indices(r.size());
            for (size_t i = 0; i < r.size(); ++i) {
                r[i] = pixels[3 * i];
                g[i] = pixels[3 * i + 1];
                b[i] = pixels[3 * i + 2];
            }
            GifColorType colors[256];
            run(c, double(w) * h, [&] {
                int size = 256;
                if (GifQuantizeBuffer(unsigned(w), unsigned(h), &size, r.data(), g.data(), b.data(), indices.data(), colors) != GIF_OK) {
                    std::exit(3);
                }
                return gif_encode_c(w, h, indices.data(), colors, size);
            });
            return 0;
        }
        if (c == "gif-enc-exact") {
            const auto file = load(dir + "/image.gif");
            GifSource m{file.data(), file.size(), 0};
            int err = 0;
            GifFileType* g = DGifOpen(&m, [](GifFileType* f, GifByteType* b, int n) {
                auto* s = static_cast<GifSource*>(f->UserData);
                const size_t k = std::min(size_t(n), s->n - s->at);
                std::copy_n(s->p + s->at, k, b);
                s->at += k;
                return int(k);
            }, &err);
            if (!g || DGifSlurp(g) != GIF_OK) {
                std::exit(3);
            }
            const SavedImage& frame = g->SavedImages[0];
            const ColorMapObject* map = frame.ImageDesc.ColorMap ? frame.ImageDesc.ColorMap : g->SColorMap;
            const int w = g->SWidth, h = g->SHeight;
            run(c, double(w) * h, [&] { return gif_encode_c(w, h, frame.RasterBits, map->Colors, map->ColorCount); });
            DGifCloseFile(g, &err);
            return 0;
        }
        if (c == "png-enc") {
            const auto file = load(dir + "/rgb8.png");
            png_decode_c(file, pixels, count);
            const uint32_t w = uint32_t(codec::decode(as_bytes(file))->width());
            const uint32_t h = uint32_t(pixels.size() / (3 * size_t(w)));
            std::vector<unsigned char> out;
            run(c, double(w) * h, [&] {
                out.clear();
                png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
                png_infop info = png_create_info_struct(png);
                png_set_write_fn(png, &out, [](png_structp p, png_bytep d, png_size_t n) {
                    auto* o = static_cast<std::vector<unsigned char>*>(png_get_io_ptr(p));
                    o->insert(o->end(), d, d + n);
                }, nullptr);
                png_set_IHDR(png, info, w, h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
                png_write_info(png, info);
                for (uint32_t y = 0; y < h; ++y) {
                    png_write_row(png, pixels.data() + size_t(y) * w * 3);
                }
                png_write_end(png, nullptr);
                png_destroy_write_struct(&png, &info);
                return uint64_t(out.size());
            });
            return 0;
        }
        if (c == "jpeg-enc" || c == "jpeg-enc-opt") {
            const auto file = load(dir + "/rgb8.png");
            png_decode_c(file, pixels, count);
            const int w = int(codec::decode(as_bytes(file))->width());
            const int h = int(pixels.size() / (3 * size_t(w)));
            tjhandle t = tj3Init(TJINIT_COMPRESS);
            tj3Set(t, TJPARAM_QUALITY, 90);
            tj3Set(t, TJPARAM_SUBSAMP, TJSAMP_420);
            tj3Set(t, TJPARAM_FASTDCT, 0);
            tj3Set(t, TJPARAM_OPTIMIZE, c == "jpeg-enc-opt" ? 1 : 0);
            run(c, double(w) * h, [&] {
                unsigned char* out = nullptr;
                size_t size = 0;
                if (tj3Compress8(t, pixels.data(), w, w * 3, h, TJPF_RGB, &out, &size) != 0) {
                    std::exit(3);
                }
                tj3Free(out);
                return uint64_t(size);
            });
            return 0;
        }
        const auto file = load(dir + "/" + file_of(c));
        if (c.rfind("png-", 0) == 0) {
            png_decode_c(file, pixels, count);
            run(c, count, [&] { return png_decode_c(file, pixels, count); });
        } else if (c == "jpeg-base" || c == "jpeg-prog") {
            tjhandle t = tj3Init(TJINIT_DECOMPRESS);
            tj3Set(t, TJPARAM_FASTDCT, 0);
            tj3Set(t, TJPARAM_FASTUPSAMPLE, 0);
            tj3DecompressHeader(t, file.data(), file.size());
            const int w = tj3Get(t, TJPARAM_JPEGWIDTH), h = tj3Get(t, TJPARAM_JPEGHEIGHT);
            pixels.resize(size_t(w) * h * 3);
            run(c, double(w) * h, [&] {
                tj3DecompressHeader(t, file.data(), file.size());
                if (tj3Decompress8(t, file.data(), file.size(), pixels.data(), w * 3, TJPF_RGB) != 0) {
                    std::exit(3);
                }
                return uint64_t(pixels[pixels.size() / 2]);
            });
        } else if (c == "webp-lossy" || c == "webp-lossless") {
            int w = 0, h = 0;
            WebPGetInfo(file.data(), file.size(), &w, &h);
            pixels.resize(size_t(w) * h * 3);
            run(c, double(w) * h, [&] {
                if (!WebPDecodeRGBInto(file.data(), file.size(), pixels.data(), pixels.size(), w * 3)) {
                    std::exit(3);
                }
                return uint64_t(pixels[pixels.size() / 2]);
            });
        } else if (c == "gif") {
            auto decode = [&] {
                GifSource m{file.data(), file.size(), 0};
                int err = 0;
                GifFileType* g = DGifOpen(&m, [](GifFileType* f, GifByteType* b, int n) {
                    auto* s = static_cast<GifSource*>(f->UserData);
                    const size_t k = std::min(size_t(n), s->n - s->at);
                    std::copy_n(s->p + s->at, k, b);
                    s->at += k;
                    return int(k);
                }, &err);
                if (!g || DGifSlurp(g) != GIF_OK) {
                    std::exit(3);
                }
                const SavedImage& frame = g->SavedImages[0];
                const ColorMapObject* map = frame.ImageDesc.ColorMap ? frame.ImageDesc.ColorMap : g->SColorMap;
                const size_t w = size_t(g->SWidth), h = size_t(g->SHeight);
                pixels.resize(w * h * 4);
                for (size_t i = 0; i < w * h; ++i) {
                    const GifColorType& col = map->Colors[frame.RasterBits[i]];
                    pixels[4 * i] = col.Red;
                    pixels[4 * i + 1] = col.Green;
                    pixels[4 * i + 2] = col.Blue;
                    pixels[4 * i + 3] = 255;
                }
                count = double(w * h);
                DGifCloseFile(g, &err);
                return uint64_t(pixels[pixels.size() / 2]);
            };
            decode();
            run(c, count, decode);
        } else {
            return 2;
        }
        return 0;
    }
#endif
}

int main(int argc, char** argv) {
    if (argc >= 3 && std::string(argv[1]) == "prep") {
        return prep(argv[2]);
    }
    if (argc < 4) {
        std::fprintf(stderr, "bench_codec <case> <sgcl|c> <data dir>\nbench_codec prep <data dir>\n");
        return 2;
    }
    const std::string c = argv[1], side = argv[2], dir = argv[3];
    if (side == "sgcl") {
        return sgcl_side(c, dir);
    }
#if defined(SGCL_BENCH_CODEC_LIBS)
    if (side == "c") {
        return c_side(c, dir);
    }
#endif
    std::fprintf(stderr, "bench_codec: no side %s here\n", side.c_str());
    return 2;
}
