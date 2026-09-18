//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Atomic<T>: std::atomic for a plain T, the lock-free
// atomic word for a Ptr<T> and a String, with the names of the Sgcl
// interface (Load, Store, Exchange, CompareExchange, Wait, Notify).
// AtomicRef<Ptr<T>>: a Ptr used atomically in place.
#pragma once

#include "../../concurrent/atomic.h"
#include "../../concurrent/atomic_ref.h"
#include "../Core/String.h"
#include "../Core/Ptr.h"

namespace Sgcl {
    using MemoryOrder = std::memory_order;
    inline constexpr MemoryOrder Relaxed = std::memory_order_relaxed;
    inline constexpr MemoryOrder Acquire = std::memory_order_acquire;
    inline constexpr MemoryOrder Release = std::memory_order_release;
    inline constexpr MemoryOrder AcqRel = std::memory_order_acq_rel;
    inline constexpr MemoryOrder SeqCst = std::memory_order_seq_cst;

    // A plain T: std::atomic<T> with Load, Store, the exchanges, the
    // fetches of an integral or a pointer, ++ -- += -=, Wait and Notify
    template<class T>
    class Atomic {
    public:
        using ValueType = T;
        using InnerType = sgcl::atomic<T>;

        constexpr Atomic() noexcept = default;

        constexpr Atomic(T v) noexcept
        : _a(v) {
        }

        Atomic(const Atomic&) = delete;
        Atomic& operator=(const Atomic&) = delete;

        T operator=(T v) noexcept {
            return _a = v;
        }

        operator T() const noexcept {
            return _a.load();
        }

        static constexpr bool IsAlwaysLockFree = std::atomic<T>::is_always_lock_free;

        bool IsLockFree() const noexcept {
            return _a.is_lock_free();
        }

        T Load(MemoryOrder m = SeqCst) const noexcept {
            return _a.load(m);
        }

        void Store(T v, MemoryOrder m = SeqCst) noexcept {
            _a.store(v, m);
        }

        T Exchange(T v, MemoryOrder m = SeqCst) noexcept {
            return _a.exchange(v, m);
        }

        bool CompareExchange(T& expected, T desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_strong(expected, desired, m);
        }

        bool CompareExchange(T& expected, T desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_strong(expected, desired, success, failure);
        }

        bool CompareExchangeWeak(T& expected, T desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_weak(expected, desired, m);
        }

