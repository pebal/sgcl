//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "../make_tracked.h"
#include "bytes.h"
#include "../tracked_ptr.h"
#include "../unique_ptr.h"
#include "maker.h"
#include "type_info.h"

#include <atomic>
#include <cassert>
#include <cstring>
#include <string_view>

namespace sgcl::detail {
    // The bytes behind a string (string.h): a header of eight bytes, the
    // length and the hash, then the characters and a terminator, in one
    // managed object of exactly that size, rounded to four, pointed at by
    // the string's one word. The hash is 0 until something asks for it
    // (as Java's String keeps its hashCode): the object is immutable, so
    // the first thread to compute it stores it, relaxed, and any other
    // computes the same value.
    struct StringHeader {
        uint32_t size;
        std::atomic<uint32_t> hash;
    };

    static_assert(sizeof(StringHeader) == 8);

    // The header, the characters and the terminator written into the bytes
    template<class CharT>
    SGCL_INLINE_HOT void string_fill(unsigned char* bytes, std::basic_string_view<CharT> s) noexcept {
        ::new(bytes) StringHeader{(uint32_t)s.size(), {0}};
        copy_bytes(bytes + sizeof(StringHeader), s.data(), s.size() * sizeof(CharT));
        std::memset(bytes + sizeof(StringHeader) + s.size() * sizeof(CharT), 0, sizeof(CharT));
    }

#if defined(SGCL_ASAN)
    // Under the address sanitizer (os.h: SGCL_ASAN): the bytes of the
    // string's object past its first `bytes` (the header, the characters and
    // the terminator) poisoned, the slack of its size class or of the bound
    // it was made for (make_unfilled, finish): a read past the terminator is
    // reported. The object's size from its page: a class's object
    // (Metadata::user_size) or a buffer's capacity (one byte an element).
    inline void string_poison_past(const void* p, size_t bytes) noexcept {
        auto& m = Page::metadata_of(p);
        size_t size = m.is_array ? (static_cast<const ArrayBase*>(p) - 1)->capacity : m.user_size;
        if (bytes < size) {
            SGCL_ASAN_POISON(static_cast<const char*>(p) + bytes, size - bytes);
        }
    }
#endif

    // The object of a size class: the bytes, written by StringMaker
    // (make_slot, make_unfilled_slot), never constructed; the type names
    // the class's pool (its typeid, its size, its traits: string_class_constants
    // below). Never traced (the pointer map is empty, by the
    // specialization below): the bytes are characters and nothing else.
    template<size_t Bytes>
    struct StringSlot {
        alignas(4) unsigned char bytes[Bytes];
    };

    template<size_t Bytes>
    struct MayContainTracked<StringSlot<Bytes>> {
        static constexpr auto value = false;
    };

    template<size_t Bytes>
    struct IsStringStorage<StringSlot<Bytes>> : std::true_type {};

    // The element of the buffer a long string lives in: a byte of its own
    // type, so that the buffer's metadata says "a string" (IsStringStorage)
    // where a buffer of unsigned char would not
    struct StringByte {
        unsigned char value;
    };

    template<>
    struct IsStringStorage<StringByte> : std::true_type {};

    // The size classes: every four bytes up to 256, then by half again
    // up to a page; a longer string goes to a managed buffer of bytes
    // (maker.h: a range of pages).
    constexpr size_t StringSmallClasses = 256 / 4;

    constexpr size_t string_large_class(size_t i) noexcept {
        size_t c = 256;
        for (size_t k = 0; k <= i; ++k) {
            c = (c * 3 / 2 + 7) & ~size_t(7);
        }
        return c;
    }

    constexpr size_t StringLargeClasses = [] {
        size_t i = 0;
        while (string_large_class(i) + 64 <= PageDataSize) {
            ++i;
        }
        return i;
    }();

    // The size classes as data: every class its own pool (its own
    // Metadata, pages and slab, as a type of its own), described by a row
    // of constants, its objects made by one function for all classes (a
    // table of functions per class was ~3.6 KB of code each, all of them
    // in any program that makes a string). Classes 0..63 are the small
    // ones ((c + 1) * 4 bytes), the rest string_large_class(c - 64). The
    // type of a class's objects stays StringSlot<Bytes> for the
    // statistics and the type's traits (IsStringStorage: is_string).
    constexpr size_t StringClassCount = StringSmallClasses + StringLargeClasses;

    SGCL_INLINE_HOT constexpr size_t string_class_bytes(size_t c) noexcept {
        return c < StringSmallClasses ? (c + 1) * 4 : string_large_class(c - StringSmallClasses);
    }

