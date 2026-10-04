//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/atomic_word.h"
#include "detail/handle_word.h"
#include "string.h"
#include "tracked_ptr.h"

namespace sgcl {
    // sgcl::atomic<T> is std::atomic<T> for every T but the ones below: the
    // tracked pointers and the handles (a string, an io::file, a channel:
    // req::handle), whose atomics are the library's own, over the word
    // each of them is. So a program names one atomic for its flags, its
    // counters and its pointers alike.
    template<class T>
    class atomic
    : public std::atomic<T> {
    public:
        using std::atomic<T>::atomic;
        using std::atomic<T>::operator=;
    };

    // The atomic of a tracked_ptr: the operations of detail::AtomicWord on
    // the word it holds, one word, as a tracked_ptr is. It lives where a
    // tracked_ptr may: on a stack or inside a managed object; a global
    // holding an object that other threads share and that is replaced at
    // run time is a root_ptr with an atomic_ref over it (atomic_ref.h).
    template<class T>
    class atomic<tracked_ptr<T>>
    : public detail::AtomicWord<atomic<tracked_ptr<T>>, T> {
    public:
        using value_type = tracked_ptr<T>;

        atomic(const atomic&) = delete;
        atomic& operator=(const atomic&) = delete;

        // The constructors and assignments of std::atomic: from a value, a
        // null, or a unique_ptr (whose object leaves the unique state on
        // the way in)
        SGCL_INLINE_HOT atomic() noexcept {}

        SGCL_INLINE_HOT atomic(std::nullptr_t) noexcept
        : _val(nullptr) {
        }

        SGCL_INLINE_HOT atomic(unique_ptr<T>&& p) noexcept
        : _val(std::move(p)) {
        }

        SGCL_INLINE_HOT atomic(value_type p) noexcept
        : _val(p) {
        }

        SGCL_INLINE_HOT std::nullptr_t operator=(std::nullptr_t) noexcept {
            this->store(nullptr);
            return nullptr;
        }

        SGCL_INLINE_HOT void operator=(unique_ptr<T>&& p) noexcept {
            this->store(std::move(p));
        }

        SGCL_INLINE_HOT value_type operator=(value_type p) noexcept {
            this->store(p);
            return p;
        }

    private:
        friend detail::AtomicWord<atomic, T>;

        SGCL_INLINE_HOT detail::Pointer& _ptr() noexcept {
            return *_val._ptr();
        }

        SGCL_INLINE_HOT const detail::Pointer& _ptr() const noexcept {
            return *_val._ptr();
        }

        value_type _val;
    };

    // The atomic of a handle (req::handle, detail/handle_word.h): a
    // string, an io::file, a net::connection, an async::channel — every
    // public type that is one tracked word to the object inside, whose
    // copies share that object. The atomic is the atomic of that word,
    // the operations of detail::AtomicWord with the handle on the
    // outside. A load is one atomic load with the hazard pointer of every
    // atomic load, and the handle it returns is the object as it was,
    // whatever is stored meanwhile; a store is the store of the object the
    // caller's handle holds; nothing is copied and nothing allocated
    // beyond the objects themselves. The compare-exchanges compare
    // identity, the object, as a compare-exchange on a word does: two
    // strings of the same characters made apart are two objects, and the
    // expected one must be the one loaded (or stored) from here, not one
    // equal to it; an exchange for a change of contents loads, decides,
    // and exchanges against what it loaded. Lives where the handle does:
    // on a stack or inside a managed object.
    // The word's operations are a private base: what the atomic shows
    // takes and gives the handle, never the word (no conversion to it).
    template<req::handle H>
    class atomic<H>
    : private detail::AtomicWord<atomic<H>, detail::HandleStateOf<H>> {
        using State = detail::HandleStateOf<H>;   // the word without its const: the object is never written through the atomic
        using Base = detail::AtomicWord<atomic, State>;
        using Word = tracked_ptr<State>;

    public:
        using value_type = H;
        using Base::is_always_lock_free;
        using Base::is_lock_free;
        using Base::notify_one;
        using Base::notify_all;

        atomic(const atomic&) = delete;
        atomic& operator=(const atomic&) = delete;

        // The handle's own default: what the handle is when made with
        // nothing (an empty string, a file that is none, a channel)
        atomic() noexcept(std::is_nothrow_default_constructible_v<H>) = default;

        SGCL_INLINE_HOT atomic(const H& h) noexcept
        : _val(h) {
        }

        SGCL_INLINE_HOT H load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
            return _handle(Base::load(m));
        }

        SGCL_INLINE_HOT operator H() const noexcept {
            return load();
        }

        SGCL_INLINE_HOT void store(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Base::store(_word(h), m);
        }

        SGCL_INLINE_HOT H operator=(const H& h) noexcept {
            store(h);
            return h;
        }

        // The handle exchanged in, and the one that was there
        SGCL_INLINE_HOT H exchange(const H& h, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            return _handle(Base::exchange(_word(h), m));
        }

        SGCL_INLINE_HOT bool compare_exchange_strong(H& expected, const H& desired, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_strong(e, _word(desired), m);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        SGCL_INLINE_HOT bool compare_exchange_strong(H& expected, const H& desired, const std::memory_order s, const std::memory_order f) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_strong(e, _word(desired), s, f);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        SGCL_INLINE_HOT bool compare_exchange_weak(H& expected, const H& desired, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_weak(e, _word(desired), m);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        SGCL_INLINE_HOT bool compare_exchange_weak(H& expected, const H& desired, const std::memory_order s, const std::memory_order f) noexcept {
            Word e = _word(expected);
            bool done = Base::compare_exchange_weak(e, _word(desired), s, f);
            if (!done) {
                expected = _handle(e);
            }
            return done;
        }

        SGCL_INLINE_HOT void wait(const H& h, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            Base::wait(_word(h), m);
        }

    private:
        friend Base;

        // the word of a handle as the atomic's word, and back
        SGCL_INLINE_HOT static Word _word(const H& h) noexcept {
            return const_pointer_cast<State>(detail::HandleWord::word(h));
        }

        SGCL_INLINE_HOT static H _handle(const Word& w) noexcept {
            return detail::HandleWord::make<H>(detail::HandleWordOf<H>(w));
        }

        SGCL_INLINE_HOT detail::Pointer& _ptr() noexcept {
            return *detail::HandleWord::word(_val)._ptr();
        }

        SGCL_INLINE_HOT const detail::Pointer& _ptr() const noexcept {
            return *detail::HandleWord::word(_val)._ptr();
        }

        H _val;
    };

    // Deduction: the atomic of what the initializer holds
    template<class T>
    atomic(unique_ptr<T>&&) -> atomic<tracked_ptr<T>>;
    template<class T>
    atomic(tracked_ptr<T>) -> atomic<tracked_ptr<T>>;
    template<class T>
    atomic(T) -> atomic<T>;
}
