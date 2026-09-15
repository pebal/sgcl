//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../sgcl/sgcl.h"
#include "tracked_ptr.h"

// The family that lives anywhere. gc::tracked_ptr is the pointer
// (tracked_ptr.h); every container, observer and coroutine of sgcl that
// holds its memory by a word takes the kind of that word as its last
// parameter, and here each is named with gc::tracked_ptr in that place: a
// gc::vector or a gc::map may be a global, an element of a std container
// or a capture of a lambda on the heap, where the sgcl one, holding an
// sgcl::tracked_ptr, may not. The nodes and buffers are the same managed
// objects either way; what a gc container pays is the test of a
// gc::tracked_ptr on each access to its root word. The elements are a
// choice apart, and one the container makes moot for pointers: an element
// type that names a tracked_type (gc::tracked_ptr names sgcl::tracked_ptr)
// is stored as that type (sgcl/detail/managed.h), since in a managed
// buffer or node the two are one word in the tracked mode, so a
// gc::vector<gc::tracked_ptr<T>> keeps sgcl::tracked_ptrs and costs what
// an sgcl::vector<sgcl::tracked_ptr<T>> does, while its value_type, its
// references and its iterators stay gc::tracked_ptr<T>.
// The names are alias templates: class template argument deduction works
// through them from the constructors and, for vector and array, from an
// iterator pair (the guides of sgcl carry Ptr for that); the iterator-pair
// guides of the associative containers do not carry through an alias, as
// their key type sits in a nested name, so those are named with their
// arguments.
namespace gc {
    template<class T>
    using weak_ptr = sgcl::weak_ptr<T, tracked_ptr>;
    template<class Key, class T>
    using weak_map = sgcl::weak_map<Key, T, tracked_ptr>;
    template<class Key, class T>
    using weak_multimap = sgcl::weak_multimap<Key, T, tracked_ptr>;
    template<class Key>
    using weak_set = sgcl::weak_set<Key, tracked_ptr>;

    using sgcl::atomic;
    using sgcl::atomic_ref;
    using sgcl::unique_ptr;   // lives anywhere as it is
    // any and function hold their value by a word of the kind: the gc
    // ones live anywhere. variant and expected have no word of their own:
    // where they may live is decided by what they hold.
    using any = sgcl::basic_any<tracked_ptr>;
    template<class Signature>
    using function = sgcl::function<Signature, tracked_ptr>;
    template<class Signature>
    using move_only_function = sgcl::move_only_function<Signature, tracked_ptr>;
    using sgcl::variant;
    using sgcl::monostate;
    using sgcl::expected;
    using sgcl::unexpected;
    using sgcl::unexpect;
    using sgcl::unexpect_t;
    using sgcl::bad_expected_access;
    // the standard types safe with a tracked_ptr inside, as sgcl names them (sgcl/aliases.h)
    using sgcl::optional;
    using sgcl::nullopt;
    using sgcl::nullopt_t;
    using sgcl::make_optional;
    using sgcl::pair;
    using sgcl::make_pair;
    using sgcl::tuple;
    using sgcl::make_tuple;
    using sgcl::tie;
    using sgcl::forward_as_tuple;

    // gc::make_tracked is sgcl::make_tracked, the same function (a
    // unique_ptr, deterministic until converted): a using-declaration, not
    // a wrapper, so that argument-dependent lookup on a gc::tracked_ptr
    // argument finds one entity, not two.
    using sgcl::make_tracked;

    template<class T>
    using vector = sgcl::vector<T, tracked_ptr>;
    template<class T, size_t N = sgcl::dynamic_extent>
    using array = sgcl::array<T, N, tracked_ptr>;
    template<class T>
    using deque = sgcl::deque<T, tracked_ptr>;
    template<class T>
    using list = sgcl::list<T, tracked_ptr>;
    template<class T>
    using forward_list = sgcl::forward_list<T, tracked_ptr>;
    template<class Key, class T, class Compare = std::less<Key>>
    using map = sgcl::map<Key, T, Compare, tracked_ptr>;
    template<class Key, class T, class Compare = std::less<Key>>
    using multimap = sgcl::multimap<Key, T, Compare, tracked_ptr>;
    template<class Key, class Compare = std::less<Key>>
    using set = sgcl::set<Key, Compare, tracked_ptr>;
    template<class Key, class Compare = std::less<Key>>
    using multiset = sgcl::multiset<Key, Compare, tracked_ptr>;
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using unordered_map = sgcl::unordered_map<Key, T, Hash, KeyEqual, tracked_ptr>;
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using unordered_multimap = sgcl::unordered_multimap<Key, T, Hash, KeyEqual, tracked_ptr>;
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using unordered_set = sgcl::unordered_set<Key, Hash, KeyEqual, tracked_ptr>;
    template<class Key, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    using unordered_multiset = sgcl::unordered_multiset<Key, Hash, KeyEqual, tracked_ptr>;
    template<class T, class Container = deque<T>>
    using stack = sgcl::stack<T, Container>;
    template<class T, class Container = deque<T>>
    using queue = sgcl::queue<T, Container>;
    template<class T, class Container = vector<T>, class Compare = std::less<typename Container::value_type>>
    using priority_queue = sgcl::priority_queue<T, Container, Compare>;

    template<class T>
    using expiry_queue = sgcl::expiry_queue<T, tracked_ptr>;

    using sgcl::managed_frame;
    template<class Promise>
    using frame_ptr = sgcl::frame_ptr<Promise, tracked_ptr>;
    template<class T = void>
    using task = sgcl::task<T, tracked_ptr>;
    template<class T>
    using generator = sgcl::generator<T, tracked_ptr>;

    // The free functions of sgcl over the types above: the casts of a
    // unique_ptr (the casts of a gc::tracked_ptr are in tracked_ptr.h),
    // the tuple access and to_array of an array, erase and erase_if of
    // the containers (which sgcl also puts into std)
    using sgcl::static_pointer_cast;
    using sgcl::const_pointer_cast;
    using sgcl::dynamic_pointer_cast;
    using sgcl::get;
    using sgcl::get_if;
    using sgcl::holds_alternative;
    using sgcl::visit;
    using sgcl::variant_size;
    using sgcl::variant_size_v;
    using sgcl::variant_alternative;
    using sgcl::variant_alternative_t;
    using sgcl::any_cast;
    // make_any for the gc kind
    template<class T, class... A>
    any make_any(A&&... a) {
        return any(std::in_place_type<T>, std::forward<A>(a)...);
    }
    template<class T, class U, class... A>
    any make_any(std::initializer_list<U> il, A&&... a) {
        return any(std::in_place_type<T>, il, std::forward<A>(a)...);
    }
    using sgcl::to_array;
    using sgcl::dynamic_extent;
    using sgcl::erase;
    using sgcl::erase_if;

    using sgcl::collector;
    namespace config = sgcl::config;
}
