/*------------------------------------------------------------------------------
 * SGCL: a C++20 application platform
 * Copyright (c) 2022-2026 Sebastian Nibisz
 * SPDX-License-Identifier: Apache-2.0
 *------------------------------------------------------------------------------
 * The C oracle of the codec tests: a file decoded by the reference library of
 * its format, its pixels written to stdout in one form for every file, so
 * that a test compares them with the module's byte for byte.
 *
 *   codec_oracle png <file>      libpng, each channel at the file's depth
 *   codec_oracle png8 <file>     libpng, 16-bit channels scaled to 8
 *                                (png_set_scale_16: the nearest value)
 *   codec_oracle jpeg <file>     libjpeg(-turbo), what `djpeg -dct int` does:
 *                                the integer IDCT, fancy upsampling; gray as
 *                                gray, YCbCr and RGB as RGB, CMYK and YCCK
 *                                as CMYK (the values as libjpeg gives them,
 *                                Adobe's inversion not undone). Its output:
 *                                "W H C" (C the channels), the samples.
 *   codec_oracle jpegns <file>   the same with block smoothing off (what a
 *                                progression that stops before its low
 *                                coefficients are whole decodes to without
 *                                libjpeg's smoothing of it)
 *                                Built without it (CODEC_ORACLE_NO_JPEG)
 *                                when there is no libjpeg.
 *   codec_oracle gif <file>      giflib (DGifSlurp): every frame composed on
 *                                a canvas of the logical screen, as the
 *                                module composes them (transparent start;
 *                                disposal 2 clears the rectangle, 3 puts back
 *                                what was under it; the transparent index left
 *                                as it was); the output "W H 8", then the
 *                                frames RGBA one after another
 *   codec_oracle gifmeta <file>  giflib: "plays P" (0 forever) and "delay D"
 *                                of each frame, hundredths of a second
 *                                Built without it (CODEC_ORACLE_NO_GIF) when
 *                                there is no giflib.
 *   codec_oracle webp <file>     libwebp (WebPDecodeRGBA): a still image,
 *                                or an animation's first frame by itself
 *   codec_oracle webpyuv <file>  libwebp (WebPDecodeYUV): a lossy image's
 *                                planes as it decodes them, before any
 *                                color conversion: "W H 8", then the rows of
 *                                Y (W bytes each), of U and of V ((W + 1) / 2
 *                                bytes each, (H + 1) / 2 rows)
 *   codec_oracle webpanim <file> libwebp's WebPAnimDecoder (libwebpdemux,
 *                                the demuxer's checks of the container):
 *                                every frame on the canvas, "W H 8" and the
 *                                canvases RGBA one after another
 *   codec_oracle webpfirst <file> the same, the first frame alone: what a
 *                                reader of the first frame decodes (the
 *                                demuxer's checks of the whole file, then
 *                                one frame)
 *   codec_oracle webpmeta <file> the same: "plays P" (0 forever), then
 *                                "delay D" of each frame, milliseconds
 *                                Built without them (CODEC_ORACLE_NO_WEBP)
 *                                when there is no libwebp.
 *   codec_oracle pngenc <level> <w> <h> <color> <depth> <raw> <out>
 *                                libpng writes the rows of <raw> (PNG's own
 *                                layout, 16-bit samples big-endian) as a PNG
 *                                of that type at that zlib level, its own
 *                                defaults otherwise (every filter, the
 *                                heuristic of least sum; Z_FILTERED), to <out>
 *
 * The output: a line "W H D" (D the bits of a channel, 8 or 16), then H rows
 * of W pixels RGBA, a 16-bit channel big-endian. Every image is expanded to
 * that: a palette to its colors, gray of 1, 2 and 4 bits to 8, gray to
 * three channels, tRNS to alpha, an opaque alpha added where there is none.
 * A file the library refuses: a line "ERROR <what>" on stderr, exit 1.
 *
 * Built by the tests (tests/codec/oracle.h) with the system's compiler
 * against Homebrew's libpng; skipped when either is missing.
 *----------------------------------------------------------------------------*/
#include <png.h>
#ifndef CODEC_ORACLE_NO_JPEG
#include <jpeglib.h>
#endif
#ifndef CODEC_ORACLE_NO_GIF
#include <gif_lib.h>
#endif
#ifndef CODEC_ORACLE_NO_WEBP
#include <webp/decode.h>
#include <webp/demux.h>
#endif

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_error(png_structp p, png_const_charp what) {
    fprintf(stderr, "ERROR %s\n", what);
    png_longjmp(p, 1);
}

