//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/atomic_word.h"
#include "detail/handle_word.h"
#include "tracked_ptr.h"

#include <atomic>

namespace sgcl {
    // sgcl::atomic_ref<T> is std::atomic_ref<T> for every T but a
    // tracked_ptr and a handle (req::handle), as sgcl::atomic<T> is
    // std::atomic<T> (atomic.h)
    template<class T>
    class atomic_ref
    : public std::atomic_ref<T> {
    public:
        using std::atomic_ref<T>::atomic_ref;
        using std::atomic_ref<T>::operator=;
    };

    template<class T>
    atomic_ref(T&) -> atomic_ref<T>;

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

    // The atomic view of a handle (req::handle, detail/handle_word.h): the
    // operations of detail::AtomicWord on the word of a handle that lives
    // elsewhere — a member of a managed object, or the one a rooted<H>
    // holds (`atomic_ref a(*rooted)`). The handle must not be moved or
    // destroyed while a view of it exists. Identity, as atomic<H>: the
    // compare-exchanges compare the object, not its contents.
    template<req::handle H>
    class atomic_ref<H>
    : public detail::AtomicWord<atomic_ref<H>, detail::HandleStateOf<H>> {
        using State = detail::HandleStateOf<H>;
        using Base = detail::AtomicWord<atomic_ref, State>;
        using Word = tracked_ptr<State>;

    public:
        using value_type = H;

        atomic_ref& operator=(const atomic_ref&) = delete;

        explicit atomic_ref(H& h) noexcept
        : ref(h) {
        }

        atomic_ref(const atomic_ref& a) noexcept
        : ref(a.ref) {
        }

        H load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
            return _handle(Base::load(m));
        }

        operator H() const noexcept {
            return load();
        }

        void store(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Base::store(_word(h), m);
        }

        H operator=(const H& h) noexcept {
            store(h);
            return h;
        }

        H exchange(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            return _handle(Base::exchange(_word(h), m));
        }

        bool compare_exchange_strong(H& expected, const H& desired, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_strong(e, _word(desired), m);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        bool compare_exchange_strong(H& expected, const H& desired, const std::memory_order s, const std::memory_order f) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_strong(e, _word(desired), s, f);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        bool compare_exchange_weak(H& expected, const H& desired, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_weak(e, _word(desired), m);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        bool compare_exchange_weak(H& expected, const H& desired, const std::memory_order s, const std::memory_order f) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_weak(e, _word(desired), s, f);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        void wait(const H& h, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            Base::wait(_word(h), m);
        }

        H& ref;

    private:
        friend Base;

        static Word _word(const H& h) noexcept {
            return const_pointer_cast<State>(detail::HandleWord::word(h));
        }

        static H _handle(const Word& w) noexcept {
            return detail::HandleWord::make<H>(detail::HandleWordOf<H>(w));
        }

        detail::Pointer& _ptr() noexcept {
            return *detail::HandleWord::word(ref)._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *detail::HandleWord::word(ref)._ptr();
        }
    };

    template <class T>
    atomic_ref(tracked_ptr<T>) -> atomic_ref<tracked_ptr<T>>;

    // The atomic of a root_ptr: the view of the word it holds its object by
    template <class T>
    atomic_ref(root_ptr<T>) -> atomic_ref<tracked_ptr<T>>;
}
