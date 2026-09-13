//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer.h"
#include "detail/tracked.h"
#include "make_tracked.h"
#include "unique_ptr.h"
#include "types.h"

namespace sgcl {
    namespace detail {
        // Tag of the callers that hold a reference to current_thread():
        // the thread is registered, the constructor need not check.
        struct OnRegisteredThread {};
    }

    // One word: the pointer itself. A tracked_ptr lives either inside a
    // managed object (the collector finds it through the type's pointer
    // map, which it builds itself) or on a thread's stack (the collector
    // scans the stacks); nowhere else. Construction is a check of the
    // thread-registration flag (one thread-local load; skipping it for
    // members of managed objects with a heap range check measured slower
    // than the load), a store of the value with the barrier, and an escape
    // of the address: an atomic whose address never escapes may otherwise
    // be kept in a register, where no scan can see it. The registration
    // comes first: a thread registered before the store has its stack
    // scanned after it, or does the barrier after the cycle's demotion, so
    // either the word or the state is seen (_registered).
    template<class T>
    class tracked_ptr : detail::Tracked {
    public:
        using element_type = T;

        tracked_ptr() noexcept
        : _raw_ptr(_registered((element_type*)nullptr)) {
            _init();
        }

        tracked_ptr(std::nullptr_t) noexcept
        : tracked_ptr() {
        }

        // From a raw pointer: into a managed object (the object, a base
        // subobject, a member), never into a buffer of a container, whose
        // elements no tracked_ptr may address (README, "Pointer aliases"),
        // and never into an object a unique_ptr owns.
        // The constructors with a value store it once, with the barrier,
        // and never write a null first (detail::Pointer).
        template<class U, std::enable_if_t<std::is_convertible_v<U*, element_type*>, int> = 0>
        explicit tracked_ptr(U* p) noexcept
        : _raw_ptr(_registered(static_cast<element_type*>(p))) {
            _init();
            assert((!p || detail::Page::is_object(p)) && "a tracked_ptr may address a managed object or a part of it, not an element of a container's buffer");
            assert(!p || !detail::Page::is_unique(p));
        }

