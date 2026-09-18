//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../make_tracked.h"
#include "pointer_word.h"
#include "slot.h"

#include <cstring>
#include <functional>
#include <typeinfo>
#include <utility>

namespace sgcl::detail {
    template<class T>
    struct IsReferenceWrapper : std::false_type {};

    template<class T>
    struct IsReferenceWrapper<std::reference_wrapper<T>> : std::true_type {};

    // Where any (any.h) and function (function.h) keep a value of a type
    // they do not know: by what it is (pointer_word.h). A pointer word
    // (tracked_ptr, weak_ptr) is constructed in the word of the storage
    // itself; a small value that cannot hold a pointer in a buffer of 16
    // bytes; anything else, an object with tracked pointers among its
    // data first of all, in a managed node of its own (a Slot<T>, as the
    // containers keep their elements), held by a tracked_ptr<Slot<T>>
    // constructed in the same word. So the word holds null or an address
    // and nothing else, and what it holds is traced: a
    // value that points back at the object holding the storage is a
    // cycle, collected like any other (which a value owned the way a
    // unique_ptr owns, a root by the state of its slot, would not be).
    // The value is destroyed the moment the storage drops it, on that
    // thread, as a container destroys an erased element (slot.h); the
    // node is reclaimed by the collector later. std::any and
    // std::function keep the small value in a buffer inside themselves,
    // where a pointer would share its word with the data of other values
    // (README: Pointer maps), and the large one on the unmanaged heap,
    // where a tracked_ptr may not live. The word is a tracked_ptr, so the
    // storage lives where one may. Extra is what the manager carries
    // besides: the invoker of a function, nothing for an any.
    template<class Extra = std::nullptr_t>
    class ValueStorage {
    protected:
        static constexpr size_t BufferSize = 16;
        static constexpr size_t BufferAlign = 8;

        template<class T>
        static constexpr bool Word = IsPointerWord<T>;

        // A reference_wrapper is one raw pointer to an object elsewhere,
        // never a tracked pointer, so it goes in the buffer as std keeps
        // it (the trait would send it to a node: a word that is not
        // trivially constructible); function's assignment from one is
        // noexcept and allocates nothing
        template<class T>
        static constexpr bool Inline = (IsReferenceWrapper<T>::value || !MayContainTracked<T>::value) && sizeof(T) <= BufferSize && alignof(T) <= BufferAlign && std::is_nothrow_move_constructible_v<T>;

        struct Manager {
            const std::type_info& type;
            void (*copy)(ValueStorage& to, const ValueStorage& from);   // null for a type that is not copied
            void (*move)(ValueStorage& to, ValueStorage& from) noexcept;
            void (*destroy)(ValueStorage& s) noexcept;
            void* (*get)(const ValueStorage& s) noexcept;
            Extra extra;
        };

        // The pointer word constructed in the storage's word
        template<class T, bool Copyable, Extra X>
        struct WordOps {
            static void copy(ValueStorage& to, const ValueStorage& from) {
                if constexpr(Copyable) {
                    ::new(to._word) T(*static_cast<const T*>(get(from)));
                    to._manager = from._manager;
                }
            }
            static void move(ValueStorage& to, ValueStorage& from) noexcept {
                ::new(to._word) T(std::move(*static_cast<T*>(get(from))));
                to._manager = from._manager;
                destroy(from);
            }
            static void destroy(ValueStorage& s) noexcept {
                static_cast<T*>(get(s))->~T();   // leaves null in the word
                s._manager = nullptr;
            }
            static void* get(const ValueStorage& s) noexcept {
                return const_cast<unsigned char*>(s._word);
            }
            static constexpr Manager manager = {typeid(T), Copyable ? copy : nullptr, move, destroy, get, X};
        };

        // The value in the buffer
        template<class T, bool Copyable, Extra X>
        struct InlineOps {
            static void copy(ValueStorage& to, const ValueStorage& from) {
                if constexpr(Copyable) {
                    ::new(to._buffer) T(*static_cast<const T*>(get(from)));
                    to._manager = from._manager;
                }
            }
            static void move(ValueStorage& to, ValueStorage& from) noexcept {
                ::new(to._buffer) T(std::move(*static_cast<T*>(get(from))));
                to._manager = from._manager;
                destroy(from);
            }
            static void destroy(ValueStorage& s) noexcept {
                static_cast<T*>(get(s))->~T();
                s._manager = nullptr;
            }
            static void* get(const ValueStorage& s) noexcept {
                return const_cast<unsigned char*>(s._buffer);
            }
            static constexpr Manager manager = {typeid(T), Copyable ? copy : nullptr, move, destroy, get, X};
        };

