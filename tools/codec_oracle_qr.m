/*------------------------------------------------------------------------------
 * SGCL: a C++20 application platform
 * Copyright (c) 2022-2026 Sebastian Nibisz
 * SPDX-License-Identifier: Apache-2.0
 *------------------------------------------------------------------------------
 * The QR oracle of the codec tests (macOS): CoreImage's generator and
 * detector, the system's own QR code.
 *
 *   codec_oracle_qr make <text> <L|M|Q|H>   CIQRCodeGenerator's symbol of the
 *                                           text's UTF-8: its side, then a
 *                                           line of 0 and 1 (dark) a row, the
 *                                           one-module border left out
 *   codec_oracle_qr read <image>            CIDetector's reading: the text,
 *                                           then "version V level L mask M"
 *
 * Built by the tests (tests/codec/oracle.h):
 *   cc -O2 -fobjc-arc tools/codec_oracle_qr.m -framework Foundation -framework CoreImage -framework CoreGraphics
 * Nothing read: "NONE" on stderr, exit 1.
 */
#import <CoreImage/CoreImage.h>
#import <Foundation/Foundation.h>

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc == 4 && strcmp(argv[1], "make") == 0) {
            NSData* message = [[NSString stringWithUTF8String:argv[2]] dataUsingEncoding:NSUTF8StringEncoding];
            CIFilter* f = [CIFilter filterWithName:@"CIQRCodeGenerator"];
            [f setValue:message forKey:@"inputMessage"];
            [f setValue:[NSString stringWithUTF8String:argv[3]] forKey:@"inputCorrectionLevel"];
            CIImage* img = f.outputImage;
            CGImageRef cg = [[CIContext context] createCGImage:img fromRect:img.extent];
            if (!cg) {
                fprintf(stderr, "NONE\n");
                return 1;
            }
            const size_t w = CGImageGetWidth(cg), h = CGImageGetHeight(cg);
            CFDataRef d = CGDataProviderCopyData(CGImageGetDataProvider(cg));
            const uint8_t* p = CFDataGetBytePtr(d);
            const size_t row = CGImageGetBytesPerRow(cg), px = CGImageGetBitsPerPixel(cg) / 8;
            printf("%zu\n", w - 2);
            for (size_t y = 1; y + 1 < h; ++y) {
                for (size_t x = 1; x + 1 < w; ++x) {
                    putchar(p[y * row + x * px] < 128 ? '1' : '0');
                }
                putchar('\n');
            }
            CFRelease(d);
            CGImageRelease(cg);
            return 0;
        }
        if (argc == 3 && strcmp(argv[1], "read") == 0) {
            CIImage* img = [CIImage imageWithContentsOfURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]]];
            CIDetector* det = [CIDetector detectorOfType:CIDetectorTypeQRCode context:nil options:@{CIDetectorAccuracy : CIDetectorAccuracyHigh}];
            NSArray* found = img ? [det featuresInImage:img] : nil;
            if (found.count == 0) {
                fprintf(stderr, "NONE\n");
                return 1;
            }
            CIQRCodeFeature* q = found[0];
            CIQRCodeDescriptor* d = q.symbolDescriptor;
            printf("%s\nversion %ld level %c mask %ld\n", q.messageString ? q.messageString.UTF8String : "",
                   (long)d.symbolVersion, (char)d.errorCorrectionLevel, (long)d.maskPattern);
            return 0;
        }
        fprintf(stderr, "usage: codec_oracle_qr make <text> <L|M|Q|H> | read <image>\n");
        return 2;
    }
}
