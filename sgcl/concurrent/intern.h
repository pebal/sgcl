//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/nothrow_function.h"
#include "../core/detail/transparent.h"
#include "../core/string.h"
#include "../core/make_tracked.h"
#include "../core/root_ptr.h"
#include "../core/atomic.h"
#include "set.h"
#include "detail/concurrent_weak_table.h"

#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>

namespace sgcl::concurrent::detail {
    using namespace sgcl::detail;
    // How a pool holds the values of a type: what the pool hands out (the
    // handle), what its entries address weakly (the object, without the
    // const the handle adds: a weak pointer locks through the hazard
    // pointer, which addresses the object as it is), how a value becomes
    // an object and an object a handle again. The general case puts the
    // value into a managed object of its own and hands out a
    // tracked_ptr<const T> to it.
    template<class T>
    struct InternTraits {
        using handle = tracked_ptr<const T>;
        using object = T;

        template<class K>
        SGCL_INLINE_HOT static handle make(const K& value) noexcept(std::is_nothrow_constructible_v<T, const K&>) {
            return make_tracked<T>(value);
        }

        SGCL_INLINE_HOT static handle lock(const weak_ptr<object>& w) noexcept {
            return w.lock();
        }

        SGCL_INLINE_HOT static bool alive(const handle& h) noexcept {
            return static_cast<bool>(h);
        }

        SGCL_INLINE_HOT static const T& value(const handle& h) noexcept {
            return *h;
        }

        SGCL_INLINE_HOT static weak_ptr<object> weak(const handle& h) noexcept {
            return weak_ptr<object>(const_pointer_cast<object>(h));
        }
    };

    // A string is one word pointing at an object never modified, so the
    // interned string is the string itself: the entry addresses the
    // string's object, the handle is a string over it, and a string
    // passed in enters the pool as it is when its value is new (Java's
    // String.intern), a view or a literal becomes a string first. The
    // empty string is null, one value with no object: it is never entered
    // and never looked up. The string over its object is built through
    // detail::StringAccess (string.h), as atomic<string> builds one from
    // the word it loads.
    template<class CharT, class Traits>
    struct InternTraits<basic_string<CharT, Traits>> {
        using String = basic_string<CharT, Traits>;
        using View = typename String::view_type;
        using handle = String;
        using object = void;

        SGCL_INLINE_HOT static handle make(const String& s) noexcept {
            return s;
        }

        template<class K>
        SGCL_INLINE_HOT static handle make(const K& value) {
            return String(View(value));
        }

        SGCL_INLINE_HOT static handle lock(const weak_ptr<object>& w) noexcept {
            tracked_ptr<object> p = w.lock();
            if (!p) {
                return String();
            }
            return detail::StringAccess::over<String>(tracked_ptr<const void>(p));
        }

        SGCL_INLINE_HOT static bool alive(const handle& h) noexcept {
            return h.object() != nullptr;
        }

        SGCL_INLINE_HOT static const String& value(const handle& h) noexcept {
            return h;
        }

        SGCL_INLINE_HOT static weak_ptr<object> weak(const handle& h) noexcept {
            return weak_ptr<object>(tracked_ptr<object>(const_cast<void*>(h.object())));
        }

        template<class K>
        SGCL_INLINE_HOT static bool is_empty(const K& value) noexcept {
            return View(value).empty();
        }
    };

    // The hash and the equality of a pool's entries, by the contents of
    // the objects: an entry hashes to the hash it was placed with (the
    // hash of its contents, kept because the object may be gone by the
    // time the entry is erased, and with its top bit set so that the
    // word is never taken for a heap address by a conservative scan); a
    // value hashes by Hash. An entry equals a value, or another entry,
    // when its object is alive and KeyEqual says so of the contents: a
    // dead entry equals nothing, so it is never found and never blocks
    // the entry that replaces it. Transparent for the table, which is
    // keyed by entries and searched by values; whether a value of another
    // type than T is accepted is decided by the pool from Hash and
    // KeyEqual.
    template<class T, class Hash>
    struct InternHash {
        using is_transparent = void;
        using Entry = WeakKey<typename InternTraits<T>::object>;

        [[no_unique_address]] Hash hash;

        SGCL_INLINE_HOT size_t operator()(const Entry& e) const noexcept {
            return e.hash;
        }

        template<class K>
        SGCL_INLINE_HOT size_t operator()(const K& value) const noexcept {
            return hash(value) | ~(size_t(-1) >> 1);
        }
    };

    template<class T, class KeyEqual>
    struct InternEqual {
        using is_transparent = void;
        using Traits = InternTraits<T>;
        using Entry = WeakKey<typename Traits::object>;

        [[no_unique_address]] KeyEqual equal;

        SGCL_INLINE_HOT bool operator()(const Entry& a, const Entry& b) const noexcept {
            auto x = Traits::lock(a.weak);
            if (!Traits::alive(x)) {
                return false;
            }
            auto y = Traits::lock(b.weak);
            return Traits::alive(y) && equal(Traits::value(x), Traits::value(y));
        }

