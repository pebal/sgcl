//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../gc/tracked_ptr.h"
#include "detail/atomic_word.h"
#include "tracked_ptr.h"

namespace sgcl {
    // The atomic view of a tracked_ptr: the operations of detail::AtomicWord
    // on a plain tracked_ptr that lives elsewhere, a member of a managed
    // object as a rule. The tracked_ptr must not be moved or destroyed
    // while a view of it exists.
    template<class T>
    class atomic_ref<tracked_ptr<T>> : public detail::AtomicWord<atomic_ref<tracked_ptr<T>>, T, tracked_ptr<T>> {
    public:
        using value_type = tracked_ptr<T>;

        atomic_ref& operator=(const atomic_ref&) = delete;

        explicit atomic_ref(value_type& p) noexcept
        : ref(p) {
        }

        atomic_ref(const atomic_ref& a) noexcept
        : ref(a.ref) {
        }

        std::nullptr_t operator=(std::nullptr_t) noexcept {
            this->store(nullptr);
            return nullptr;
        }

        void operator=(unique_ptr<T>&& p) noexcept {
            this->store(std::move(p));
        }

        value_type operator=(value_type p) noexcept {
            this->store(p);
            return p;
        }

        value_type& ref;

    private:
        friend detail::AtomicWord<atomic_ref, T, value_type>;

        detail::Pointer& _ptr() noexcept {
            return *ref._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *ref._ptr();
        }
    };

    template <class T>
    atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;

    // The atomic view of a gc::tracked_ptr: the same operations on the word
    // the gc::tracked_ptr holds its object by (gc/tracked_ptr.h: the
    // conversion to sgcl::tracked_ptr<T>&), bound once in the constructor.
    // Inside, sgcl::tracked_ptr (detail::AtomicWord); outside,
    // gc::tracked_ptr. As above, the gc::tracked_ptr must not be moved or
    // destroyed while a view of it exists.
    template<class T>
    class atomic_ref<gc::tracked_ptr<T>> : public detail::AtomicWord<atomic_ref<gc::tracked_ptr<T>>, T, gc::tracked_ptr<T>> {
    public:
        using value_type = gc::tracked_ptr<T>;

        atomic_ref& operator=(const atomic_ref&) = delete;

        explicit atomic_ref(value_type& p) noexcept
        : ref(p) {
        }

        atomic_ref(const atomic_ref& a) noexcept
        : ref(a.ref) {
        }

        std::nullptr_t operator=(std::nullptr_t) noexcept {
            this->store(nullptr);
            return nullptr;
        }

        void operator=(unique_ptr<T>&& p) noexcept {
            this->store(std::move(p));
        }

        value_type operator=(tracked_ptr<T> p) noexcept {
            this->store(p);
            return p;
        }

        tracked_ptr<T>& ref;   // the word the gc::tracked_ptr holds its object by

    private:
        friend detail::AtomicWord<atomic_ref, T, value_type>;

        detail::Pointer& _ptr() noexcept {
            return *ref._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *ref._ptr();
        }
    };

    template <class T>
    atomic_ref(gc::tracked_ptr<T>) -> atomic_ref<gc::tracked_ptr<T>>;
}
