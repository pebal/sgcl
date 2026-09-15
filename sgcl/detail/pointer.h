//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "array_metadata.h"
#include "thread.h"

#include <cstring>

namespace sgcl::detail {
    // The word behind every tracked_ptr, atomic and containers' pointers:
    // one atomic word holding the address of a managed object (or of an
    // element of a container's buffer: base_address_of, data_base_address_of
    // find the object or the buffer from it), stored through the write
    // barrier (_update: the state on the target, the card of this word's
    // page). The loads are relaxed: what a load reads is held by the
    // barrier of whoever stored it. The compare-exchanges are the atomic's
    // (atomic_word.h), with the barrier on success. What is asked of a
    // pointer's target (its type, its size, whether it is a buffer) is
    // answered from the page.
    class Pointer {
    public:
        Pointer() noexcept
        : _ptr(nullptr) {
        }

        Pointer(std::nullptr_t) noexcept
        : _ptr(nullptr) {
        }

        // One store, then the barrier: the word is never null first.
        Pointer(const void* p) noexcept
        : _ptr(const_cast<void*>(p)) {
            _update(p);
        }

        Pointer(const Pointer& p) noexcept
        : Pointer(p.load()) {
        }

        // The first store of an object released from its unique_ptr: the
        // barrier that takes it out of the unique state (page.h:
        // set_state_released), on this path only, and before the word: a
        // thread that loads the word (an atomic, an atomic_ref) copies it
        // through the ordinary barrier, which a unique object must never
        // reach; the released state alone holds the object for the cycle,
        // whatever the word says meanwhile.
        struct Released {};
        Pointer(const void* p, Released) noexcept
        : _ptr(_released(p)) {
            _card(p);
        }

        Pointer& operator=(const Pointer& p) noexcept {
            store(p.load());
            return *this;
        }

        void* load() const noexcept {   // relaxed: what it reads, its storer's barrier holds
            return _ptr.load(std::memory_order_relaxed);
        }

        // A plain load, for the thread that owns the word: a container
        // reading the pointer to its own buffer. Unlike the atomic load, the
        // compiler may keep the value in a register across a loop.
        void* load_plain() const noexcept {
            static_assert(sizeof(_ptr) == sizeof(void*));
            return *reinterpret_cast<void* const*>(&_ptr);   // a typed load: no aliasing with the buffer's header
        }

        void* load(const std::memory_order m) const noexcept {
            return _ptr.load(m);
        }

        void store(std::nullptr_t) noexcept {
            _ptr.store(nullptr, std::memory_order_relaxed);   // a null publishes nothing
        }

        void store(const void* p) noexcept {
            store_no_update(p);
            _update(p);
        }

        void store(std::nullptr_t, const std::memory_order m) noexcept {
            _ptr.store(nullptr, m);
        }

        void store(const void* p, const std::memory_order m) noexcept {
            _ptr.store(const_cast<void*>(p), m);
            _update(p);
        }

        void store_released(const void* p) noexcept {
            store_no_update(_released(p));
            _card(p);
        }

        void store_released(const void* p, const std::memory_order m) noexcept {
            _ptr.store(_released(p), m);
            _card(p);
        }

        void store_no_update(const void* p) noexcept {
            _ptr.store(const_cast<void*>(p), std::memory_order_release);
        }

        bool compare_exchange_strong(void*& o, std::nullptr_t, const std::memory_order m) noexcept {
            return _ptr.compare_exchange_strong(o, nullptr, m);
        }

