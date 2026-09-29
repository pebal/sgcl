/*------------------------------------------------------------------------------
 * SGCL: a C++20 application platform
 * Copyright (c) 2022-2026 Sebastian Nibisz
 * SPDX-License-Identifier: Apache-2.0
 *------------------------------------------------------------------------------
 * The HEIF oracle of the codec tests (macOS): ImageIO's own reading of a
 * file, by the way every program on the system reads it (CoreGraphics draws
 * the image into a bitmap), against which the module's wrapping of the same
 * ImageIO is held: the size, the depth, alpha, orientation, the profile.
 *
 *   codec_oracle_heif heif <file>   the first image drawn into RGBA of the
 *                                   file's depth (8, or 16 when it has more)
 *                                   in its RGB color space (sRGB for gray),
 *                                   alpha premultiplied as a bitmap context
 *                                   holds it: "W H D", then the pixels
 *                                   (16-bit channels big-endian)
 *   codec_oracle_heif meta <file>   "orientation O", "depth D", "alpha A",
 *                                   "icc N" (the profile's bytes), "type T"
 *   codec_oracle_heif make <in> <out> <type> <quality>
 *                                   the first image of <in> written by ImageIO
 *                                   as <type> (public.heic, public.avif) at a
 *                                   quality of 1..100; exit 1 when the system
 *                                   has no encoder of that type
 *
 * Built by the tests (tests/codec/oracle.h):
 *   cc -O2 tools/codec_oracle_heif.c -framework ImageIO -framework CoreGraphics -framework CoreFoundation
 * A file ImageIO refuses: "ERROR" on stderr, exit 1.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CGImageSourceRef open_source(const char* path) {
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)path, (CFIndex)strlen(path), false);
    CGImageSourceRef src = url ? CGImageSourceCreateWithURL(url, NULL) : NULL;
    if (url) {
        CFRelease(url);
    }
    if (!src || CGImageSourceGetStatus(src) != kCGImageStatusComplete || CGImageSourceGetCount(src) == 0) {
        fprintf(stderr, "ERROR ImageIO refuses the file\n");
        exit(1);
    }
    return src;
}

static int has_alpha(CGImageRef img) {
    CGImageAlphaInfo a = CGImageGetAlphaInfo(img);
    return a != kCGImageAlphaNone && a != kCGImageAlphaNoneSkipFirst && a != kCGImageAlphaNoneSkipLast;
}

static int decode(const char* path) {
    CGImageSourceRef src = open_source(path);
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    if (!img) {
        fprintf(stderr, "ERROR no image\n");
        return 1;
    }
    const size_t w = CGImageGetWidth(img), h = CGImageGetHeight(img);
    const int deep = CGImageGetBitsPerComponent(img) > 8;
    CGColorSpaceRef space = CGImageGetColorSpace(img);
    CGColorSpaceRef own = NULL;
    if (!space || CGColorSpaceGetModel(space) != kCGColorSpaceModelRGB) {
        own = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        space = own;
    }
    const size_t bpc = deep ? 16 : 8, stride = w * 4 * (deep ? 2 : 1);
    unsigned char* px = calloc(stride, h);
    CGBitmapInfo info = (has_alpha(img) ? kCGImageAlphaPremultipliedLast : kCGImageAlphaNoneSkipLast) | (deep ? kCGBitmapByteOrder16Little : 0);
    CGContextRef ctx = CGBitmapContextCreate(px, w, h, bpc, stride, space, info);
    if (!ctx) {
        fprintf(stderr, "ERROR no bitmap context\n");
        return 1;
    }
    CGContextDrawImage(ctx, CGRectMake(0, 0, (CGFloat)w, (CGFloat)h), img);
    printf("%zu %zu %d\n", w, h, deep ? 16 : 8);
    for (size_t i = 0; i < w * h; ++i) {
        if (deep) {
            unsigned char* p = px + i * 8;
            if (!has_alpha(img)) {
                p[6] = p[7] = 0xFF;
            }
            for (int c = 0; c < 4; ++c) {
                const unsigned char out[2] = {p[2 * c + 1], p[2 * c]};
                fwrite(out, 1, 2, stdout);
            }
        } else {
            unsigned char* p = px + i * 4;
            if (!has_alpha(img)) {
                p[3] = 0xFF;
            }
            fwrite(p, 1, 4, stdout);
        }
    }
    CGContextRelease(ctx);
    free(px);
    if (own) {
        CGColorSpaceRelease(own);
    }
    CGImageRelease(img);
    CFRelease(src);
    return 0;
}

static int meta(const char* path) {
    CGImageSourceRef src = open_source(path);
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    if (!img) {
        fprintf(stderr, "ERROR no image\n");
        return 1;
    }
    int orientation = 1;
    CFDictionaryRef props = CGImageSourceCopyPropertiesAtIndex(src, 0, NULL);
    if (props) {
        CFNumberRef n = CFDictionaryGetValue(props, kCGImagePropertyOrientation);
        if (n) {
            CFNumberGetValue(n, kCFNumberIntType, &orientation);
        }
        CFRelease(props);
    }
    long icc = 0;
    CGColorSpaceRef space = CGImageGetColorSpace(img);
    CFDataRef data = space ? CGColorSpaceCopyICCData(space) : NULL;
    if (data) {
        icc = (long)CFDataGetLength(data);
        CFRelease(data);
    }
    char type[128] = "";
    CFStringGetCString(CGImageSourceGetType(src), type, sizeof type, kCFStringEncodingUTF8);
    printf("orientation %d\ndepth %zu\nalpha %d\nicc %ld\ntype %s\n", orientation, CGImageGetBitsPerComponent(img), has_alpha(img), icc, type);
    CGImageRelease(img);
    CFRelease(src);
    return 0;
}

static int make(const char* in, const char* out, const char* type_name, int quality) {
    CGImageSourceRef src = open_source(in);
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)out, (CFIndex)strlen(out), false);
    CFStringRef type = CFStringCreateWithCString(NULL, type_name, kCFStringEncodingUTF8);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, type, 1, NULL);
    if (!img || !dst) {
        fprintf(stderr, "ERROR no encoder of %s\n", type_name);
        return 1;
    }
    const double q = quality / 100.0;
    CFNumberRef qn = CFNumberCreate(NULL, kCFNumberDoubleType, &q);
    const void* keys[] = {kCGImageDestinationLossyCompressionQuality};
    const void* values[] = {qn};
    CFDictionaryRef props = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CGImageDestinationAddImage(dst, img, props);
    const int ok = CGImageDestinationFinalize(dst);
    CFRelease(props);
    CFRelease(qn);
    CFRelease(dst);
    CFRelease(type);
    CFRelease(url);
    CGImageRelease(img);
    CFRelease(src);
    if (!ok) {
        fprintf(stderr, "ERROR the encoder failed\n");
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "heif") == 0) {
        return decode(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "meta") == 0) {
        return meta(argv[2]);
    }
    if (argc == 6 && strcmp(argv[1], "make") == 0) {
        return make(argv[2], argv[3], argv[4], atoi(argv[5]));
    }
    fprintf(stderr, "usage: codec_oracle_heif heif|meta <file> | make <in> <out> <type> <quality>\n");
    return 2;
}
