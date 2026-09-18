//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/string.h"
#include "../core/tracked_ptr.h"
#include "detail/atomic_word.h"

namespace sgcl {
    // sgcl::atomic<T> is std::atomic<T> for every T but the ones below: the
    // tracked pointers and the strings, whose atomics are the library's
    // own, over the word each of them is. So a program names one atomic
    // for its flags, its counters and its pointers alike.
    template<class T>
    class atomic : public std::atomic<T> {
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
    class atomic<tracked_ptr<T>> : public detail::AtomicWord<atomic<tracked_ptr<T>>, T> {
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
        friend detail::AtomicWord<atomic, T>;

        detail::Pointer& _ptr() noexcept {
            return *_val._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *_val._ptr();
        }

        value_type _val;
    };

    // The atomic of a string: a string is one word to an object never
    // modified, so an atomic string is the atomic of that word, the
    // operations of detail::AtomicWord with a string on the outside. A
    // load is one atomic load with the hazard pointer of every atomic
    // load, and the string it returns is the object as it was, whatever
    // is stored meanwhile; a store is the store of the object the caller
    // has already made; nothing is copied and nothing allocated beyond
    // the strings themselves. The compare-exchanges compare identity, the
    // object, as a compare-exchange on a word does: two strings of the
    // same characters made apart are two objects, and the expected one
    // must be the one loaded (or stored) from here, not one equal to it;
    // an exchange for a change of contents loads, decides, and exchanges
    // against what it loaded. Lives where the string does: on a stack or
    // inside a managed object.
    template<class CharT, class Traits>
    class atomic<basic_string<CharT, Traits>> : public detail::AtomicWord<atomic<basic_string<CharT, Traits>>, void> {
        using Base = detail::AtomicWord<atomic, void>;   // the word without its const: the string's object is never written through it
        using String = basic_string<CharT, Traits>;

    public:
        using value_type = String;

        atomic(const atomic&) = delete;
        atomic& operator=(const atomic&) = delete;

        atomic() noexcept = default;

        atomic(const String& s) noexcept
        : _val(s) {
        }

        String load(const std::memory_order m = std::memory_order_seq_cst) const noexcept {
            return String(Base::load(m));
        }

        operator String() const noexcept {
            return load();
        }

        void store(const String& s, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            Base::store(_word(s), m);
        }

        String operator=(const String& s) noexcept {
            store(s);
            return s;
        }

        // The string exchanged in, and the one that was there
        String exchange(const String& s, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            String old = load(std::memory_order_acquire);
            while (!compare_exchange_weak(old, s, m, std::memory_order_acquire)) {
            }
            return old;
        }

        bool compare_exchange_strong(String& expected, const String& desired, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            tracked_ptr<void> e = _word(expected);
            bool done = Base::compare_exchange_strong(e, _word(desired), m);
            if (!done) {
                expected = String(e);
            }
            return done;
        }

        bool compare_exchange_strong(String& expected, const String& desired, const std::memory_order s, const std::memory_order f) noexcept {
            tracked_ptr<void> e = _word(expected);
            bool done = Base::compare_exchange_strong(e, _word(desired), s, f);
            if (!done) {
                expected = String(e);
            }
            return done;
        }

        bool compare_exchange_weak(String& expected, const String& desired, const std::memory_order m = std::memory_order_seq_cst) noexcept {
            tracked_ptr<void> e = _word(expected);
            bool done = Base::compare_exchange_weak(e, _word(desired), m);
            if (!done) {
                expected = String(e);
            }
            return done;
        }

        bool compare_exchange_weak(String& expected, const String& desired, const std::memory_order s, const std::memory_order f) noexcept {
            tracked_ptr<void> e = _word(expected);
            bool done = Base::compare_exchange_weak(e, _word(desired), s, f);
            if (!done) {
                expected = String(e);
            }
            return done;
        }

        void wait(const String& s, std::memory_order m = std::memory_order_seq_cst) const noexcept {
            Base::wait(_word(s), m);
        }

    private:
        friend Base;

        // the word of a string as a tracked_ptr<void>
        static tracked_ptr<void> _word(const String& s) noexcept {
            return const_pointer_cast<void>(tracked_ptr<const void>(s._word));
        }

        detail::Pointer& _ptr() noexcept {
            return *static_cast<tracked_ptr<const void>&>(_val._word)._ptr();
        }

        const detail::Pointer& _ptr() const noexcept {
            return *static_cast<const tracked_ptr<const void>&>(_val._word)._ptr();
        }

        String _val;
    };

    // Deduction: the atomic of what the initializer holds
    template<class T>
    atomic(unique_ptr<T>&&) -> atomic<tracked_ptr<T>>;
    template<class T>
    atomic(tracked_ptr<T>) -> atomic<tracked_ptr<T>>;
    template<class CharT, class Traits>
    atomic(basic_string<CharT, Traits>) -> atomic<basic_string<CharT, Traits>>;
    template<class T>
    atomic(T) -> atomic<T>;
}
