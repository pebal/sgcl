//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/tracked_ptr.h"
#include "detail/atomic_word.h"

namespace sgcl {
    // The atomic view of a tracked_ptr: the operations of detail::AtomicWord
    // on a plain tracked_ptr that lives elsewhere, a member of a managed
    // object as a rule, or the one a root_ptr holds its object by
    // (root_ptr.h: ptr(), the cell's word; a root_ptr converts to it, so
    // `atomic_ref a(root)` is the atomic of a root that lives anywhere).
    // The tracked_ptr must not be moved or destroyed while a view of it
    // exists.
    template<class T>
    class atomic_ref<tracked_ptr<T>>
    : public detail::AtomicWord<atomic_ref<tracked_ptr<T>>, T> {
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
        friend detail::AtomicWord<atomic_ref, T>;

        detail::Pointer& _ptr() noexcept {
            return *ref._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *ref._ptr();
        }
    };

    template <class T>
    atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;

    // The atomic of a root_ptr: the view of the word it holds its object by
    template <class T>
    atomic_ref(root_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
}