        tracked_ptr(const tracked_ptr& p) noexcept
        : _raw_ptr(_registered(p.get())) {
            _init();
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(const tracked_ptr<U>& p) noexcept
        : _raw_ptr(_registered(static_cast<element_type*>(p.get()))) {
            _init();
        }

        tracked_ptr(tracked_ptr&& p) noexcept
        : _raw_ptr(_registered(p.get())) {
            _init();
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(tracked_ptr<U>&& p) noexcept
        : _raw_ptr(_registered(static_cast<element_type*>(p.get()))) {
            _init();
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr(unique_ptr<U>&& u) noexcept
        : _raw_ptr(_registered(static_cast<element_type*>(u.release()))) {
            _init();
        }

        // Clears the word: a dead slot or stack word does not keep its
        // target alive. A plain store hidden from the thread sanitizer: the
        // destructor of an object the collector reclaims runs on its thread,
        // ordered after the object's last use by the cycle that found it
        // unreferenced, which the sanitizer cannot see (os::load_word does
        // the same for the collector's reads).
        SGCL_NO_SANITIZE ~tracked_ptr() noexcept {
            *(void* volatile*)&_raw_ptr = nullptr;
        }

        tracked_ptr& operator=(std::nullptr_t) noexcept {
            _ptr()->store(nullptr);
            return *this;
        }

        tracked_ptr& operator=(const tracked_ptr& p) noexcept {
            _ptr()->store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(const tracked_ptr<U>& p) noexcept {
            _ptr()->store(static_cast<element_type*>(p.get()));
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename unique_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(unique_ptr<U>&& u) noexcept {
            auto p = u.release();
            _ptr()->store(static_cast<element_type*>(p));
            return *this;
        }

        tracked_ptr& operator=(tracked_ptr&& p) noexcept {
            _ptr()->store(p.get());
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        tracked_ptr& operator=(tracked_ptr<U>&& p) noexcept {
            _ptr()->store(static_cast<element_type*>(p.get()));
            return *this;
        }

        operator tracked_ptr<void>&() noexcept {
            return *(tracked_ptr<void>*)(this);
        }

        operator const tracked_ptr<void>&() const noexcept {
            return *(const tracked_ptr<void>*)(this);
        }

        explicit operator bool() const noexcept {
            return (get() != nullptr);
        }

        template <class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U& operator*() const noexcept {
            assert(get() != nullptr);
            return *get();
        }

        template <class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
        U* operator->() const noexcept {
            assert(get() != nullptr);
            return get();
        }

        element_type* get() const noexcept {
            return (element_type*)_ptr()->load();
        }

        // For destructors: a copy of the pointer, or null when its target
        // dies in the same sweep as the object being destroyed (README,
        // "Pointer maps": a destructor may not touch such a peer, it may be
        // gone already). Elsewhere a plain copy: outside a sweep a
        // tracked_ptr in a live object points at a live object. The copy
        // of a live target is safe in the middle of a sweep too: the
        // barrier's mark lands on a marked object, which the sweep leaves
        // alone, and the next cycle demotes it as usual.
        tracked_ptr if_alive() const noexcept {
            auto p = get();
            if (p && detail::sweeping && detail::Page::dying(p)) {
                return tracked_ptr();
            }
            return *this;
        }

        void reset() noexcept {
            _ptr()->store(nullptr);
        }

        // The pointer replaced by a raw one, under the rules of the raw
        // constructor (a managed object or a part of it): one store with
        // its barrier, no temporary. What the containers use to relink
        // nodes the container roots.
        void reset(element_type* p) noexcept {
            assert((!p || detail::Page::is_object(p)) && "a tracked_ptr may address a managed object or a part of it, not an element of a container's buffer");
            assert(!p || !detail::Page::is_unique(p));
            _ptr()->store(p);
        }

        void swap(tracked_ptr& p) noexcept {
            tracked_ptr<element_type> t = *this;
            *this = p;
            p = t;
        }

        template<class U>
        bool is() const noexcept {
            return type() == typeid(U);
        }

        template<class U>
        tracked_ptr<U> as() const noexcept {
            if (is<U>()) {
                return tracked_ptr<U>((typename tracked_ptr<U>::element_type*)_ptr()->data_base_address());
            } else {
                return {nullptr};
            }
        }

        const std::type_info& type() const noexcept {
            return _ptr()->template type_info<element_type>();
        }

    protected:
        detail::Pointer* _ptr() noexcept {
            return &_raw_ptr;
        }

        const detail::Pointer* _ptr() const noexcept {
            return &_raw_ptr;
        }

        detail::Pointer _raw_ptr;

    private:
        // From a raw pointer on a thread known to be registered (the
        // atomics, right after current_thread()): the null store and the
        // escape as in every constructor, the thread-local check skipped.
        // On Darwin that check is a call into the dynamic loader per
        // construction, which costs the atomics a third of their time.
        tracked_ptr(element_type* p, detail::OnRegisteredThread) noexcept
        : _raw_ptr(p) {
            detail::os::escape(this);
            assert(detail::thread_registered);
            assert(!p || detail::Page::is_object(p));
        }

        // Before the word is stored, in every constructor: the thread is
        // registered. The write barrier skips its store when the target's
        // state is Reachable already, and a cycle's first round demotes
        // that state; a thread that copied a pointer onto its stack before
        // registering, and registered after the cycle's second round, had
        // neither its stack scanned in that cycle nor a state left on the
        // target, and lost the object (DESIGN, the stack-root race). With
        // the registration first, a thread registered before the second
        // round has its stack scanned after the store, and one registered
        // later does the barrier after the demotion, so its store stands.
        static element_type* _registered(element_type* p) noexcept {
            detail::ensure_thread_registered();
            return p;
        }

        // Every constructor, after the store: the address escapes (an
        // atomic that never escapes may live in a register only, invisible
        // to the stack scan), the location is a legal one.
        void _init() noexcept {
            detail::os::escape(this);
            assert((detail::Heap::contains(this) || detail::current_thread().on_stack(this)) && "a tracked_ptr must live on the stack or inside a managed object");
        }

        // The pointer read without an atomic load (detail::Pointer::load_plain):
        // for the containers, on the pointer to their own buffer, which no
        // other thread writes.
        element_type* get_plain() const noexcept {
            return (element_type*)_ptr()->load_plain();
        }

        template<class> friend class atomic;
        template<class> friend class atomic_ref;
        template<class> friend class tracked_ptr;
        template<class> friend class vector;
        template<class> friend class weak_ptr;
        template<class, size_t> friend struct array;
        template<class> friend class detail::Maker;
    };

    // Arrays are not a public type: sgcl::vector and the other containers own
    // them and create them through detail::Maker<T[]>.
    template<class T>
    class tracked_ptr<T[]>;

    template <typename T>
    tracked_ptr(T*) -> tracked_ptr<T>;

    template <typename T>
    tracked_ptr(tracked_ptr<T>) -> tracked_ptr<T>;

    template <typename T>
    tracked_ptr(unique_ptr<T>&&) -> tracked_ptr<T>;

    template<class T, class U>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        using Y = typename std::common_type<decltype(l.get()), decltype(r.get())>::type;
        return static_cast<Y>(l.get()) <=> static_cast<Y>(r.get());
    }

    template<class T, class U>
    inline bool operator==(const tracked_ptr<T>& l, const tracked_ptr<U>& r) noexcept {
        return static_cast<const void*>(l.get()) == static_cast<const void*>(r.get());
    }

    template<class T>
    inline std::strong_ordering operator<=>(const tracked_ptr<T>& l, std::nullptr_t) noexcept {
        return l.get() <=> static_cast<decltype(l.get())>(nullptr);
    }

    template<class T>
    inline bool operator==(const tracked_ptr<T>& l, std::nullptr_t) noexcept {
        return l.get() == nullptr;
    }

    template<class T>
    inline std::strong_ordering operator<=>(std::nullptr_t, const tracked_ptr<T>& r) noexcept {
        return static_cast<decltype(r.get())>(nullptr) <=> r.get();
    }

    template<class T>
    inline bool operator==(std::nullptr_t, const tracked_ptr<T>& r) noexcept {
        return r.get() == nullptr;
    }

    template<class T, class U>
    inline tracked_ptr<T> static_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(static_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T, class U>
    inline tracked_ptr<T> const_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(const_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T, class U>
    inline tracked_ptr<T> dynamic_pointer_cast(const tracked_ptr<U>& p) noexcept {
        return tracked_ptr<T>(dynamic_cast<typename tracked_ptr<T>::element_type*>(p.get()));
    }

    template<class T>
    std::ostream& operator<<(std::ostream& s, const tracked_ptr<T>& p) {
        s << p.get();
        return s;
    }

}

namespace std {
    template<class T>
    struct hash<sgcl::tracked_ptr<T>> {
        std::size_t operator()(const sgcl::tracked_ptr<T>& p) const noexcept {
            return std::hash<T*>{}(p.get());
        }
    };
}