        // The value in a managed node of its own, the node's pointer in the
        // storage's word
        template<class T, bool Copyable, Extra X>
        struct NodeOps {
            using Node = Slot<T>;
            using NodePtr = tracked_ptr<Node>;

            static NodePtr& node(const ValueStorage& s) noexcept {
                return *const_cast<NodePtr*>(reinterpret_cast<const NodePtr*>(s._word));
            }
            // The node made and the value constructed in it first, the
            // node placed in the word only then: a constructor that throws
            // leaves the word as it was (null, with no manager), the node
            // let go by its unique_ptr (the slot's destructor skips the
            // value that was never made: slot.h)
            template<class... A>
            static T& make(ValueStorage& s, A&&... a) {
                static_assert(sizeof(detail::Array<sizeof(Node)>) <= detail::PageDataSize, "a value larger than a page is not supported");
                auto node = make_tracked<Node>();
                T& v = node->construct(std::forward<A>(a)...);
                ::new(s._word) NodePtr(std::move(node));
                return v;
            }
            static void copy(ValueStorage& to, const ValueStorage& from) {
                if constexpr(Copyable) {
                    make(to, *static_cast<const T*>(get(from)));
                    to._manager = from._manager;
                }
            }
            static void move(ValueStorage& to, ValueStorage& from) noexcept {
                ::new(to._word) NodePtr(std::move(node(from)));
                to._manager = from._manager;
                node(from).~NodePtr();   // the moved-from pointer: null
                from._manager = nullptr;
            }
            static void destroy(ValueStorage& s) noexcept {
                // The value now, the node to the collector. Not in a sweep:
                // the storage dying inside a dying managed object has a
                // node that is garbage of the same sweep, whose slot
                // destroys the value itself when the sweep reaches it (or
                // has already: a second destruction would run on a value
                // that is gone), as the containers' nodes do (list.h)
                if (!sweeping) {
                    node(s)->destroy();
                }
                node(s).~NodePtr();
                s._manager = nullptr;
            }
            static void* get(const ValueStorage& s) noexcept {
                return &node(s)->value;
            }
            static constexpr Manager manager = {typeid(T), Copyable ? copy : nullptr, move, destroy, get, X};
        };

        template<class T, bool Copyable, Extra X>
        using Ops = std::conditional_t<Word<T>, WordOps<T, Copyable, X>, std::conditional_t<Inline<T>, InlineOps<T, Copyable, X>, NodeOps<T, Copyable, X>>>;

        ValueStorage() noexcept = default;

        ValueStorage(const ValueStorage& o) {
            if (o._manager) {
                o._manager->copy(*this, o);
            }
        }

        ValueStorage(ValueStorage&& o) noexcept {
            if (o._manager) {
                o._manager->move(*this, o);
            }
        }

        ~ValueStorage() {
            _reset();
        }

        ValueStorage& operator=(const ValueStorage&) = delete;
        ValueStorage& operator=(ValueStorage&&) = delete;

        template<class T, bool Copyable, Extra X, class... A>
        T& _emplace(A&&... a) {
            if constexpr(Word<T>) {
                auto p = ::new(_word) T(std::forward<A>(a)...);
                _manager = &WordOps<T, Copyable, X>::manager;
                return *p;
            } else if constexpr(Inline<T>) {
                auto p = ::new(_buffer) T(std::forward<A>(a)...);
                _manager = &InlineOps<T, Copyable, X>::manager;
                return *p;
            } else {
                auto& v = NodeOps<T, Copyable, X>::make(*this, std::forward<A>(a)...);
                _manager = &NodeOps<T, Copyable, X>::manager;
                return v;
            }
        }

        // The value of type T held here, without the manager: for an
        // invoker that knows T
        template<class T>
        static T* _value(const ValueStorage& s) noexcept {
            return static_cast<T*>(Ops<T, false, Extra{}>::get(s));
        }

        void _reset() noexcept {
            if (_manager) {
                _manager->destroy(*this);
            }
        }

        void _swap(ValueStorage& o) noexcept {
            if (this == &o) {
                return;
            }
            ValueStorage tmp(std::move(o));
            if (_manager) {
                _manager->move(o, *this);
            }
            if (tmp._manager) {
                tmp._manager->move(*this, tmp);
            }
        }

        void* _get() const noexcept {
            return _manager->get(*this);
        }

        const Manager* _manager = nullptr;
        // null or an address, never data: a pointer word, or the pointer to
        // the node holding the value, constructed in place
        alignas(BufferAlign) unsigned char _word[sizeof(void*)] = {};
        alignas(BufferAlign) unsigned char _buffer[BufferSize];
    };
}