static void on_warning(png_structp p, png_const_charp what) {
    (void)p;
    (void)what;
}

static int decode_png(const char* path, int scale_to_8) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR cannot open %s\n", path);
        return 1;
    }
    png_structp p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, on_error, on_warning);
    png_infop info = png_create_info_struct(p);
    png_bytep pixels = NULL;
    png_bytepp rows = NULL;
    if (setjmp(png_jmpbuf(p))) {
        png_destroy_read_struct(&p, &info, NULL);
        free(pixels);
        free(rows);
        fclose(f);
        return 1;
    }
    png_init_io(p, f);
    png_read_info(p, info);
    png_uint_32 w = png_get_image_width(p, info);
    png_uint_32 h = png_get_image_height(p, info);
    int depth = png_get_bit_depth(p, info);
    int color = png_get_color_type(p, info);
    png_set_expand(p);                       /* palette to RGB, low-bit gray to 8, tRNS to alpha */
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(p);
    }
    int out_depth = depth == 16 && !scale_to_8 ? 16 : 8;
    if (depth == 16 && scale_to_8) {
        png_set_scale_16(p);
    }
    png_set_add_alpha(p, out_depth == 16 ? 0xFFFF : 0xFF, PNG_FILLER_AFTER);   /* where there is none */
    png_set_interlace_handling(p);
    png_read_update_info(p, info);
    size_t row = png_get_rowbytes(p, info);
    if (row != (size_t)w * 4 * (out_depth / 8)) {
        fprintf(stderr, "ERROR unexpected row size %zu\n", row);
        png_longjmp(p, 1);
    }
    pixels = malloc(row * h);
    rows = malloc(sizeof(png_bytep) * h);
    for (png_uint_32 y = 0; y < h; ++y) {
        rows[y] = pixels + row * y;
    }
    png_read_image(p, rows);
    png_read_end(p, NULL);
    printf("%u %u %d\n", (unsigned)w, (unsigned)h, out_depth);
    fwrite(pixels, 1, row * h, stdout);
    png_destroy_read_struct(&p, &info, NULL);
    free(pixels);
    free(rows);
    fclose(f);
    return 0;
}

static int encode_png(int level, png_uint_32 w, png_uint_32 h, int color, int depth, const char* raw_path, const char* out_path) {
    int channels = color == 0 ? 1 : color == 4 ? 2 : color == 2 ? 3 : 4;
    size_t row = (size_t)w * channels * (depth / 8);
    FILE* in = fopen(raw_path, "rb");
    if (!in) {
        fprintf(stderr, "ERROR cannot open %s\n", raw_path);
        return 1;
    }
    png_bytep pixels = malloc(row * h);
    if (fread(pixels, 1, row * h, in) != row * h) {
        fprintf(stderr, "ERROR short raw file\n");
        fclose(in);
        free(pixels);
        return 1;
    }
    fclose(in);
    FILE* out = fopen(out_path, "wb");
    if (!out) {
        fprintf(stderr, "ERROR cannot create %s\n", out_path);
        free(pixels);
        return 1;
    }
    png_structp p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, on_error, on_warning);
    png_infop info = png_create_info_struct(p);
    png_bytepp rows = malloc(sizeof(png_bytep) * h);
    if (setjmp(png_jmpbuf(p))) {
        png_destroy_write_struct(&p, &info);
        free(pixels);
        free(rows);
        fclose(out);
        return 1;
    }
    png_init_io(p, out);
    png_set_compression_level(p, level);
    png_set_IHDR(p, info, w, h, depth, color, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    for (png_uint_32 y = 0; y < h; ++y) {
        rows[y] = pixels + row * y;
    }
    png_set_rows(p, info, rows);
    png_write_png(p, info, PNG_TRANSFORM_IDENTITY, NULL);
    png_destroy_write_struct(&p, &info);
    free(pixels);
    free(rows);
    fclose(out);
    return 0;
}

#ifndef CODEC_ORACLE_NO_JPEG
struct jpeg_failure {
    struct jpeg_error_mgr base;
    jmp_buf jump;
};

static void on_jpeg_error(j_common_ptr c) {
    char text[JMSG_LENGTH_MAX];
    c->err->format_message(c, text);
    fprintf(stderr, "ERROR %s\n", text);
    longjmp(((struct jpeg_failure*)c->err)->jump, 1);
}