    template<size_t Bytes>
    SGCL_INLINE_HOT constexpr TypeConstants string_class_constants_of() noexcept {
        using Info = TypeInfo<StringSlot<Bytes>>;
        static_assert(Info::Allocator::IsPoolAllocator::value && !Info::MayContainTracked);
        return Metadata::constants_of<StringSlot<Bytes>>();
    }

    template<size_t... Is>
    SGCL_INLINE_HOT constexpr std::array<TypeConstants, sizeof...(Is)> string_class_constants_of(std::index_sequence<Is...>) noexcept {
        return {string_class_constants_of<string_class_bytes(Is)>()...};
    }

    inline constexpr auto string_class_constants = string_class_constants_of(std::make_index_sequence<StringClassCount>());

    // The pools of the classes: per class its number among the allocators
    // of every thread (thread.h: pool_allocator; 0 until its first use) and
    // its Metadata, made on the first use of the class on any thread. Both
    // constinit: no guard and no order of initialization.
    // Each array on lines of its own, as the buffer classes' (maker.h:
    // BufferPools): read on every string made, and placed by the linker
    // next to whatever static comes before or after them, such as the
    // collector's counters it writes every cycle
    struct StringPools {
        alignas(config::cache_line_size) inline static constinit std::atomic<unsigned> slots[StringClassCount] = {};
        alignas(config::cache_line_size) inline static constinit std::atomic<Metadata*> metadata[StringClassCount] = {};

        SGCL_INLINE_HOT static Metadata& metadata_of(unsigned c) noexcept {
            return Metadata::of_pool(metadata[c], string_class_constants[c]);
        }

        // A slot of class c, in state UniqueLock (as make_tracked's), its
        // bytes as the slot's last user left them
        SGCL_ALWAYS_INLINE static void* alloc(unsigned c) noexcept {
            auto& a = current_thread().pool_allocator(slots[c], [c]() -> Metadata& { return metadata_of(c); });
            return MakerBase::allocate(a, 0, [](void*) noexcept {});   // never null: out of memory handled in the allocators
        }
    };

    // The managed object for a string of `bytes` (header, characters and
    // terminator), from the smallest class that holds it, as the string's
    // word: a pointer to the first byte
    struct StringMaker {
        using Word = tracked_ptr<const void>;

        // What the size classes hand back is a unique_ptr and not the
        // string's word. The object is kept alive by the state of its slot
        // (UniqueLock, set by the allocator before it was handed out), not
        // by any pointer to it, so carrying it out as a raw word is as safe
        // as make_tracked's own return — and it is free, where a tracked_ptr
        // returned across a call the compiler cannot inline costs three
        // touches: the released transition in the callee, a barrier store
        // when the caller takes the result, and the temporary's destructor.
        // Built here instead, in make, the transition happens once and
        // writes straight into the string's word.
        using Slot = unique_ptr<void>;

        // An object of class c holding `s`; one function for all classes
        template<class CharT>
        SGCL_NOINLINE static Slot make_slot(unsigned c, std::basic_string_view<CharT> s) noexcept {
            auto p = StringPools::alloc(c);
            string_fill((unsigned char*)p, s);
#if defined(SGCL_ASAN)
            string_poison_past(p, sizeof(StringHeader) + (s.size() + 1) * sizeof(CharT));
#endif
            return Slot(UniquePtr<void>(p));
        }

        // An object of class c with its bytes left unwritten, for a string
        // written in place (make_bounded)
        SGCL_NOINLINE static Slot make_unfilled_slot(unsigned c) noexcept {
            return Slot(UniquePtr<void>(StringPools::alloc(c)));
        }

        template<class CharT>
        SGCL_INLINE_HOT static Slot make_buffer(std::basic_string_view<CharT> s, size_t bytes) noexcept {
            unique_ptr<StringByte> buffer(Maker<StringByte[]>::make_tracked_data(bytes));
            string_fill(reinterpret_cast<unsigned char*>(buffer.get()), s);
            return Slot(std::move(buffer));
        }

        template<class CharT>
        static Word make(std::basic_string_view<CharT> s) {
            if (s.size() > UINT32_MAX) {   // the header holds 32 bits of it: past that the length would be cut short
                throw length_error("sgcl::basic_string");
            }
            const size_t bytes = sizeof(StringHeader) + (s.size() + 1) * sizeof(CharT);
            if (bytes <= 256) {
                return Word(make_slot<CharT>((unsigned)((bytes - 1) / 4), s));
            }
            for (size_t i = 0; i < StringLargeClasses; ++i) {
                if (bytes <= string_large_class(i)) {
                    return Word(make_slot<CharT>((unsigned)(StringSmallClasses + i), s));
                }
            }
            return Word(make_buffer(s, bytes));
        }

