//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Intern<T>: a pool where equal values share one managed object (Go's
// unique package, Java's String.intern, .NET's String.Intern). Make(value)
// is the canonical object of the value, a Ptr<const T>, or for a String
// the string itself; the pool holds its objects weakly, so an object
// nobody holds any more is collected and made again by the next Make.
// Shared by any number of threads without a lock.
#pragma once

#include "../../concurrent/intern.h"
#include "../Core/String.h"
#include "../Core/Ptr.h"

#include <functional>
#include <string_view>
#include <type_traits>

namespace Sgcl {
    namespace detail {
        // What a pool of T hands out and what the sgcl pool under it is
        // keyed by: a value as it is, and a Ptr<const T> to its object;
        // a String by the sgcl string inside, and the String itself back
        template<class T>
        struct InternValue {
            using Inner = T;
            using Handle = Ptr<const T>;

            static const T& inner(const T& value) noexcept {
                return value;
            }

            static Handle handle(sgcl::tracked_ptr<const T> h) noexcept {
                return Handle(std::move(h));
            }
        };

        template<class C, class Tr>
        struct InternValue<BasicString<C, Tr>> {
            using Inner = sgcl::basic_string<C, Tr>;
            using Handle = BasicString<C, Tr>;

            static const Inner& inner(const BasicString<C, Tr>& s) noexcept {
                return s.Inner();
            }

            static Handle handle(Inner h) noexcept {
                return Handle(std::move(h));
            }
        };
    }

    template<class T, class Hash = std::hash<T>, class Equal = std::equal_to<T>>
    class Intern {
        using Value = detail::InternValue<T>;
        using InnerHash = std::conditional_t<std::is_same_v<Hash, std::hash<T>>, std::hash<typename Value::Inner>, Hash>;
        using InnerEqual = std::conditional_t<std::is_same_v<Equal, std::equal_to<T>>, std::equal_to<typename Value::Inner>, Equal>;

    public:
        using ValueType = T;
        using HandleType = typename Value::Handle;
        using InnerType = sgcl::intern<typename Value::Inner, InnerHash, InnerEqual>;
        using SizeType = size_t;

        Intern() = default;
        Intern(const Intern&) = delete;
        Intern& operator=(const Intern&) = delete;

        // The canonical object of the value: the pool's when one is alive,
        // or a new one made from the value and entered. A K other than
        // the value type (a string_view or a literal for a String) looks
        // up without making a value when Hash and Equal are transparent.
        HandleType Get(const T& value) {
            return Value::handle(_p.get(Value::inner(value)));
        }

        template<class K> requires sgcl::detail::TransparentLookup<InnerHash, InnerEqual>
        HandleType Get(const K& value) {
            return Value::handle(_p.get(value));
        }

        // The canonical object of the value when one is alive, or null
        // (the empty String for a pool of Strings)
        HandleType Find(const T& value) const noexcept {
            return Value::handle(_p.find(Value::inner(value)));
        }

        template<class K> requires sgcl::detail::TransparentLookup<InnerHash, InnerEqual>
        HandleType Find(const K& value) const noexcept {
            return Value::handle(_p.find(value));
        }

        // The entries, the dead ones not yet swept included
        SizeType Count() const noexcept {
            return _p.size();
        }

        bool IsEmpty() const noexcept {
            return _p.empty();
        }

        // The entries of the dead objects freed: how many
        SizeType Sweep() {
            return _p.sweep();
        }

        void Clear() {
            _p.clear();
        }

        // The table grown to at least `n` entries up front
        void Reserve(SizeType n) {
            _p.reserve(n);
        }

        // The default pool of the type in this interface, one for the
        // program: a managed object under a RootPtr, made on first use
        static Intern& Pool() {
            static RootPtr<Intern> p = Sgcl::Make<Intern>();
            return *p;
        }

        // Get on the default pool
        static HandleType Make(const T& value) {
            return Pool().Get(value);
        }

        template<class K> requires sgcl::detail::TransparentLookup<InnerHash, InnerEqual>
        static HandleType Make(const K& value) {
            return Pool().Get(value);
        }

        InnerType& Inner() noexcept {
            return _p;
        }

        const InnerType& Inner() const noexcept {
            return _p;
        }

    private:
        InnerType _p;
    };

    // The String of these characters, interned in the default pool of
    // Strings: the one every thread holds for them
    inline String InternString(std::string_view s) {
        return Intern<String>::Make(s);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