static void on_jpeg_message(j_common_ptr c, int level) {
    (void)c;
    (void)level;
}

static int decode_jpeg(const char* path, int smoothing) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR cannot open %s\n", path);
        return 1;
    }
    struct jpeg_decompress_struct d;
    struct jpeg_failure e;
    unsigned char* pixels = NULL;
    d.err = jpeg_std_error(&e.base);
    e.base.error_exit = on_jpeg_error;
    e.base.emit_message = on_jpeg_message;
    if (setjmp(e.jump)) {
        jpeg_destroy_decompress(&d);
        free(pixels);
        fclose(f);
        return 1;
    }
    jpeg_create_decompress(&d);
    jpeg_stdio_src(&d, f);
    jpeg_read_header(&d, TRUE);
    d.dct_method = JDCT_ISLOW;
    d.do_fancy_upsampling = TRUE;
    d.do_block_smoothing = smoothing ? TRUE : FALSE;
    switch (d.num_components) {
        case 1: d.out_color_space = JCS_GRAYSCALE; break;
        case 4: d.out_color_space = JCS_CMYK; break;
        default: d.out_color_space = JCS_RGB; break;
    }
    jpeg_start_decompress(&d);
    size_t row = (size_t)d.output_width * d.output_components;
    pixels = malloc(row * d.output_height);
    while (d.output_scanline < d.output_height) {
        unsigned char* line = pixels + row * d.output_scanline;
        jpeg_read_scanlines(&d, &line, 1);
    }
    jpeg_finish_decompress(&d);
    printf("%u %u %d\n", d.output_width, d.output_height, d.output_components);
    fwrite(pixels, 1, row * d.output_height, stdout);
    jpeg_destroy_decompress(&d);
    free(pixels);
    fclose(f);
    return 0;
}
#endif

