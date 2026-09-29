//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The rule for platform headers: sgcl never includes a system framework's
// headers; sgcl/codec/detail/apple_imageio.h declares what codec::heif calls
// under names of its own, bound by asm labels to the system's symbols. This
// file includes the SDK's headers (a test may) and holds the declarations
// against them: the layouts copied (sizes and offsets), every function and
// constant the same symbol, every value of an enumeration the same.
#if defined(__APPLE__)

#include <gtest/gtest.h>

#include <Accelerate/Accelerate.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include "sgcl/codec/detail/apple_imageio.h"

#include <cstddef>
#include <dlfcn.h>

namespace apple = sgcl::codec::detail::apple;

// the structures whose values pass through the ABI
static_assert(sizeof(apple::VImageBuffer) == sizeof(vImage_Buffer));
static_assert(offsetof(apple::VImageBuffer, data) == offsetof(vImage_Buffer, data));
static_assert(offsetof(apple::VImageBuffer, height) == offsetof(vImage_Buffer, height));
static_assert(offsetof(apple::VImageBuffer, width) == offsetof(vImage_Buffer, width));
static_assert(offsetof(apple::VImageBuffer, row_bytes) == offsetof(vImage_Buffer, rowBytes));
static_assert(sizeof(apple::VImageFormat) == sizeof(vImage_CGImageFormat));
static_assert(offsetof(apple::VImageFormat, bits_per_component) == offsetof(vImage_CGImageFormat, bitsPerComponent));
static_assert(offsetof(apple::VImageFormat, bits_per_pixel) == offsetof(vImage_CGImageFormat, bitsPerPixel));
static_assert(offsetof(apple::VImageFormat, color_space) == offsetof(vImage_CGImageFormat, colorSpace));
static_assert(offsetof(apple::VImageFormat, bitmap_info) == offsetof(vImage_CGImageFormat, bitmapInfo));
static_assert(offsetof(apple::VImageFormat, version) == offsetof(vImage_CGImageFormat, version));
static_assert(offsetof(apple::VImageFormat, decode) == offsetof(vImage_CGImageFormat, decode));
static_assert(offsetof(apple::VImageFormat, rendering_intent) == offsetof(vImage_CGImageFormat, renderingIntent));
static_assert(sizeof(apple::Rect) == sizeof(CGRect));
static_assert(offsetof(apple::Rect, x) == offsetof(CGRect, origin.x));
static_assert(offsetof(apple::Rect, y) == offsetof(CGRect, origin.y));
static_assert(offsetof(apple::Rect, width) == offsetof(CGRect, size.width));
static_assert(offsetof(apple::Rect, height) == offsetof(CGRect, size.height));
static_assert(sizeof(apple::ConsumerCallbacks) == sizeof(CGDataConsumerCallbacks));
static_assert(offsetof(apple::ConsumerCallbacks, put_bytes) == offsetof(CGDataConsumerCallbacks, putBytes));
static_assert(offsetof(apple::ConsumerCallbacks, release_consumer) == offsetof(CGDataConsumerCallbacks, releaseConsumer));

// the scalar types
static_assert(sizeof(apple::Index) == sizeof(CFIndex));
static_assert(sizeof(apple::Status) == sizeof(CGImageSourceStatus));
static_assert(sizeof(apple::BitmapInfo) == sizeof(CGBitmapInfo));
static_assert(sizeof(apple::AlphaInfo) == sizeof(CGImageAlphaInfo));
static_assert(sizeof(apple::ColorSpaceModel) == sizeof(CGColorSpaceModel));
static_assert(sizeof(apple::RenderingIntent) == sizeof(CGColorRenderingIntent));
static_assert(sizeof(apple::VImageError) == sizeof(vImage_Error));
static_assert(sizeof(CGFloat) == sizeof(double));

// the values of the enumerations
static_assert(apple::StatusComplete == kCGImageStatusComplete);
static_assert(apple::StatusIncomplete == kCGImageStatusIncomplete);
static_assert(apple::StatusReadingHeader == kCGImageStatusReadingHeader);
static_assert(apple::StatusUnknownType == kCGImageStatusUnknownType);
static_assert(apple::StatusInvalidData == kCGImageStatusInvalidData);
static_assert(apple::StatusUnexpectedEof == kCGImageStatusUnexpectedEOF);
static_assert(apple::AlphaNone == kCGImageAlphaNone);
static_assert(apple::AlphaPremultipliedLast == kCGImageAlphaPremultipliedLast);
static_assert(apple::AlphaPremultipliedFirst == kCGImageAlphaPremultipliedFirst);
static_assert(apple::AlphaLast == kCGImageAlphaLast);
static_assert(apple::AlphaFirst == kCGImageAlphaFirst);
static_assert(apple::AlphaNoneSkipLast == kCGImageAlphaNoneSkipLast);
static_assert(apple::AlphaNoneSkipFirst == kCGImageAlphaNoneSkipFirst);
static_assert(apple::AlphaOnly == kCGImageAlphaOnly);
static_assert(apple::AlphaInfoMask == kCGBitmapAlphaInfoMask);
static_assert(apple::ByteOrder16Little == kCGBitmapByteOrder16Little);
static_assert(apple::ModelMonochrome == kCGColorSpaceModelMonochrome);
static_assert(apple::ModelRgb == kCGColorSpaceModelRGB);
static_assert(apple::NumberSInt32 == kCFNumberSInt32Type);
static_assert(apple::NumberDouble == kCFNumberDoubleType);
static_assert(apple::Utf8 == kCFStringEncodingUTF8);
static_assert(apple::VImageNoAllocate == kvImageNoAllocate);
static_assert(apple::VImageNoError == kvImageNoError);

