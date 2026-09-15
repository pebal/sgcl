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
    // The atomic of a tracked_ptr: the operations of detail::AtomicWord on
    // the word it holds, one word, as a tracked_ptr is. It lives where a
    // tracked_ptr may: on a stack or inside a managed object.
    template<class T>
    class atomic<tracked_ptr<T>> : public detail::AtomicWord<atomic<tracked_ptr<T>>, T, tracked_ptr<T>> {
    public:
        using value_type = tracked_ptr<T>;

        atomic(const atomic&) = delete;
        atomic& operator=(const atomic&) = delete;

        // The constructors and assignments of std::atomic: from a value, a
        // null, or a unique_ptr (whose object leaves the unique state on
        // the way in)
        atomic() noexcept {}

        atomic(std::nullptr_t) noexcept
        : _val(nullptr) {
        }

        atomic(unique_ptr<T>&& p) noexcept
        : _val(std::move(p)) {
        }

        atomic(value_type p) noexcept
        : _val(p) {
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

    private:
        friend detail::AtomicWord<atomic, T, value_type>;

        detail::Pointer& _ptr() noexcept {
            return *_val._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *_val._ptr();
        }

        value_type _val;
    };

    // The atomic of a gc::tracked_ptr: the same operations on the word the
    // gc::tracked_ptr holds its object by (gc/tracked_ptr.h: the conversion
    // to sgcl::tracked_ptr<T>&), the tracked_ptr itself inside a managed
    // object or on a stack, the cell's word in any other memory (a
    // gc::tracked_ptr takes its cell in its constructor, so no store
    // allocates under a concurrent load), so that it lives anywhere: a
    // global holding an object that other threads share and that is
    // replaced at run time is `static sgcl::atomic<gc::tracked_ptr<T>>`
    // (README, "The rules"). Inside, sgcl::tracked_ptr (detail::AtomicWord);
    // outside, gc::tracked_ptr: what load(), the conversion and the
    // assignment return.
    template<class T>
    class atomic<gc::tracked_ptr<T>> : public detail::AtomicWord<atomic<gc::tracked_ptr<T>>, T, gc::tracked_ptr<T>> {
    public:
        using value_type = gc::tracked_ptr<T>;

        atomic(const atomic&) = delete;
        atomic& operator=(const atomic&) = delete;

        atomic() noexcept = default;

        atomic(std::nullptr_t) noexcept
        : _val(nullptr) {
        }

        atomic(unique_ptr<T>&& p) noexcept
        : _val(std::move(p)) {
        }

        atomic(tracked_ptr<T> p) noexcept
        : _val(p) {
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

    private:
        friend detail::AtomicWord<atomic, T, value_type>;

        detail::Pointer& _ptr() noexcept {
            return *static_cast<tracked_ptr<T>&>(_val)._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *static_cast<const tracked_ptr<T>&>(_val)._ptr();
        }

        value_type _val;
    };
}