#ifndef CODEC_ORACLE_NO_GIF
static int decode_gif(const char* path, int meta) {
    int err = 0;
    GifFileType* g = DGifOpenFileName(path, &err);
    if (!g) {
        fprintf(stderr, "ERROR open %d\n", err);
        return 1;
    }
    if (DGifSlurp(g) != GIF_OK) {
        fprintf(stderr, "ERROR slurp %d\n", g->Error);
        DGifCloseFile(g, &err);
        return 1;
    }
    int w = g->SWidth, h = g->SHeight;
    if (meta) {
        long plays = 1;
        for (int i = 0; i < g->ImageCount; ++i) {
            SavedImage* s = &g->SavedImages[i];
            for (int k = 0; k + 1 < s->ExtensionBlockCount; ++k) {
                ExtensionBlock* e = &s->ExtensionBlocks[k];
                if (e->Function == APPLICATION_EXT_FUNC_CODE && e->ByteCount == 11 &&
                    (memcmp(e->Bytes, "NETSCAPE2.0", 11) == 0 || memcmp(e->Bytes, "ANIMEXTS1.0", 11) == 0)) {
                    ExtensionBlock* d = &s->ExtensionBlocks[k + 1];
                    if (d->ByteCount >= 3 && d->Bytes[0] == 1) {
                        int n = d->Bytes[1] | d->Bytes[2] << 8;
                        plays = n == 0 ? 0 : n + 1;
                    }
                }
            }
        }
        printf("plays %ld\n", plays);
        for (int i = 0; i < g->ImageCount; ++i) {
            GraphicsControlBlock gcb = {0, 0, 0, NO_TRANSPARENT_COLOR};
            DGifSavedExtensionToGCB(g, i, &gcb);
            printf("delay %d\n", gcb.DelayTime);
        }
        DGifCloseFile(g, &err);
        return 0;
    }
    size_t canvas_size = (size_t)w * h * 4;
    unsigned char* canvas = calloc(canvas_size, 1);
    unsigned char* saved = malloc(canvas_size);
    unsigned char* out = malloc(canvas_size * (g->ImageCount ? g->ImageCount : 1));
    int last_disposal = 0, ll = 0, lt = 0, lr = 0, lb = 0;
    for (int i = 0; i < g->ImageCount; ++i) {
        SavedImage* s = &g->SavedImages[i];
        if (last_disposal == 2 || last_disposal == 3) {
            for (int y = lt; y < lb; ++y) {
                unsigned char* d = canvas + ((size_t)y * w + ll) * 4;
                if (last_disposal == 2) {
                    memset(d, 0, (size_t)(lr - ll) * 4);
                } else {
                    memcpy(d, saved + ((size_t)y * w + ll) * 4, (size_t)(lr - ll) * 4);
                }
            }
        }
        GraphicsControlBlock gcb = {0, 0, 0, NO_TRANSPARENT_COLOR};
        DGifSavedExtensionToGCB(g, i, &gcb);
        int disposal = gcb.DisposalMode > 3 ? 1 : gcb.DisposalMode;
        ColorMapObject* map = s->ImageDesc.ColorMap ? s->ImageDesc.ColorMap : g->SColorMap;
        if (!map) {
            fprintf(stderr, "ERROR no color table\n");
            return 1;
        }
        int left = s->ImageDesc.Left, top = s->ImageDesc.Top, fw = s->ImageDesc.Width, fh = s->ImageDesc.Height;
        int l = left < w ? left : w, t = top < h ? top : h;
        int r = left + fw < w ? left + fw : w, b = top + fh < h ? top + fh : h;
        if (disposal == 3) {
            for (int y = t; y < b; ++y) {
                memcpy(saved + ((size_t)y * w + l) * 4, canvas + ((size_t)y * w + l) * 4, (size_t)(r - l) * 4);
            }
        }
        for (int y = 0; y < fh; ++y) {
            if (top + y >= h) {
                continue;
            }
            for (int x = 0; x < fw && left + x < w; ++x) {
                int idx = s->RasterBits[(size_t)y * fw + x];
                if (gcb.TransparentColor != NO_TRANSPARENT_COLOR && idx == gcb.TransparentColor) {
                    continue;
                }
                unsigned char* q = canvas + ((size_t)(top + y) * w + left + x) * 4;
                if (idx < map->ColorCount) {
                    q[0] = map->Colors[idx].Red;
                    q[1] = map->Colors[idx].Green;
                    q[2] = map->Colors[idx].Blue;
                } else {
                    q[0] = q[1] = q[2] = 0;
                }
                q[3] = 255;
            }
        }
        memcpy(out + canvas_size * i, canvas, canvas_size);
        last_disposal = disposal;
        ll = l;
        lt = t;
        lr = r;
        lb = b;
    }
    printf("%d %d 8\n", w, h);
    fwrite(out, 1, canvas_size * g->ImageCount, stdout);
    free(canvas);
    free(saved);
    free(out);
    DGifCloseFile(g, &err);
    return 0;
}
#endif

#ifndef CODEC_ORACLE_NO_WEBP
static unsigned char* read_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* data = malloc(n > 0 ? (size_t)n : 1);
    *size = fread(data, 1, (size_t)n, f);
    fclose(f);
    return data;
}

static int decode_webp(const char* path) {
    size_t size = 0;
    unsigned char* data = read_file(path, &size);
    if (!data) {
        fprintf(stderr, "ERROR open\n");
        return 1;
    }
    int w = 0, h = 0;
    uint8_t* rgba = WebPDecodeRGBA(data, size, &w, &h);
    free(data);
    if (!rgba) {
        fprintf(stderr, "ERROR decode\n");
        return 1;
    }
    printf("%d %d 8\n", w, h);
    fwrite(rgba, 1, (size_t)w * h * 4, stdout);
    WebPFree(rgba);
    return 0;
}

static int decode_webp_yuv(const char* path) {
    size_t size = 0;
    unsigned char* data = read_file(path, &size);
    if (!data) {
        fprintf(stderr, "ERROR open\n");
        return 1;
    }
    int w = 0, h = 0, stride = 0, uv_stride = 0;
    uint8_t *u = NULL, *v = NULL;
    uint8_t* y = WebPDecodeYUV(data, size, &w, &h, &u, &v, &stride, &uv_stride);
    free(data);
    if (!y) {
        fprintf(stderr, "ERROR decode\n");
        return 1;
    }
    printf("%d %d 8\n", w, h);
    for (int r = 0; r < h; ++r) {
        fwrite(y + (size_t)r * stride, 1, (size_t)w, stdout);
    }
    for (int r = 0; r < (h + 1) / 2; ++r) {
        fwrite(u + (size_t)r * uv_stride, 1, (size_t)(w + 1) / 2, stdout);
    }
    for (int r = 0; r < (h + 1) / 2; ++r) {
        fwrite(v + (size_t)r * uv_stride, 1, (size_t)(w + 1) / 2, stdout);
    }
    WebPFree(y);
    return 0;
}