// Every declaration the system's symbol: a function at the SDK's address,
// a constant the SDK's value
TEST(CodecHeifPlatform_Tests, TheDeclarationsAreTheSdks) {
    // the addresses compared as the loader resolved the symbol (dlsym): two
    // declarations are two functions to the optimizer, which may fold an ==
    // between them to false, so each goes through a volatile first
    auto resolved = [](auto function) {
        const void* volatile p = reinterpret_cast<const void*>(function);
        return p;
    };
    auto same = [&](auto ours, auto theirs, const char* name) {
        const void* symbol = dlsym(RTLD_DEFAULT, name);
        return symbol != nullptr && resolved(ours) == symbol && resolved(theirs) == symbol;
    };
    EXPECT_TRUE(same(&apple::release, &CFRelease, "CFRelease")) << "CFRelease";
    EXPECT_TRUE(same(&apple::data_create, &CFDataCreate, "CFDataCreate")) << "CFDataCreate";
    EXPECT_TRUE(same(&apple::data_create_no_copy, &CFDataCreateWithBytesNoCopy, "CFDataCreateWithBytesNoCopy")) << "CFDataCreateWithBytesNoCopy";
    EXPECT_TRUE(same(&apple::data_create_mutable, &CFDataCreateMutable, "CFDataCreateMutable")) << "CFDataCreateMutable";
    EXPECT_TRUE(same(&apple::data_bytes, &CFDataGetBytePtr, "CFDataGetBytePtr")) << "CFDataGetBytePtr";
    EXPECT_TRUE(same(&apple::data_length, &CFDataGetLength, "CFDataGetLength")) << "CFDataGetLength";
    EXPECT_TRUE(same(&apple::dictionary_create, &CFDictionaryCreate, "CFDictionaryCreate")) << "CFDictionaryCreate";
    EXPECT_TRUE(same(&apple::dictionary_value, &CFDictionaryGetValue, "CFDictionaryGetValue")) << "CFDictionaryGetValue";
    EXPECT_TRUE(same(&apple::number_create, &CFNumberCreate, "CFNumberCreate")) << "CFNumberCreate";
    EXPECT_TRUE(same(&apple::number_value, &CFNumberGetValue, "CFNumberGetValue")) << "CFNumberGetValue";
    EXPECT_TRUE(same(&apple::string_create, &CFStringCreateWithCString, "CFStringCreateWithCString")) << "CFStringCreateWithCString";
    EXPECT_TRUE(same(&apple::equal, &CFEqual, "CFEqual")) << "CFEqual";
    EXPECT_TRUE(same(&apple::source_create_with_data, &CGImageSourceCreateWithData, "CGImageSourceCreateWithData")) << "CGImageSourceCreateWithData";
    EXPECT_TRUE(same(&apple::source_type, &CGImageSourceGetType, "CGImageSourceGetType")) << "CGImageSourceGetType";
    EXPECT_TRUE(same(&apple::source_count, &CGImageSourceGetCount, "CGImageSourceGetCount")) << "CGImageSourceGetCount";
    EXPECT_TRUE(same(&apple::source_status, &CGImageSourceGetStatus, "CGImageSourceGetStatus")) << "CGImageSourceGetStatus";
    EXPECT_TRUE(same(&apple::source_status_at, &CGImageSourceGetStatusAtIndex, "CGImageSourceGetStatusAtIndex")) << "CGImageSourceGetStatusAtIndex";
    EXPECT_TRUE(same(&apple::source_copy_properties_at, &CGImageSourceCopyPropertiesAtIndex, "CGImageSourceCopyPropertiesAtIndex")) << "CGImageSourceCopyPropertiesAtIndex";
    EXPECT_TRUE(same(&apple::source_create_image_at, &CGImageSourceCreateImageAtIndex, "CGImageSourceCreateImageAtIndex")) << "CGImageSourceCreateImageAtIndex";
    EXPECT_TRUE(same(&apple::destination_create_with_data, &CGImageDestinationCreateWithData, "CGImageDestinationCreateWithData")) << "CGImageDestinationCreateWithData";
    EXPECT_TRUE(same(&apple::destination_create_with_consumer, &CGImageDestinationCreateWithDataConsumer, "CGImageDestinationCreateWithDataConsumer")) << "CGImageDestinationCreateWithDataConsumer";
    EXPECT_TRUE(same(&apple::destination_add_image, &CGImageDestinationAddImage, "CGImageDestinationAddImage")) << "CGImageDestinationAddImage";
    EXPECT_TRUE(same(&apple::destination_finalize, &CGImageDestinationFinalize, "CGImageDestinationFinalize")) << "CGImageDestinationFinalize";
    EXPECT_TRUE(same(&apple::image_width, &CGImageGetWidth, "CGImageGetWidth")) << "CGImageGetWidth";
    EXPECT_TRUE(same(&apple::image_height, &CGImageGetHeight, "CGImageGetHeight")) << "CGImageGetHeight";
    EXPECT_TRUE(same(&apple::image_bits_per_component, &CGImageGetBitsPerComponent, "CGImageGetBitsPerComponent")) << "CGImageGetBitsPerComponent";
    EXPECT_TRUE(same(&apple::image_alpha_info, &CGImageGetAlphaInfo, "CGImageGetAlphaInfo")) << "CGImageGetAlphaInfo";
    EXPECT_TRUE(same(&apple::image_color_space, &CGImageGetColorSpace, "CGImageGetColorSpace")) << "CGImageGetColorSpace";
    EXPECT_TRUE(same(&apple::image_create, &CGImageCreate, "CGImageCreate")) << "CGImageCreate";
    EXPECT_TRUE(same(&apple::color_space_model, &CGColorSpaceGetModel, "CGColorSpaceGetModel")) << "CGColorSpaceGetModel";
    EXPECT_TRUE(same(&apple::color_space_copy_icc, &CGColorSpaceCopyICCData, "CGColorSpaceCopyICCData")) << "CGColorSpaceCopyICCData";
    EXPECT_TRUE(same(&apple::color_space_create_with_name, &CGColorSpaceCreateWithName, "CGColorSpaceCreateWithName")) << "CGColorSpaceCreateWithName";
    EXPECT_TRUE(same(&apple::color_space_create_with_icc, &CGColorSpaceCreateWithICCData, "CGColorSpaceCreateWithICCData")) << "CGColorSpaceCreateWithICCData";
    EXPECT_TRUE(same(&apple::provider_create_with_data, &CGDataProviderCreateWithData, "CGDataProviderCreateWithData")) << "CGDataProviderCreateWithData";
    EXPECT_TRUE(same(&apple::consumer_create, &CGDataConsumerCreate, "CGDataConsumerCreate")) << "CGDataConsumerCreate";
    EXPECT_TRUE(same(&apple::bitmap_context_create, &CGBitmapContextCreate, "CGBitmapContextCreate")) << "CGBitmapContextCreate";
    EXPECT_TRUE(same(&apple::context_draw_image, &CGContextDrawImage, "CGContextDrawImage")) << "CGContextDrawImage";
    EXPECT_TRUE(same(&apple::vimage_init_with_image, &vImageBuffer_InitWithCGImage, "vImageBuffer_InitWithCGImage")) << "vImageBuffer_InitWithCGImage";

    // the constants: the same objects (the pointers the SDK's variables hold)
    EXPECT_EQ(apple::allocator_null, reinterpret_cast<const void*>(kCFAllocatorNull));
    EXPECT_EQ(apple::boolean_false, reinterpret_cast<const void*>(kCFBooleanFalse));
    EXPECT_EQ(static_cast<const void*>(apple::dictionary_key_callbacks), static_cast<const void*>(&kCFTypeDictionaryKeyCallBacks));
    EXPECT_EQ(static_cast<const void*>(apple::dictionary_value_callbacks), static_cast<const void*>(&kCFTypeDictionaryValueCallBacks));
    EXPECT_EQ(apple::source_should_cache, reinterpret_cast<const void*>(kCGImageSourceShouldCache));
    EXPECT_EQ(apple::property_pixel_width, reinterpret_cast<const void*>(kCGImagePropertyPixelWidth));
    EXPECT_EQ(apple::property_pixel_height, reinterpret_cast<const void*>(kCGImagePropertyPixelHeight));
    EXPECT_EQ(apple::property_orientation, reinterpret_cast<const void*>(kCGImagePropertyOrientation));
    EXPECT_EQ(apple::destination_lossy_quality, reinterpret_cast<const void*>(kCGImageDestinationLossyCompressionQuality));
    EXPECT_EQ(apple::color_space_srgb, reinterpret_cast<const void*>(kCGColorSpaceSRGB));
    EXPECT_EQ(apple::color_space_gray, reinterpret_cast<const void*>(kCGColorSpaceGenericGrayGamma2_2));
}

#endif
