//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(__APPLE__)

#include <cstddef>
#include <cstdint>

// The few C functions of ImageIO, CoreGraphics, CoreFoundation and vImage
// (Accelerate) that HEIF needs, declared here under names of their own and
// bound to the system's symbols by asm labels, rather than through the
// SDK's headers: those would bring MacTypes' Point, Rect, Size, Byte and
// the rest into the global namespace of every program that includes
// sgcl/codec, and Accelerate's whole header set into its compilation. The
// names here never meet the SDK's, so a program that includes the SDK too
// sees no conflict. The objects are opaque pointers; the two structures
// vImage takes are copies of its layout (tests/codec/heif.cpp holds them,
// the constants and the declarations against the SDK's headers). The
// frameworks are linked with the library on Apple's systems (CMake).
namespace sgcl::codec::detail::apple {
    using Ref = const void*;                // any CoreFoundation object: CFDataRef, CGImageRef…
    using Index = long;                     // CFIndex
    using Status = int32_t;                 // CGImageSourceStatus
    using BitmapInfo = uint32_t;            // CGBitmapInfo
    using AlphaInfo = uint32_t;             // CGImageAlphaInfo
    using ColorSpaceModel = int32_t;        // CGColorSpaceModel
    using RenderingIntent = int32_t;        // CGColorRenderingIntent
    using VImageError = long;               // vImage_Error

    struct VImageBuffer {                   // vImage_Buffer
        void* data;
        unsigned long height;
        unsigned long width;
        size_t row_bytes;
    };

    struct VImageFormat {                   // vImage_CGImageFormat
        uint32_t bits_per_component;
        uint32_t bits_per_pixel;
        Ref color_space;
        BitmapInfo bitmap_info;
        uint32_t version;
        const double* decode;
        RenderingIntent rendering_intent;
    };

    struct Rect {                           // CGRect: origin, size (CGFloat is double)
        double x, y, width, height;
    };

    struct ConsumerCallbacks {              // CGDataConsumerCallbacks
        size_t (*put_bytes)(void* info, const void* buffer, size_t count);
        void (*release_consumer)(void* info);
    };

    // CGImageSourceStatus
    inline constexpr Status StatusComplete = 0;
    inline constexpr Status StatusIncomplete = -1;
    inline constexpr Status StatusReadingHeader = -2;
    inline constexpr Status StatusUnknownType = -3;
    inline constexpr Status StatusInvalidData = -4;
    inline constexpr Status StatusUnexpectedEof = -5;

    // CGImageAlphaInfo and CGBitmapInfo
    inline constexpr AlphaInfo AlphaNone = 0;
    inline constexpr AlphaInfo AlphaPremultipliedLast = 1;
    inline constexpr AlphaInfo AlphaPremultipliedFirst = 2;
    inline constexpr AlphaInfo AlphaLast = 3;
    inline constexpr AlphaInfo AlphaFirst = 4;
    inline constexpr AlphaInfo AlphaNoneSkipLast = 5;
    inline constexpr AlphaInfo AlphaNoneSkipFirst = 6;
    inline constexpr AlphaInfo AlphaOnly = 7;
    inline constexpr BitmapInfo AlphaInfoMask = 0x1F;
    inline constexpr BitmapInfo ByteOrder16Little = 1u << 12;

    // CGColorSpaceModel
    inline constexpr ColorSpaceModel ModelMonochrome = 0;
    inline constexpr ColorSpaceModel ModelRgb = 1;

    // CFNumberType, CFStringEncoding
    inline constexpr Index NumberSInt32 = 3;
    inline constexpr Index NumberDouble = 13;
    inline constexpr uint32_t Utf8 = 0x08000100;

    // vImage_Flags, vImage_Error
    inline constexpr uint32_t VImageNoAllocate = 512;
    inline constexpr VImageError VImageNoError = 0;