        template<class K>
        SGCL_INLINE_HOT bool operator()(const Entry& a, const K& value) const noexcept {
            auto x = Traits::lock(a.weak);
            return Traits::alive(x) && equal(Traits::value(x), value);
        }
    };
}

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // A pool where equal values share one managed object, Go's unique
    // package and Java's String.intern: make(value) is the canonical
    // object of the value, the one the pool holds when it is alive, or a
    // new one made from the value and entered, so that a program holds
    // one copy of each distinct value it interns and compares two by
    // their identity, a pointer comparison. The pool does not keep its
    // objects alive: an entry is a weak pointer to the canonical object,
    // found by the hash of the object's contents and compared through
    // the live object (detail::InternHash, InternEqual), so an object
    // nobody holds any more is collected, its entry is dead from then on,
    // never found, swept out every so many insertions (the sweep of the
    // concurrent weak containers, detail/concurrent_weak_table.h: by the
    // inserting thread, one at a time) and on sweep(), and the next
    // make of that value makes a new object. The table is the lock-free
    // hash set of set: find is wait-free and writes nothing once the
    // value's bucket has its dummy node, of and make are lock-free, and two threads interning the
    // same new value at once both get the object of the one whose entry
    // won the table's compare-exchange, the other object being garbage.
    // A value of another type finds the object when Hash and KeyEqual
    // are transparent, as std::hash and std::equal_to of a string are: a
    // string_view or a literal interns a string with no string made for
    // the search. For sgcl::string the interned string is the string
    // itself (detail::InternTraits): make hands back a string, not a
    // pointer to one, and intern_string(view) is intern<string>::make.
    // The pool holds tracked pointers and lives where a tracked_ptr may;
    // the default pool of a type, pool(), is a managed object under a
    // root_ptr, made on first use, and make(value) is of(value) on it.
    template<class T, class Hash = std::hash<T>, class KeyEqual = std::equal_to<T>>
    class intern : detail::ConcurrentWeakTable<typename detail::InternTraits<T>::object, set<detail::WeakKey<typename detail::InternTraits<T>::object>, detail::InternHash<T, Hash>, detail::InternEqual<T, KeyEqual>>> {
        using Traits = detail::InternTraits<T>;
        using Entry = detail::WeakKey<typename Traits::object>;
        using Base = detail::ConcurrentWeakTable<typename Traits::object, set<Entry, detail::InternHash<T, Hash>, detail::InternEqual<T, KeyEqual>>>;
        using Base::_table;
        static_assert(detail::nothrow_function_object<Hash, const T&>, "sgcl::concurrent::intern: Hash must be noexcept");
        static_assert(detail::nothrow_function_object<KeyEqual, const T&, const T&>, "sgcl::concurrent::intern: KeyEqual must be noexcept");

        // What of() does to the value: the object made from it
        template<class K>
        static constexpr bool NothrowMake = noexcept(Traits::make(std::declval<const K&>()));

    public:
        using value_type = T;
        using handle = typename Traits::handle;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using size_type = size_t;

        intern() = default;

        // The canonical object of the value: the pool's when one is alive,
        // or a new one made from the value and entered (hash::of's name for
        // what a value maps to; get would read, and this enters). Lock-free.
        SGCL_INLINE_HOT handle of(const T& value) noexcept(NothrowMake<T>) {
            return _of(value);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT handle of(const K& value) noexcept(NothrowMake<K>) {
            return _of(value);
        }

        // The canonical object of the value when one is alive, or null (the
        // empty string for a pool of strings). Wait-free and writes
        // nothing once the value's bucket has its dummy node (set).
        SGCL_INLINE_HOT handle find(const T& value) const noexcept {
            return _find(value);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT handle find(const K& value) const noexcept {
            return _find(value);
        }

        // The entries, the dead ones not yet swept included; sweep()
        // drops the dead ones and returns how many; clear() forgets every
        // object, the live ones included, which live on where they are
        // held and are made again by the next make
        using Base::size;
        using Base::empty;
        using Base::sweep;
        using Base::clear;

        // Buckets for at least `count` entries, grown now rather than by
        // the insertions
        SGCL_INLINE_HOT void reserve(size_type count) noexcept {
            _table.reserve(count);
        }

        // The default pool of the type, one for the program: a managed
        // object under a root_ptr, made on first use
        static intern& pool() noexcept {
            static root_ptr<intern> p = make_tracked<intern>();
            return *p;
        }

        // of on the default pool: Go's unique.Make
        SGCL_INLINE_HOT static handle make(const T& value) noexcept(NothrowMake<T>) {
            return pool().of(value);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        SGCL_INLINE_HOT static handle make(const K& value) noexcept(NothrowMake<K>) {
            return pool().of(value);
        }

    private:
        template<class K>
        handle _of(const K& value) noexcept(NothrowMake<K>) {
            if constexpr(requires { Traits::is_empty(value); }) {
                if (Traits::is_empty(value)) {
                    return handle();
                }
            }
            // One search: the entry of a live object of the value is
            // the answer; when there is none, a new object is made (once)
            // and entered unless another thread's got in first, whose
            // object is shared then and this one dropped; an entry found
            // equal whose object has died since is passed over by the
            // next try
            handle made;
            for (;;) {
                auto [it, inserted] = _table._insert_absent(value, [&] {
                    if (!Traits::alive(made)) {
                        made = Traits::make(value);
                    }
                    return _table._make_node(Entry{Traits::weak(made), _table.hash_function()(value)});
                });
                if (inserted) {
                    this->_inserted_one();
                    return made;
                }
                if (handle h = Traits::lock(it->weak); Traits::alive(h)) {
                    return h;
                }
            }
        }

        template<class K>
        SGCL_INLINE_HOT handle _find(const K& value) const noexcept {
            auto it = _table.find(value);
            return it != _table.end() ? Traits::lock(it->weak) : handle();
        }
    };

    // The string of these characters, interned in the default pool of
    // strings: the one every thread holds for them
    SGCL_INLINE_HOT string intern_string(std::string_view s) {
        return intern<string>::make(s);
    }
}
