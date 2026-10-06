//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#if defined(__APPLE__)

#include <cstddef>
#include <cstdint>

// The few C functions of FSEvents (CoreServices), CoreFoundation and
// libdispatch that io::watch reads the changes of a tree with on macOS,
// declared here under names of their own and bound to the system's symbols
// by asm labels, rather than through the SDK's headers (the rule for
// platform headers: CoreServices' would bring MacTypes' Point, Rect, Byte
// and the rest into every program that includes sgcl/io; the pattern of
// crypto/detail/apple_security.h, whose CoreFoundation functions these share
// by name and type: a function of C linkage is one function whatever
// namespace declares it). The objects are opaque pointers; the one
// structure that crosses the ABI is FSEventStreamContext, five words. The
// constants are the SDK's (FSEvents.h, macOS 26 SDK). CoreServices is
// linked with the library on Apple's systems (CMake).
namespace sgcl::io::detail::apple {
    using Ref = const void*;                // CFStringRef, CFArrayRef, CFAllocatorRef
    using Index = long;                     // CFIndex
    using Stream = struct __FSEventStream*; // FSEventStreamRef
    using Queue = struct dispatch_queue_s*; // dispatch_queue_t (C++, no Objective-C objects)
    using EventFlags = uint32_t;            // FSEventStreamEventFlags
    using EventId = uint64_t;               // FSEventStreamEventId

    // FSEventStreamContext
    struct StreamContext {
        Index version;
        void* info;
        const void* (*retain)(const void* info);
        void (*release)(const void* info);
        Ref (*copy_description)(const void* info);
    };

    // FSEventStreamCallback, the event paths a char** (no kFSEventStreamCreateFlagUseCFTypes)
    using StreamCallback = void (*)(const __FSEventStream* stream, void* info, size_t count, void* paths, const EventFlags flags[], const EventId ids[]);

    inline constexpr uint32_t Utf8 = 0x08000100;                    // kCFStringEncodingUTF8
    inline constexpr EventId SinceNow = 0xFFFFFFFFFFFFFFFFull;     // kFSEventStreamEventIdSinceNow
    inline constexpr uint32_t CreateNoDefer = 0x02;                 // kFSEventStreamCreateFlagNoDefer
    inline constexpr uint32_t CreateWatchRoot = 0x04;               // kFSEventStreamCreateFlagWatchRoot
    inline constexpr uint32_t CreateFileEvents = 0x10;              // kFSEventStreamCreateFlagFileEvents

    inline constexpr EventFlags MustScanSubDirs = 0x00000001;       // kFSEventStreamEventFlagMustScanSubDirs
    inline constexpr EventFlags UserDropped = 0x00000002;
    inline constexpr EventFlags KernelDropped = 0x00000004;
    inline constexpr EventFlags RootChanged = 0x00000020;
    inline constexpr EventFlags ItemCreated = 0x00000100;
    inline constexpr EventFlags ItemRemoved = 0x00000200;
    inline constexpr EventFlags ItemInodeMetaMod = 0x00000400;
    inline constexpr EventFlags ItemRenamed = 0x00000800;
    inline constexpr EventFlags ItemModified = 0x00001000;
    inline constexpr EventFlags ItemFinderInfoMod = 0x00002000;
    inline constexpr EventFlags ItemChangeOwner = 0x00004000;
    inline constexpr EventFlags ItemXattrMod = 0x00008000;
    inline constexpr EventFlags ItemIsFile = 0x00010000;
    inline constexpr EventFlags ItemIsDir = 0x00020000;
    inline constexpr EventFlags ItemIsSymlink = 0x00040000;

    extern "C" {
        // CoreFoundation
        void release(Ref object) __asm__("_CFRelease");
        Ref string_create(Ref allocator, const char* text, uint32_t encoding) __asm__("_CFStringCreateWithCString");
        Ref array_create(Ref allocator, const void** values, Index count, const void* callbacks) __asm__("_CFArrayCreate");
        extern const char type_array_callbacks[] __asm__("_kCFTypeArrayCallBacks");

        // FSEvents
        Stream stream_create(Ref allocator, StreamCallback callback, StreamContext* context, Ref paths, EventId since, double latency, uint32_t flags) __asm__("_FSEventStreamCreate");
        void stream_set_dispatch_queue(Stream stream, Queue queue) __asm__("_FSEventStreamSetDispatchQueue");
        bool stream_start(Stream stream) __asm__("_FSEventStreamStart");
        void stream_stop(Stream stream) __asm__("_FSEventStreamStop");
        void stream_invalidate(Stream stream) __asm__("_FSEventStreamInvalidate");
        void stream_release(Stream stream) __asm__("_FSEventStreamRelease");

        // libdispatch
        Queue queue_create(const char* label, const void* attributes) __asm__("_dispatch_queue_create");
        void sync_f(Queue queue, void* context, void (*work)(void*)) __asm__("_dispatch_sync_f");
        void queue_release(Queue object) __asm__("_dispatch_release");
    }
}

#endif