static int decode_webp_anim(const char* path, int meta, int first_only) {
    size_t size = 0;
    unsigned char* data = read_file(path, &size);
    if (!data) {
        fprintf(stderr, "ERROR open\n");
        return 1;
    }
    WebPData wd = {data, size};
    WebPAnimDecoderOptions o;
    WebPAnimDecoderOptionsInit(&o);
    o.color_mode = MODE_RGBA;
    o.use_threads = 0;
    WebPAnimDecoder* d = WebPAnimDecoderNew(&wd, &o);
    if (!d) {
        free(data);
        fprintf(stderr, "ERROR demux\n");
        return 1;
    }
    WebPAnimInfo info;
    WebPAnimDecoderGetInfo(d, &info);
    size_t canvas = (size_t)info.canvas_width * info.canvas_height * 4;
    unsigned char* out = malloc(canvas * (info.frame_count ? info.frame_count : 1));
    int* delays = malloc(sizeof(int) * (info.frame_count ? info.frame_count : 1));
    int frames = 0, prev = 0;
    while (WebPAnimDecoderHasMoreFrames(d) && !(first_only && frames == 1)) {
        uint8_t* buf;
        int ts;
        if (!WebPAnimDecoderGetNext(d, &buf, &ts)) {
            WebPAnimDecoderDelete(d);
            free(data);
            fprintf(stderr, "ERROR frame %d\n", frames);
            return 1;
        }
        memcpy(out + canvas * frames, buf, canvas);
        delays[frames++] = ts - prev;
        prev = ts;
    }
    if (meta) {
        printf("plays %u\n", info.loop_count);
        for (int i = 0; i < frames; ++i) {
            printf("delay %d\n", delays[i]);
        }
    } else {
        printf("%u %u 8\n", info.canvas_width, info.canvas_height);
        fwrite(out, 1, canvas * frames, stdout);
    }
    free(out);
    free(delays);
    WebPAnimDecoderDelete(d);
    free(data);
    return 0;
}
#endif

int main(int argc, char** argv) {
#ifndef CODEC_ORACLE_NO_WEBP
    if (argc == 3 && strcmp(argv[1], "webp") == 0) {
        return decode_webp(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "webpyuv") == 0) {
        return decode_webp_yuv(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "webpanim") == 0) {
        return decode_webp_anim(argv[2], 0, 0);
    }
    if (argc == 3 && strcmp(argv[1], "webpfirst") == 0) {
        return decode_webp_anim(argv[2], 0, 1);
    }
    if (argc == 3 && strcmp(argv[1], "webpmeta") == 0) {
        return decode_webp_anim(argv[2], 1, 0);
    }
#endif
#ifndef CODEC_ORACLE_NO_GIF
    if (argc == 3 && strcmp(argv[1], "gif") == 0) {
        return decode_gif(argv[2], 0);
    }
    if (argc == 3 && strcmp(argv[1], "gifmeta") == 0) {
        return decode_gif(argv[2], 1);
    }
#endif
#ifndef CODEC_ORACLE_NO_JPEG
    if (argc == 3 && strcmp(argv[1], "jpeg") == 0) {
        return decode_jpeg(argv[2], 1);
    }
    if (argc == 3 && strcmp(argv[1], "jpegns") == 0) {
        return decode_jpeg(argv[2], 0);
    }
#endif
    if (argc == 9 && strcmp(argv[1], "pngenc") == 0) {
        return encode_png(atoi(argv[2]), (png_uint_32)strtoul(argv[3], NULL, 10), (png_uint_32)strtoul(argv[4], NULL, 10),
                          atoi(argv[5]), atoi(argv[6]), argv[7], argv[8]);
    }
    if (argc != 3) {
        fprintf(stderr, "usage: codec_oracle png|png8 <file> | pngenc <level> <w> <h> <color> <depth> <raw> <out>\n");
        return 2;
    }
    if (strcmp(argv[1], "png") == 0) {
        return decode_png(argv[2], 0);
    }
    if (strcmp(argv[1], "png8") == 0) {
        return decode_png(argv[2], 1);
    }
    fprintf(stderr, "ERROR unknown mode %s\n", argv[1]);
    return 2;
}
