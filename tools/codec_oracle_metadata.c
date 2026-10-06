/*------------------------------------------------------------------------------
 * SGCL: a C++20 application platform
 * Copyright (c) 2022-2026 Sebastian Nibisz
 * SPDX-License-Identifier: Apache-2.0
 *------------------------------------------------------------------------------
 * The metadata oracle of the codec tests (macOS): ImageIO's reading of a
 * file's EXIF and XMP, CGImageSourceCopyPropertiesAtIndex, what sips and
 * mdls show. ImageIO merges a file's XMP into the same dictionaries, so
 * one view holds both, as codec::metadata gives one.
 *
 *   codec_oracle_metadata <file>   one line "key=value" for each property
 *                                  ImageIO has, of the keys below; numbers
 *                                  as %.10g, texts as they are
 *
 * Keys: make, model, software, artist, copyright, description, modified
 * ({TIFF}); original, digitized, offset_original, offset_digitized,
 * offset_modified, subsec_original, subsec_digitized, subsec_modified,
 * exposure, fnumber, iso (the first of ISOSpeedRatings), bias, focal,
 * focal35, flash, lens_make, lens_model, width, height ({Exif}); lat,
 * lat_ref, lon, lon_ref, alt, alt_ref, gps_date, gps_time ({GPS});
 * orientation (the top level).
 *
 * Built by the tests (tests/codec/oracle.h):
 *   cc -O2 tools/codec_oracle_metadata.c -framework ImageIO -framework CoreFoundation
 * A file ImageIO refuses: "ERROR" on stderr, exit 1.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>
#include <stdio.h>
#include <string.h>

static void put(const char* key, CFTypeRef v) {
    if (!v) {
        return;
    }
    if (CFGetTypeID(v) == CFArrayGetTypeID()) {
        if (CFArrayGetCount((CFArrayRef)v) == 0) {
            return;
        }
        v = CFArrayGetValueAtIndex((CFArrayRef)v, 0);
    }
    if (CFGetTypeID(v) == CFStringGetTypeID()) {
        char buf[8192];
        if (CFStringGetCString((CFStringRef)v, buf, sizeof(buf), kCFStringEncodingUTF8)) {
            printf("%s=%s\n", key, buf);
        }
    } else if (CFGetTypeID(v) == CFNumberGetTypeID()) {
        double d;
        if (CFNumberGetValue((CFNumberRef)v, kCFNumberDoubleType, &d)) {
            printf("%s=%.10g\n", key, d);
        }
    }
}

static void from(CFDictionaryRef dict, CFStringRef name, const char* key) {
    if (dict) {
        put(key, CFDictionaryGetValue(dict, name));
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: codec_oracle_metadata <file>\n");
        return 2;
    }
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8*)argv[1], (CFIndex)strlen(argv[1]), false);
    CGImageSourceRef src = url ? CGImageSourceCreateWithURL(url, NULL) : NULL;
    CFDictionaryRef props = src && CGImageSourceGetCount(src) > 0 ? CGImageSourceCopyPropertiesAtIndex(src, 0, NULL) : NULL;
    if (!props) {
        fprintf(stderr, "ERROR\n");
        return 1;
    }
    CFDictionaryRef tiff = CFDictionaryGetValue(props, kCGImagePropertyTIFFDictionary);
    CFDictionaryRef exif = CFDictionaryGetValue(props, kCGImagePropertyExifDictionary);
    CFDictionaryRef gps = CFDictionaryGetValue(props, kCGImagePropertyGPSDictionary);
    from(tiff, kCGImagePropertyTIFFMake, "make");
    from(tiff, kCGImagePropertyTIFFModel, "model");
    from(tiff, kCGImagePropertyTIFFSoftware, "software");
    from(tiff, kCGImagePropertyTIFFArtist, "artist");
    from(tiff, kCGImagePropertyTIFFCopyright, "copyright");
    from(tiff, kCGImagePropertyTIFFImageDescription, "description");
    from(tiff, kCGImagePropertyTIFFDateTime, "modified");
    from(exif, kCGImagePropertyExifDateTimeOriginal, "original");
    from(exif, kCGImagePropertyExifDateTimeDigitized, "digitized");
    from(exif, kCGImagePropertyExifOffsetTimeOriginal, "offset_original");
    from(exif, kCGImagePropertyExifOffsetTimeDigitized, "offset_digitized");
    from(exif, kCGImagePropertyExifOffsetTime, "offset_modified");
    from(exif, kCGImagePropertyExifSubsecTimeOriginal, "subsec_original");
    from(exif, kCGImagePropertyExifSubsecTimeDigitized, "subsec_digitized");
    from(exif, kCGImagePropertyExifSubsecTime, "subsec_modified");
    from(exif, kCGImagePropertyExifExposureTime, "exposure");
    from(exif, kCGImagePropertyExifFNumber, "fnumber");
    from(exif, kCGImagePropertyExifISOSpeedRatings, "iso");
    from(exif, kCGImagePropertyExifExposureBiasValue, "bias");
    from(exif, kCGImagePropertyExifFocalLength, "focal");
    from(exif, kCGImagePropertyExifFocalLenIn35mmFilm, "focal35");
    from(exif, kCGImagePropertyExifFlash, "flash");
    from(exif, kCGImagePropertyExifLensMake, "lens_make");
    from(exif, kCGImagePropertyExifLensModel, "lens_model");
    from(exif, kCGImagePropertyExifPixelXDimension, "width");
    from(exif, kCGImagePropertyExifPixelYDimension, "height");
    from(gps, kCGImagePropertyGPSLatitude, "lat");
    from(gps, kCGImagePropertyGPSLatitudeRef, "lat_ref");
    from(gps, kCGImagePropertyGPSLongitude, "lon");
    from(gps, kCGImagePropertyGPSLongitudeRef, "lon_ref");
    from(gps, kCGImagePropertyGPSAltitude, "alt");
    from(gps, kCGImagePropertyGPSAltitudeRef, "alt_ref");
    from(gps, kCGImagePropertyGPSDateStamp, "gps_date");
    from(gps, kCGImagePropertyGPSTimeStamp, "gps_time");
    put("orientation", CFDictionaryGetValue(props, kCGImagePropertyOrientation));
    CFRelease(props);
    CFRelease(src);
    CFRelease(url);
    return 0;
}
