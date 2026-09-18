//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The pointers of the Sgcl interface: Ptr over tracked_ptr, UniquePtr
// over unique_ptr, RootPtr over root_ptr, WeakPtr over weak_ptr, and
// Make, which builds an object and hands it back as a UniquePtr. Each is a value
// with the one pointer inside (composition), every method a forward: the
// rules of the pointer inside are the rules of the wrapper (README, "The
// rules"), and so is the cost.
#pragma once

#include "../../core/detail/pointer_word.h"
#include "../../core/detail/tracked.h"
#include "../../core/make_tracked.h"
#include "../../core/root_ptr.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"
#include "../../core/weak_ptr.h"

#include <compare>
#include <memory>
#include <ostream>

namespace Sgcl {
    template<class T> class Ptr;
    template<class T> class UniquePtr;
    template<class T> class RootPtr;
    template<class T> class WeakPtr;

    // A pointer to a managed object, traced: the object lives while a Ptr
    // refers to it. One word. Lives on a stack or in a managed object,
    // never in unmanaged memory (rule 1); RootPtr is the one for the
    // latter. A tracked pointer word to the containers and the variant
    // (IsTrackedPointer, below): a buffer of Ptrs is zeroed rather than
    // constructed and moved as words, several Ptr alternatives share one
    // word, as for the word inside
    template<class T>
    class Ptr {
    public:
        using ElementType = T;
        using InnerType = sgcl::tracked_ptr<T>;

        constexpr Ptr() noexcept = default;

        constexpr Ptr(std::nullptr_t) noexcept
        : _p(nullptr) {
        }

        // From a raw pointer to a managed object (the checks of tracked_ptr)
        template<class U>
        requires std::is_convertible_v<U*, T*>
        explicit Ptr(U* p) noexcept
        : _p(p) {
        }