        bool compare_exchange_strong(void*& o, const void* n, const std::memory_order m) noexcept {
            auto res = _ptr.compare_exchange_strong(o, const_cast<void*>(n), m);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool compare_exchange_strong(void*& o, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            return _ptr.compare_exchange_strong(o, nullptr, s, f);
        }

        bool compare_exchange_strong(void*& o, const void* n, const std::memory_order s, const std::memory_order f) noexcept {
            auto res = _ptr.compare_exchange_strong(o, const_cast<void*>(n), s, f);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool compare_exchange_weak(void*& o, std::nullptr_t, const std::memory_order m) noexcept {
            return _ptr.compare_exchange_weak(o, nullptr, m);
        }

        bool compare_exchange_weak(void*& o, const void* n, const std::memory_order m) noexcept {
            auto res = _ptr.compare_exchange_weak(o, const_cast<void*>(n), m);
            if (res) {
                _update(n);
            }
            return res;
        }

        bool compare_exchange_weak(void*& o, std::nullptr_t, const std::memory_order s, const std::memory_order f) noexcept {
            return _ptr.compare_exchange_weak(o, nullptr, s, f);
        }

        bool compare_exchange_weak(void*& o, const void* n, const std::memory_order s, const std::memory_order f) noexcept {
            auto res = _ptr.compare_exchange_weak(o, const_cast<void*>(n), s, f);
            if (res) {
                _update(n);
            }
            return res;
        }

        // The rest of std::atomic's interface on the word (atomic.h)
        bool is_lock_free() const noexcept {
            return _ptr.is_lock_free();
        }

        void notify_one() noexcept {
            _ptr.notify_one();
        }

        void notify_all() noexcept {
            _ptr.notify_all();
        }

        void wait(const void* p, std::memory_order m) const noexcept {
            _ptr.wait(const_cast<void*>(p), m);
        }

        // The managed object (its first byte) an address is into: a base
        // subobject or a member of it, or an element of a buffer
        inline static void* base_address_of(const void* p) noexcept {
            return p ? Page::base_address_of(p) : nullptr;
        }

        void* base_address() const noexcept {
            auto p = load();
            return p ? base_address_of(p) : nullptr;
        }

        // The same, past the header of a buffer: its first element
        inline static void* data_base_address_of(const void* p) noexcept {
            auto page = Page::page_of(p);
            auto data = page->pointer_of(page->index_of(p));
            return page->is_array ? ((ArrayBase*)data) + 1 : data;
        }

        void* data_base_address() const noexcept {
            auto p = load();
            return p ? data_base_address_of(p) : nullptr;
        }

        // The dynamic type of the object at p (a buffer's: its element
        // type's array), or T for a null pointer
        template<class T>
        inline static const std::type_info& type_info(const void* p) noexcept {
            if (p) {
                auto metadata = Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (ArrayBase*)Page::base_address_of(p);
                    auto metadata = array->metadata;
                    return metadata->type_info;
                } else {
                    return metadata.type_info;
                }
            } else {
                return typeid(T);
            }
        }

        template<class T>
        const std::type_info& type_info() const noexcept {
            auto p = load();
            return type_info<T>(p);
        }

        // Whether p is into a buffer (the storage of a container), and the
        // size of one object or element, the number of elements, and the
        // buffer's capacity word (null for an object): what a container
        // asks about the storage it holds
        inline static bool is_array(const void* p) noexcept {
            return p ? Page::metadata_of(p).is_array : false;
        }

        bool is_array() const noexcept {
            auto p = load();
            return is_array(p);
        }

        inline static size_t object_size(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    auto metadata = array->metadata;
                    return metadata->object_size;
                } else {
                    return metadata.object_size;
                }
            }
            return 0;
        }

        size_t object_size() const noexcept {
            auto p = load();
            return object_size(p);
        }

        inline static size_t size(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return array->capacity;
                } else {
                    return 1;
                }
            }
            return 0;
        }

        inline static size_t capacity(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return array->capacity;
                } else {
                    return 1;
                }
            }
            return 0;
        }

        inline static const size_t* capacity_ptr(const void* p) noexcept {
            if (p) {
                auto metadata = detail::Page::metadata_of(p);
                if (metadata.is_array) {
                    auto array = (detail::ArrayBase*)Page::base_address_of(p);
                    return &array->capacity;
                } else {
                    return nullptr;
                }
            }
            return nullptr;
        }

    private:
        // Write barrier: the target becomes Reachable (the collector marks
        // it in this cycle), and the page holding this pointer is carded for
        // the young cycles (page.h: mark_card).
        void _update(const void* p) noexcept {
            if (p) {
                Page::set_state<State::Reachable>(p);
                Page::mark_card(this);
            }
        }

        // The released state, set before the word is stored (Released)
        static void* _released(const void* p) noexcept {
            if (p) {
                Page::set_state_released(p);
            }
            return const_cast<void*>(p);
        }

        void _card(const void* p) noexcept {
            if (p) {
                Page::mark_card(this);
            }
        }

        RawPointer _ptr;
    };
}
