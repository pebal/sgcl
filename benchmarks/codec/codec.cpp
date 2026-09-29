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
        if (c == "gif") {
            return "image.gif";
        }
        return c.substr(4) + ".png";
    }

    bool encoding(const std::string& c) {
        return c == "png-enc" || c == "jpeg-enc" || c == "jpeg-enc-opt";
    }

    int sgcl_side(const std::string& c, const std::string& dir) {
        if (encoding(c)) {
            codec::image rgb = codec::decode(as_bytes(load(dir + "/rgb8.png")));
            const double pixels = double(rgb.width()) * rgb.height();
            if (c == "png-enc") {
                run(c, pixels, [&] { return uint64_t(codec::png::encode(rgb).size()); });
            } else {
                const codec::jpeg::options o{.quality = 90, .subsampling = codec::jpeg::subsampling::s420, .optimize = c == "jpeg-enc-opt"};
                run(c, pixels, [&] { return uint64_t(codec::jpeg::encode(rgb, o).size()); });
            }
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

    int c_side(const std::string& c, const std::string& dir) {
        std::vector<unsigned char> pixels;
        double count = 0;
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