        Ptr(const Ptr&) noexcept = default;
        Ptr(Ptr&&) noexcept = default;

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr(const Ptr<U>& p) noexcept
        : _p(p.Inner()) {
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr(Ptr<U>&& p) noexcept
        : _p(std::move(p.Inner())) {
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr(UniquePtr<U>&& u) noexcept
        : _p(std::move(u.Inner())) {
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr(const RootPtr<U>& r) noexcept
        : _p(r.Inner().ptr()) {
        }

        explicit Ptr(sgcl::tracked_ptr<T> p) noexcept
        : _p(std::move(p)) {
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        explicit Ptr(sgcl::unique_ptr<U>&& u) noexcept
        : _p(std::move(u)) {
        }

        Ptr& operator=(const Ptr&) noexcept = default;
        Ptr& operator=(Ptr&&) noexcept = default;

        Ptr& operator=(std::nullptr_t) noexcept {
            _p = nullptr;
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr& operator=(const Ptr<U>& p) noexcept {
            _p = p.Inner();
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr& operator=(Ptr<U>&& p) noexcept {
            _p = std::move(p.Inner());
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        Ptr& operator=(UniquePtr<U>&& u) noexcept {
            _p = std::move(u.Inner());
            return *this;
        }

        T* Get() const noexcept {
            return _p.get();
        }

        void Reset() noexcept {
            _p.reset();
        }

        void Reset(T* p) noexcept {
            _p.reset(p);
        }

        void Swap(Ptr& p) noexcept {
            _p.swap(p._p);
        }

        // The pointer if its object is alive, null if it is one the
        // collector has found unreachable (a weak pointer's use)
        Ptr IfAlive() const noexcept {
            return Ptr(_p.if_alive());
        }

        // The dynamic type of the object: Is<U>() whether it is a U,
        // As<U>() the pointer as a Ptr<U> (null when it is not one)
        template<class U>
        bool Is() const noexcept {
            return _p.template is<U>();
        }

        template<class U>
        Ptr<U> As() const noexcept {
            return Ptr<U>(_p.template as<U>());
        }

        const std::type_info& Type() const noexcept {
            return _p.type();
        }

        template<class U = T>
        requires (!std::is_void_v<U>)
        U& operator*() const noexcept {
            return *_p;
        }

        template<class U = T>
        requires (!std::is_void_v<U>)
        U* operator->() const noexcept {
            return _p.operator->();
        }

        explicit operator bool() const noexcept {
            return (bool)_p;
        }

        // The same word seen without its type: every Ptr<T> is a Ptr<void>&
        operator Ptr<void>&() noexcept {
            return *(Ptr<void>*)(this);
        }

        operator const Ptr<void>&() const noexcept {
            return *(const Ptr<void>*)(this);
        }

        // The object held from unmanaged memory: a shared_ptr whose control
        // block owns a managed holder of this pointer (a root); the object
        // stays managed and lives on after the last shared_ptr if anything
        // else reaches it. Two allocations: a named function, not a conversion
        std::shared_ptr<T> ToShared() const {
            return _p.to_shared();
        }

        sgcl::tracked_ptr<T>& Inner() noexcept {
            return _p;
        }

        const sgcl::tracked_ptr<T>& Inner() const noexcept {
            return _p;
        }

    private:
        sgcl::tracked_ptr<T> _p;
    };

    template<class T> Ptr(T*) -> Ptr<T>;
    template<class T> Ptr(sgcl::tracked_ptr<T>) -> Ptr<T>;
    template<class T> Ptr(UniquePtr<T>&&) -> Ptr<T>;
    template<class T> Ptr(const RootPtr<T>&) -> Ptr<T>;

    template<class T, class U>
    bool operator==(const Ptr<T>& l, const Ptr<U>& r) noexcept {
        return l.Inner() == r.Inner();
    }

    template<class T, class U>
    std::strong_ordering operator<=>(const Ptr<T>& l, const Ptr<U>& r) noexcept {
        return l.Inner() <=> r.Inner();
    }

    template<class T>
    bool operator==(const Ptr<T>& l, std::nullptr_t) noexcept {
        return !l;
    }

    template<class T>
    std::strong_ordering operator<=>(const Ptr<T>& l, std::nullptr_t) noexcept {
        return l.Inner() <=> nullptr;
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const Ptr<T>& p) {
        return s << p.Inner();
    }

    template<class T>
    void swap(Ptr<T>& l, Ptr<T>& r) noexcept {
        l.Swap(r);
    }

    template<class T, class U>
    Ptr<T> StaticCast(const Ptr<U>& p) noexcept {
        return Ptr<T>(sgcl::static_pointer_cast<T>(p.Inner()));
    }

    template<class T, class U>
    Ptr<T> ConstCast(const Ptr<U>& p) noexcept {
        return Ptr<T>(sgcl::const_pointer_cast<T>(p.Inner()));
    }

    template<class T, class U>
    Ptr<T> DynamicCast(const Ptr<U>& p) noexcept {
        return Ptr<T>(sgcl::dynamic_pointer_cast<T>(p.Inner()));
    }

    // The sole owner of a managed object, until it is handed to a Ptr:
    // what Make gives back before anyone shares the object. Movable, not
    // copyable; lives anywhere.
    template<class T>
    class UniquePtr {
    public:
        using ElementType = T;
        using InnerType = sgcl::unique_ptr<T>;

        constexpr UniquePtr() noexcept = default;

        constexpr UniquePtr(std::nullptr_t) noexcept
        : _p(nullptr) {
        }

        UniquePtr(UniquePtr&&) noexcept = default;
        UniquePtr(const UniquePtr&) = delete;

        template<class U>
        requires std::is_convertible_v<U*, T*>
        UniquePtr(UniquePtr<U>&& u) noexcept
        : _p(std::move(u.Inner())) {
        }

        explicit UniquePtr(sgcl::unique_ptr<T>&& u) noexcept
        : _p(std::move(u)) {
        }

        UniquePtr& operator=(UniquePtr&&) noexcept = default;
        UniquePtr& operator=(const UniquePtr&) = delete;

        UniquePtr& operator=(std::nullptr_t) noexcept {
            _p = nullptr;
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        UniquePtr& operator=(UniquePtr<U>&& u) noexcept {
            _p = std::move(u.Inner());
            return *this;
        }

        T* Get() const noexcept {
            return _p.get();
        }

        // Gives the object up without destroying it: the raw pointer, for
        // a Ptr to take (Ptr(p.Release()))
        T* Release() noexcept {
            return _p.release();
        }

        void Reset() noexcept {
            _p.reset();
        }

        // A managed object nothing owns taken over (one from Release())
        void Reset(T* p) noexcept {
            _p.reset(p);
        }

        void Swap(UniquePtr& u) noexcept {
            _p.swap(u._p);
        }

        template<class U>
        bool Is() const noexcept {
            return _p.template is<U>();
        }

        template<class U>
        UniquePtr<U> As() noexcept {
            return UniquePtr<U>(_p.template as<U>());
        }

        const std::type_info& Type() const noexcept {
            return _p.type();
        }

        template<class U = T>
        requires (!std::is_void_v<U>)
        U& operator*() const {
            return *_p;
        }

        template<class U = T>
        requires (!std::is_void_v<U>)
        U* operator->() const noexcept {
            return _p.operator->();
        }

        explicit operator bool() const noexcept {
            return (bool)_p;
        }

        // The same owner seen without its type
        operator UniquePtr<void>&() noexcept {
            return *(UniquePtr<void>*)(this);
        }

        operator const UniquePtr<void>&() const noexcept {
            return *(const UniquePtr<void>*)(this);
        }

        sgcl::unique_ptr<T>& Inner() noexcept {
            return _p;
        }

        const sgcl::unique_ptr<T>& Inner() const noexcept {
            return _p;
        }

    private:
        sgcl::unique_ptr<T> _p;
    };

    template<class T, class U>
    bool operator==(const UniquePtr<T>& l, const UniquePtr<U>& r) noexcept {
        return l.Get() == r.Get();
    }

    template<class T, class U>
    std::strong_ordering operator<=>(const UniquePtr<T>& l, const UniquePtr<U>& r) noexcept {
        return std::compare_three_way()(static_cast<const void*>(l.Get()), static_cast<const void*>(r.Get()));
    }

    template<class T>
    bool operator==(const UniquePtr<T>& l, std::nullptr_t) noexcept {
        return !l;
    }

    template<class T>
    std::strong_ordering operator<=>(const UniquePtr<T>& l, std::nullptr_t) noexcept {
        return std::compare_three_way()(static_cast<const void*>(l.Get()), static_cast<const void*>(nullptr));
    }

    template<class T>
    void swap(UniquePtr<T>& l, UniquePtr<T>& r) noexcept {
        l.Swap(r);
    }

    // The casts on an rvalue: the object released from `u` and owned by
    // the result; a failed dynamic_cast loses the object (test with Is<U>()
    // or As<U>() first when the type is in doubt)
    template<class T, class U>
    UniquePtr<T> StaticCast(UniquePtr<U>&& u) noexcept {
        return UniquePtr<T>(sgcl::static_pointer_cast<T>(std::move(u.Inner())));
    }

    template<class T, class U>
    UniquePtr<T> ConstCast(UniquePtr<U>&& u) noexcept {
        return UniquePtr<T>(sgcl::const_pointer_cast<T>(std::move(u.Inner())));
    }

    template<class T, class U>
    UniquePtr<T> DynamicCast(UniquePtr<U>&& u) noexcept {
        return UniquePtr<T>(sgcl::dynamic_pointer_cast<T>(std::move(u.Inner())));
    }

    // A pointer that lives anywhere: a global, a std container, malloc
    // memory, a plain coroutine frame. The Ptr is kept one step away, in a
    // cell of the thread's block (root_ptr.h); a cell per RootPtr.
    template<class T>
    class RootPtr {
    public:
        using ElementType = T;
        using InnerType = sgcl::root_ptr<T>;

        RootPtr() noexcept = default;

        RootPtr(std::nullptr_t) noexcept
        : _p(nullptr) {
        }

        RootPtr(const RootPtr&) noexcept = default;
        RootPtr(RootPtr&&) noexcept = default;

        template<class U>
        requires std::is_convertible_v<U*, T*>
        RootPtr(const Ptr<U>& p) noexcept
        : _p(p.Inner()) {
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        RootPtr(UniquePtr<U>&& u) noexcept
        : _p(std::move(u.Inner())) {
        }

        explicit RootPtr(sgcl::root_ptr<T> p) noexcept
        : _p(std::move(p)) {
        }

        RootPtr& operator=(const RootPtr&) noexcept = default;
        RootPtr& operator=(RootPtr&&) noexcept = default;

        RootPtr& operator=(std::nullptr_t) noexcept {
            _p = nullptr;
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        RootPtr& operator=(const Ptr<U>& p) noexcept {
            _p = p.Inner();
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        RootPtr& operator=(UniquePtr<U>&& u) noexcept {
            _p = std::move(u.Inner());
            return *this;
        }

        T* Get() const noexcept {
            return _p.get();
        }

        // The Ptr the root holds its object by, by reference: the cell's
        // word, seen as a Ptr (one word, the same layout)
        Ptr<T>& GetPtr() noexcept {
            return *(Ptr<T>*)&_p.ptr();
        }

        const Ptr<T>& GetPtr() const noexcept {
            return *(const Ptr<T>*)&_p.ptr();
        }

        operator Ptr<T>&() noexcept {
            return GetPtr();
        }

        operator const Ptr<T>&() const noexcept {
            return GetPtr();
        }

        void Reset() noexcept {
            _p.reset();
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        void Reset(const Ptr<U>& p) noexcept {
            _p.reset(p.Inner());
        }

        void Swap(RootPtr& r) noexcept {
            _p.swap(r._p);
        }

        template<class U>
        bool Is() const noexcept {
            return _p.template is<U>();
        }

        template<class U>
        Ptr<U> As() const noexcept {
            return Ptr<U>(_p.template as<U>());
        }

        const std::type_info& Type() const noexcept {
            return _p.type();
        }

        T& operator*() const noexcept {
            return *_p;
        }

        T* operator->() const noexcept {
            return _p.operator->();
        }

        explicit operator bool() const noexcept {
            return (bool)_p;
        }

        sgcl::root_ptr<T>& Inner() noexcept {
            return _p;
        }

        const sgcl::root_ptr<T>& Inner() const noexcept {
            return _p;
        }

    private:
        sgcl::root_ptr<T> _p;
    };

    template<class T> RootPtr(const Ptr<T>&) -> RootPtr<T>;
    template<class T> RootPtr(UniquePtr<T>&&) -> RootPtr<T>;

    template<class T, class U>
    bool operator==(const RootPtr<T>& l, const RootPtr<U>& r) noexcept {
        return l.Get() == r.Get();
    }

    template<class T, class U>
    bool operator==(const RootPtr<T>& l, const Ptr<U>& r) noexcept {
        return l.Get() == r.Get();
    }

    template<class T>
    bool operator==(const RootPtr<T>& l, std::nullptr_t) noexcept {
        return !l;
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const RootPtr<T>& p) {
        return s << p.Get();
    }

    template<class T>
    void swap(RootPtr<T>& l, RootPtr<T>& r) noexcept {
        l.Swap(r);
    }

    // A pointer that keeps nothing alive: Lock() gives the Ptr while the
    // object lives, null once the collector has found it unreachable
    template<class T>
    class WeakPtr {
    public:
        using ElementType = T;
        using InnerType = sgcl::weak_ptr<T>;

        constexpr WeakPtr() noexcept = default;

        constexpr WeakPtr(std::nullptr_t) noexcept
        : _p(nullptr) {
        }

        WeakPtr(const WeakPtr&) noexcept = default;
        WeakPtr(WeakPtr&&) noexcept = default;

        template<class U>
        requires std::is_convertible_v<U*, T*>
        WeakPtr(const WeakPtr<U>& w) noexcept
        : _p(w.Inner()) {
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        WeakPtr(const Ptr<U>& p)
        : _p(p.Inner()) {
        }

        explicit WeakPtr(sgcl::weak_ptr<T> w) noexcept
        : _p(std::move(w)) {
        }

        WeakPtr& operator=(const WeakPtr&) noexcept = default;
        WeakPtr& operator=(WeakPtr&&) noexcept = default;

        template<class U>
        requires std::is_convertible_v<U*, T*>
        WeakPtr& operator=(const WeakPtr<U>& w) noexcept {
            _p = w.Inner();
            return *this;
        }

        template<class U>
        requires std::is_convertible_v<U*, T*>
        WeakPtr& operator=(const Ptr<U>& p) {
            _p = p.Inner();
            return *this;
        }

        WeakPtr& operator=(std::nullptr_t) noexcept {
            _p = nullptr;
            return *this;
        }

        Ptr<T> Lock() const noexcept {
            return Ptr<T>(_p.lock());
        }

        bool IsExpired() const noexcept {
            return _p.expired();
        }

        void Reset() noexcept {
            _p.reset();
        }

        void Swap(WeakPtr& w) noexcept {
            _p.swap(w._p);
        }

        sgcl::weak_ptr<T>& Inner() noexcept {
            return _p;
        }

        const sgcl::weak_ptr<T>& Inner() const noexcept {
            return _p;
        }

    private:
        sgcl::weak_ptr<T> _p;
    };

    template<class T> WeakPtr(const Ptr<T>&) -> WeakPtr<T>;
}

namespace sgcl::detail {
    // A Ptr is a tracked pointer word (tracked.h), a WeakPtr a pointer word
    // (pointer_word.h): the alternative of a variant, the value of an any,
    // in the word kept apart from data; the element of a buffer moved as
    // a word
    template<class T>
    inline constexpr bool IsTrackedPointer<Sgcl::Ptr<T>> = true;

    template<class T>
    struct IsWeakPtr<Sgcl::WeakPtr<T>> : std::true_type {};
}

namespace Sgcl {

    template<class T>
    void swap(WeakPtr<T>& l, WeakPtr<T>& r) noexcept {
        l.Swap(r);
    }

    // The object built on the managed heap: a UniquePtr, the sole owner
    // until a Ptr, a RootPtr or an Atomic takes it over, which any of
    // them does from the expression itself: `Ptr p = Make<Node>(1);`,
    // `RootPtr r = Make<Node>();`, `list.Add(Make<Node>());`
    template<class T, class... A>
    UniquePtr<T> Make(A&&... a) {
        return UniquePtr<T>(sgcl::make_tracked<T>(std::forward<A>(a)...));
    }
}

template<class T>
struct std::hash<Sgcl::Ptr<T>> {
    size_t operator()(const Sgcl::Ptr<T>& p) const noexcept {
        return std::hash<T*>()(p.Get());
    }
};

template<class T>
struct std::hash<Sgcl::UniquePtr<T>> {
    size_t operator()(const Sgcl::UniquePtr<T>& p) const noexcept {
        return std::hash<T*>()(p.Get());
    }
};

template<class T>
struct std::hash<Sgcl::RootPtr<T>> {
    size_t operator()(const Sgcl::RootPtr<T>& p) const noexcept {
        return std::hash<T*>()(p.Get());
    }
};

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