    extern "C" {
        // CoreFoundation
        void release(Ref object) __asm__("_CFRelease");
        Ref data_create(Ref allocator, const uint8_t* bytes, Index length) __asm__("_CFDataCreate");
        Ref data_create_no_copy(Ref allocator, const uint8_t* bytes, Index length, Ref deallocator) __asm__("_CFDataCreateWithBytesNoCopy");
        Ref data_create_mutable(Ref allocator, Index capacity) __asm__("_CFDataCreateMutable");
        const uint8_t* data_bytes(Ref data) __asm__("_CFDataGetBytePtr");
        Index data_length(Ref data) __asm__("_CFDataGetLength");
        Ref dictionary_create(Ref allocator, const Ref* keys, const Ref* values, Index count, const void* key_callbacks, const void* value_callbacks) __asm__("_CFDictionaryCreate");
        Ref dictionary_value(Ref dictionary, Ref key) __asm__("_CFDictionaryGetValue");
        Ref number_create(Ref allocator, Index type, const void* value) __asm__("_CFNumberCreate");
        bool number_value(Ref number, Index type, void* value) __asm__("_CFNumberGetValue");
        Ref string_create(Ref allocator, const char* text, uint32_t encoding) __asm__("_CFStringCreateWithCString");
        bool equal(Ref a, Ref b) __asm__("_CFEqual");
        extern const Ref allocator_null __asm__("_kCFAllocatorNull");
        extern const Ref boolean_false __asm__("_kCFBooleanFalse");
        extern const unsigned char dictionary_key_callbacks[] __asm__("_kCFTypeDictionaryKeyCallBacks");
        extern const unsigned char dictionary_value_callbacks[] __asm__("_kCFTypeDictionaryValueCallBacks");

        // ImageIO: reading
        Ref source_create_with_data(Ref data, Ref options) __asm__("_CGImageSourceCreateWithData");
        Ref source_type(Ref source) __asm__("_CGImageSourceGetType");
        size_t source_count(Ref source) __asm__("_CGImageSourceGetCount");
        Status source_status(Ref source) __asm__("_CGImageSourceGetStatus");
        Status source_status_at(Ref source, size_t index) __asm__("_CGImageSourceGetStatusAtIndex");
        Ref source_copy_properties_at(Ref source, size_t index, Ref options) __asm__("_CGImageSourceCopyPropertiesAtIndex");
        Ref source_create_image_at(Ref source, size_t index, Ref options) __asm__("_CGImageSourceCreateImageAtIndex");
        extern const Ref source_should_cache __asm__("_kCGImageSourceShouldCache");
        extern const Ref property_pixel_width __asm__("_kCGImagePropertyPixelWidth");
        extern const Ref property_pixel_height __asm__("_kCGImagePropertyPixelHeight");
        extern const Ref property_orientation __asm__("_kCGImagePropertyOrientation");

        // ImageIO: writing
        Ref destination_create_with_data(Ref data, Ref type, size_t count, Ref options) __asm__("_CGImageDestinationCreateWithData");
        Ref destination_create_with_consumer(Ref consumer, Ref type, size_t count, Ref options) __asm__("_CGImageDestinationCreateWithDataConsumer");
        void destination_add_image(Ref destination, Ref image, Ref properties) __asm__("_CGImageDestinationAddImage");
        bool destination_finalize(Ref destination) __asm__("_CGImageDestinationFinalize");
        extern const Ref destination_lossy_quality __asm__("_kCGImageDestinationLossyCompressionQuality");

        // CoreGraphics
        size_t image_width(Ref image) __asm__("_CGImageGetWidth");
        size_t image_height(Ref image) __asm__("_CGImageGetHeight");
        size_t image_bits_per_component(Ref image) __asm__("_CGImageGetBitsPerComponent");
        AlphaInfo image_alpha_info(Ref image) __asm__("_CGImageGetAlphaInfo");
        Ref image_color_space(Ref image) __asm__("_CGImageGetColorSpace");
        Ref image_create(size_t width, size_t height, size_t bits_per_component, size_t bits_per_pixel, size_t bytes_per_row, Ref color_space,
                         BitmapInfo info, Ref provider, const double* decode, bool interpolate, RenderingIntent intent) __asm__("_CGImageCreate");
        ColorSpaceModel color_space_model(Ref space) __asm__("_CGColorSpaceGetModel");
        Ref color_space_copy_icc(Ref space) __asm__("_CGColorSpaceCopyICCData");
        Ref color_space_create_with_name(Ref name) __asm__("_CGColorSpaceCreateWithName");
        Ref color_space_create_with_icc(Ref data) __asm__("_CGColorSpaceCreateWithICCData");
        extern const Ref color_space_srgb __asm__("_kCGColorSpaceSRGB");
        extern const Ref color_space_gray __asm__("_kCGColorSpaceGenericGrayGamma2_2");
        Ref provider_create_with_data(void* info, const void* data, size_t size, void (*release)(void* info, const void* data, size_t size)) __asm__("_CGDataProviderCreateWithData");
        Ref consumer_create(void* info, const ConsumerCallbacks* callbacks) __asm__("_CGDataConsumerCreate");
        Ref bitmap_context_create(void* data, size_t width, size_t height, size_t bits_per_component, size_t bytes_per_row, Ref color_space,
                                  BitmapInfo info) __asm__("_CGBitmapContextCreate");
        void context_draw_image(Ref context, Rect rect, Ref image) __asm__("_CGContextDrawImage");

        // vImage
        VImageError vimage_init_with_image(VImageBuffer* buffer, VImageFormat* format, const double* background, Ref image, uint32_t flags) __asm__("_vImageBuffer_InitWithCGImage");
    }

    // An owned CoreFoundation object, released at the end of its scope
    class Owned {
    public:
        Owned() noexcept = default;

        explicit Owned(Ref r) noexcept
        : _r(r) {
        }

        Owned(const Owned&) = delete;
        Owned& operator=(const Owned&) = delete;

        Owned(Owned&& o) noexcept
        : _r(o._r) {
            o._r = nullptr;
        }

        Owned& operator=(Owned&& o) noexcept {
            if (this != &o) {
                if (_r) {
                    release(_r);
                }
                _r = o._r;
                o._r = nullptr;
            }
            return *this;
        }

        ~Owned() {
            if (_r) {
                release(_r);
            }
        }

        Ref get() const noexcept {
            return _r;
        }

        explicit operator bool() const noexcept {
            return _r != nullptr;
        }

    private:
        Ref _r = nullptr;
    };

    // A dictionary of CoreFoundation keys and values (retained by it)
    inline Ref dictionary(const Ref* keys, const Ref* values, Index n) noexcept {
        return dictionary_create(nullptr, keys, values, n, dictionary_key_callbacks, dictionary_value_callbacks);
    }

    // An int of a dictionary's CFNumber, fallback when there is none
    inline long long number_of(Ref dict, Ref key, long long fallback) noexcept {
        if (!dict) {
            return fallback;
        }
        Ref n = dictionary_value(dict, key);
        int32_t v = 0;
        if (!n || !number_value(n, NumberSInt32, &v)) {
            return fallback;
        }
        return v;
    }
}

#endif