        bool CompareExchangeWeak(T& expected, T desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_weak(expected, desired, success, failure);
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T FetchAdd(std::conditional_t<std::is_pointer_v<U>, ptrdiff_t, U> v, MemoryOrder m = SeqCst) noexcept {
            return _a.fetch_add(v, m);
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T FetchSub(std::conditional_t<std::is_pointer_v<U>, ptrdiff_t, U> v, MemoryOrder m = SeqCst) noexcept {
            return _a.fetch_sub(v, m);
        }

        template<class U = T>
        requires std::is_integral_v<U>
        T FetchAnd(U v, MemoryOrder m = SeqCst) noexcept {
            return _a.fetch_and(v, m);
        }

        template<class U = T>
        requires std::is_integral_v<U>
        T FetchOr(U v, MemoryOrder m = SeqCst) noexcept {
            return _a.fetch_or(v, m);
        }

        template<class U = T>
        requires std::is_integral_v<U>
        T FetchXor(U v, MemoryOrder m = SeqCst) noexcept {
            return _a.fetch_xor(v, m);
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T operator++() noexcept {
            return ++_a;
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T operator++(int) noexcept {
            return _a++;
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T operator--() noexcept {
            return --_a;
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T operator--(int) noexcept {
            return _a--;
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T operator+=(std::conditional_t<std::is_pointer_v<U>, ptrdiff_t, U> v) noexcept {
            return _a += v;
        }

        template<class U = T>
        requires std::is_integral_v<U> || std::is_pointer_v<U>
        T operator-=(std::conditional_t<std::is_pointer_v<U>, ptrdiff_t, U> v) noexcept {
            return _a -= v;
        }

        void Wait(T old, MemoryOrder m = SeqCst) const noexcept {
            _a.wait(old, m);
        }

        void NotifyOne() noexcept {
            _a.notify_one();
        }

        void NotifyAll() noexcept {
            _a.notify_all();
        }

        sgcl::atomic<T>& Inner() noexcept {
            return _a;
        }

        const sgcl::atomic<T>& Inner() const noexcept {
            return _a;
        }

    private:
        sgcl::atomic<T> _a;
    };

    // A Ptr<T> used atomically: the word of the pointer, lock-free, every
    // load a Ptr that holds what it saw
    template<class T>
    class Atomic<Ptr<T>> {
    public:
        using ValueType = Ptr<T>;
        using InnerType = sgcl::atomic<sgcl::tracked_ptr<T>>;

        Atomic() noexcept = default;

        Atomic(std::nullptr_t) noexcept
        : _a(nullptr) {
        }

        Atomic(Ptr<T> p) noexcept
        : _a(std::move(p.Inner())) {
        }

        Atomic(UniquePtr<T>&& u) noexcept
        : _a(std::move(u.Inner())) {
        }

        Atomic(const Atomic&) = delete;
        Atomic& operator=(const Atomic&) = delete;

        std::nullptr_t operator=(std::nullptr_t) noexcept {
            return _a = nullptr;
        }

        Ptr<T> operator=(Ptr<T> p) noexcept {
            return Ptr<T>(_a = std::move(p.Inner()));
        }

        void operator=(UniquePtr<T>&& u) noexcept {
            _a = std::move(u.Inner());
        }

        operator Ptr<T>() const noexcept {
            return Load();
        }

        static constexpr bool IsAlwaysLockFree = InnerType::is_always_lock_free;

        bool IsLockFree() const noexcept {
            return _a.is_lock_free();
        }

        Ptr<T> Load(MemoryOrder m = SeqCst) const noexcept {
            return Ptr<T>(_a.load(m));
        }

        void Store(std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            _a.store(nullptr, m);
        }

        void Store(Ptr<T> p, MemoryOrder m = SeqCst) noexcept {
            _a.store(std::move(p.Inner()), m);
        }

        void Store(UniquePtr<T>&& u, MemoryOrder m = SeqCst) noexcept {
            _a.store(std::move(u.Inner()), m);
        }

        Ptr<T> Exchange(Ptr<T> p, MemoryOrder m = SeqCst) noexcept {
            return Ptr<T>(_a.exchange(std::move(p.Inner()), m));
        }

        Ptr<T> Exchange(std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            return Ptr<T>(_a.exchange(nullptr, m));
        }

        bool CompareExchange(Ptr<T>& expected, Ptr<T> desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), std::move(desired.Inner()), m);
        }

        bool CompareExchange(Ptr<T>& expected, std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), nullptr, m);
        }

        bool CompareExchange(Ptr<T>& expected, Ptr<T> desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), std::move(desired.Inner()), success, failure);
        }

        bool CompareExchangeWeak(Ptr<T>& expected, Ptr<T> desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), std::move(desired.Inner()), m);
        }