        // A string written in place: the object for `bound` characters
        // taken from the class that holds them, `fill(CharT* chars)`
        // writes the characters straight into it and returns how many it
        // wrote (at most `bound`), and the header and the terminator are
        // set after it. Each character is written once, and nothing is
        // allocated but the object: what a builder that knows its size,
        // or a bound of it, uses in place of a std::string it would copy.
        // Fewer characters than the bound, but at least half of it, are the
        // length in the header only: the object keeps the size class of the
        // bound (the bytes past the terminator are the class's slack, as
        // the rounding to four is), never a part of it given back. Fewer
        // than half are copied once more into an object of their exact
        // class, and the bound's object goes back whole: a string does not
        // keep more than its own size again as slack for its life (the
        // bound of three bytes a character of txt::decode). None is the
        // empty string, null, and the object goes back at once. A fill
        // that throws lets the object go with it
        template<class CharT, class Fill>
        static Word make_bounded(size_t bound, Fill&& fill) {
            if (bound > UINT32_MAX) {
                throw length_error("sgcl::basic_string");
            }
            if (bound == 0) {
                return Word();
            }
            CharT* chars = nullptr;
            Slot slot = make_unfilled<CharT>(bound, chars);
            const size_t used = fill(chars);
            return finish<CharT>(std::move(slot), bound, used);
        }

        // The same in two steps, for a builder that cannot hand its writing
        // over as a function (a read that waits, in a task): the object for
        // `bound` (> 0) characters with their address in `chars`, unique
        // until finish makes it the string of the first `used` of them (the
        // same rules as make_bounded: none is the empty string, less than
        // half the bound a copy of the exact class)
        template<class CharT>
        SGCL_INLINE_HOT static Slot make_unfilled(size_t bound, CharT*& chars) {
            if (bound == 0 || bound > UINT32_MAX) {
                throw length_error("sgcl::basic_string");
            }
            const size_t bytes = sizeof(StringHeader) + (bound + 1) * sizeof(CharT);
            Slot slot = bytes <= 256 ? make_unfilled_slot((unsigned)((bytes - 1) / 4)) : _unfilled_large(bytes);
#if defined(SGCL_ASAN)
            string_poison_past(slot.get(), bytes);   // the bound's bytes stay writable, the class's slack past them not
#endif
            chars = reinterpret_cast<CharT*>(static_cast<unsigned char*>(slot.get()) + sizeof(StringHeader));
            return slot;
        }

        template<class CharT>
        static Word finish(Slot slot, size_t bound, size_t used) noexcept {
            auto* p = static_cast<unsigned char*>(slot.get());
            auto* chars = reinterpret_cast<CharT*>(p + sizeof(StringHeader));
            assert(used <= bound && "a fill wrote past the bound it was given");
            if (used == 0) {
                return Word();
            }
            if (used < bound - used) {
                // More than half the room left over (a bound of three bytes
                // a character that took one): a copy in an object of the
                // exact class, this one given back whole as it goes (never
                // a part of it), rather than the slack kept for the
                // string's life (make's length check cannot fail: used is
                // under the bound, which make_unfilled checked)
                return make(std::basic_string_view<CharT>(chars, used));
            }
            ::new(p) StringHeader{(uint32_t)used, {0}};
            chars[used] = CharT();
#if defined(SGCL_ASAN)
            string_poison_past(p, sizeof(StringHeader) + (used + 1) * sizeof(CharT));   // the bound's characters left over
#endif
            return Word(std::move(slot));
        }

        // The same for exactly `n` characters: `fill(CharT* chars)` writes
        // all of them
        template<class CharT, class Fill>
        SGCL_INLINE_HOT static Word make_filled(size_t n, Fill&& fill) {
            return make_bounded<CharT>(n, [&](CharT* chars) {
                fill(chars);
                return n;
            });
        }

    private:
        static Slot _unfilled_large(size_t bytes) noexcept {
            for (size_t i = 0; i < StringLargeClasses; ++i) {
                if (bytes <= string_large_class(i)) {
                    return make_unfilled_slot((unsigned)(StringSmallClasses + i));
                }
            }
            return Slot(unique_ptr<StringByte>(Maker<StringByte[]>::make_tracked_data(bytes)));
        }
    };
}
