//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "maker.h"
#include "type_info.h"
#include "../make_tracked.h"
#include "../unique_ptr.h"

#include <atomic>
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
    inline void string_fill(unsigned char* bytes, std::basic_string_view<CharT> s) noexcept {
        ::new(bytes) StringHeader{(uint32_t)s.size(), {0}};
        std::memcpy(bytes + sizeof(StringHeader), s.data(), s.size() * sizeof(CharT));
        std::memset(bytes + sizeof(StringHeader) + s.size() * sizeof(CharT), 0, sizeof(CharT));
    }

    // The object of a size class: the bytes, filled by the constructor,
    // no zeroing. Never traced (the pointer map is empty, by the
    // specialization below): the bytes are characters and nothing else.
    template<size_t Bytes>
    struct StringSlot {
        alignas(4) unsigned char bytes[Bytes];

        template<class CharT>
        StringSlot(std::basic_string_view<CharT> s) noexcept {
            string_fill(bytes, s);
        }
    };

    template<size_t Bytes>
    struct MayContainTracked<StringSlot<Bytes>> {
        static constexpr auto value = false;
    };

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

    // The managed object for a string of `bytes` (header, characters and
    // terminator), from the smallest class that holds it, as the string's
    // word: a pointer to the first byte
    template<template<class> class Ptr>
    struct StringMaker {
        using Word = Ptr<const void>;

        template<class CharT, size_t Bytes>
        static Word make_slot(std::basic_string_view<CharT> s) {
            return Word(make_tracked<StringSlot<Bytes>>(s));
        }

        template<class CharT>
        static Word make_buffer(std::basic_string_view<CharT> s, size_t bytes) {
            unique_ptr<unsigned char> buffer(Maker<unsigned char[]>::make_tracked_data(bytes));
            string_fill(buffer.get(), s);
            return Word(std::move(buffer));
        }

        template<class CharT>
        static Word make(std::basic_string_view<CharT> s) {
            const size_t bytes = sizeof(StringHeader) + (s.size() + 1) * sizeof(CharT);
            if (bytes <= 256) {
                return small_table<CharT>[(bytes - 1) / 4](s);
            }
            for (size_t i = 0; i < StringLargeClasses; ++i) {
                if (bytes <= string_large_class(i)) {
                    return large_table<CharT>[i](s);
                }
            }
            return make_buffer(s, bytes);
        }

    private:
        template<class CharT>
        using MakeFn = Word (*)(std::basic_string_view<CharT>);

        template<class CharT, size_t... Is>
        static constexpr std::array<MakeFn<CharT>, sizeof...(Is)> small_entries(std::index_sequence<Is...>) {
            return {&make_slot<CharT, (Is + 1) * 4>...};
        }

        template<class CharT, size_t... Is>
        static constexpr std::array<MakeFn<CharT>, sizeof...(Is)> large_entries(std::index_sequence<Is...>) {
            return {&make_slot<CharT, string_large_class(Is)>...};
        }

        template<class CharT>
        static constexpr auto small_table = small_entries<CharT>(std::make_index_sequence<StringSmallClasses>());

        template<class CharT>
        static constexpr auto large_table = large_entries<CharT>(std::make_index_sequence<StringLargeClasses>());
    };
}