        bool CompareExchangeWeak(Ptr<T>& expected, std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), nullptr, m);
        }

        bool CompareExchangeWeak(Ptr<T>& expected, Ptr<T> desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), std::move(desired.Inner()), success, failure);
        }

        void Wait(std::nullptr_t, MemoryOrder m = SeqCst) const noexcept {
            _a.wait(nullptr, m);
        }

        void Wait(Ptr<T> old, MemoryOrder m = SeqCst) const noexcept {
            _a.wait(std::move(old.Inner()), m);
        }

        void NotifyOne() noexcept {
            _a.notify_one();
        }

        void NotifyAll() noexcept {
            _a.notify_all();
        }

        InnerType& Inner() noexcept {
            return _a;
        }

        const InnerType& Inner() const noexcept {
            return _a;
        }

    private:
        InnerType _a;
    };

    // A String used atomically: one word, every load the string as it was
    template<>
    class Atomic<String> {
    public:
        using ValueType = String;
        using InnerType = sgcl::atomic<sgcl::string>;

        Atomic() noexcept = default;

        Atomic(const String& s) noexcept
        : _a(s.Inner()) {
        }

        Atomic(const Atomic&) = delete;
        Atomic& operator=(const Atomic&) = delete;

        String operator=(const String& s) noexcept {
            return String(_a = s.Inner());
        }

        operator String() const noexcept {
            return Load();
        }

        String Load(MemoryOrder m = SeqCst) const noexcept {
            return String(_a.load(m));
        }

        void Store(const String& s, MemoryOrder m = SeqCst) noexcept {
            _a.store(s.Inner(), m);
        }

        String Exchange(const String& s, MemoryOrder m = SeqCst) noexcept {
            return String(_a.exchange(s.Inner(), m));
        }

        bool CompareExchange(String& expected, const String& desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), desired.Inner(), m);
        }

        bool CompareExchange(String& expected, const String& desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), desired.Inner(), success, failure);
        }

        bool CompareExchangeWeak(String& expected, const String& desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), desired.Inner(), m);
        }

        void Wait(const String& old, MemoryOrder m = SeqCst) const noexcept {
            _a.wait(old.Inner(), m);
        }

        void NotifyOne() noexcept {
            _a.notify_one();
        }

        void NotifyAll() noexcept {
            _a.notify_all();
        }

        InnerType& Inner() noexcept {
            return _a;
        }

        const InnerType& Inner() const noexcept {
            return _a;
        }

    private:
        InnerType _a;
    };

    // A Ptr<T> that lives somewhere (a member of a managed object, a
    // RootPtr) used atomically in place: AtomicRef(node->next).Store(p)
    template<class T>
    class AtomicRef;

    template<class T>
    class AtomicRef<Ptr<T>> {
    public:
        using ValueType = Ptr<T>;
        using InnerType = sgcl::atomic_ref<sgcl::tracked_ptr<T>>;

        explicit AtomicRef(Ptr<T>& p) noexcept
        : _a(p.Inner()) {
        }

        explicit AtomicRef(RootPtr<T>& r) noexcept
        : _a(r.Inner().ptr()) {   // the cell's word
        }

        AtomicRef(const AtomicRef&) noexcept = default;
        AtomicRef& operator=(const AtomicRef&) = delete;

        // The Ptr referred to: for a RootPtr the word it holds its object by
        Ptr<T>& Ref() const noexcept {
            return *(Ptr<T>*)&_a.ref;
        }

        std::nullptr_t operator=(std::nullptr_t) noexcept {
            return _a = nullptr;
        }

        Ptr<T> operator=(Ptr<T> p) noexcept {
            return Ptr<T>(_a = std::move(p.Inner()));
        }

        void operator=(UniquePtr<T>&& u) noexcept {
            _a = std::move(u.Inner());
        }

        operator Ptr<T>() const noexcept {
            return Load();
        }

        static constexpr bool IsAlwaysLockFree = InnerType::is_always_lock_free;
        static constexpr size_t RequiredAlignment = InnerType::required_alignment;

        bool IsLockFree() const noexcept {
            return _a.is_lock_free();
        }

        Ptr<T> Load(MemoryOrder m = SeqCst) const noexcept {
            return Ptr<T>(_a.load(m));
        }

        void Store(std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            _a.store(nullptr, m);
        }

        void Store(Ptr<T> p, MemoryOrder m = SeqCst) noexcept {
            _a.store(std::move(p.Inner()), m);
        }

        void Store(UniquePtr<T>&& u, MemoryOrder m = SeqCst) noexcept {
            _a.store(std::move(u.Inner()), m);
        }

        Ptr<T> Exchange(Ptr<T> p, MemoryOrder m = SeqCst) noexcept {
            return Ptr<T>(_a.exchange(std::move(p.Inner()), m));
        }

        Ptr<T> Exchange(std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            return Ptr<T>(_a.exchange(nullptr, m));
        }

        bool CompareExchange(Ptr<T>& expected, Ptr<T> desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), std::move(desired.Inner()), m);
        }

        bool CompareExchange(Ptr<T>& expected, std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), nullptr, m);
        }

        bool CompareExchange(Ptr<T>& expected, Ptr<T> desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_strong(expected.Inner(), std::move(desired.Inner()), success, failure);
        }

        bool CompareExchangeWeak(Ptr<T>& expected, Ptr<T> desired, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), std::move(desired.Inner()), m);
        }

        bool CompareExchangeWeak(Ptr<T>& expected, std::nullptr_t, MemoryOrder m = SeqCst) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), nullptr, m);
        }

        bool CompareExchangeWeak(Ptr<T>& expected, Ptr<T> desired, MemoryOrder success, MemoryOrder failure) noexcept {
            return _a.compare_exchange_weak(expected.Inner(), std::move(desired.Inner()), success, failure);
        }

        void Wait(std::nullptr_t, MemoryOrder m = SeqCst) const noexcept {
            _a.wait(nullptr, m);
        }

        void Wait(Ptr<T> old, MemoryOrder m = SeqCst) const noexcept {
            _a.wait(std::move(old.Inner()), m);
        }

        void NotifyOne() noexcept {
            _a.notify_one();
        }

        void NotifyAll() noexcept {
            _a.notify_all();
        }

        InnerType& Inner() noexcept {
            return _a;
        }

        const InnerType& Inner() const noexcept {
            return _a;
        }

    private:
        InnerType _a;
    };

    template<class T> AtomicRef(Ptr<T>&) -> AtomicRef<Ptr<T>>;
    template<class T> AtomicRef(RootPtr<T>&) -> AtomicRef<Ptr<T>>;

    // Deduction: the Atomic of what the initializer holds
    template<class T> Atomic(UniquePtr<T>&&) -> Atomic<Ptr<T>>;
    template<class T> Atomic(Ptr<T>) -> Atomic<Ptr<T>>;
    Atomic(String) -> Atomic<String>;
    template<class T> Atomic(T) -> Atomic<T>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

