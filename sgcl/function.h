//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer_word.h"
#include "make_tracked.h"

#include <cassert>
#include <cstring>
#include <functional>
#include <typeinfo>
#include <utility>

namespace sgcl {
    using std::bad_function_call;

    namespace detail {
        // Where a callable lives, shared by function and move_only_function
        // (any.h has the same shape for a value): a small one that cannot
        // hold a pointer in a buffer of 16 bytes inside; any other, a
        // closure with tracked pointers among its captures first of all,
        // in a managed object of its own, owned the way a unique_ptr owns
        // (a root by the state of its slot, traced through its own pointer
        // map, destroyed the moment it is dropped), its address in a word
        // that holds null or an address and nothing else. A std::function
        // would keep the small closure in its buffer, the pointer sharing
        // its offset with the data of other closures (README: Pointer
        // maps), and the large one on the unmanaged heap, where a
        // tracked_ptr may not live.
        template<class Invoke>
        class CallableStorage {
        protected:
            static constexpr size_t BufferSize = 16;
            static constexpr size_t BufferAlign = 8;

            template<class F>
            static constexpr bool Inline = !MayContainTracked<F>::value && sizeof(F) <= BufferSize && alignof(F) <= BufferAlign && std::is_nothrow_move_constructible_v<F>;

            struct Manager {
                const std::type_info& type;
                void (*copy)(CallableStorage& to, const CallableStorage& from);   // null for a move-only callable
                void (*move)(CallableStorage& to, CallableStorage& from) noexcept;
                void (*destroy)(CallableStorage& s) noexcept;
                void* (*get)(const CallableStorage& s) noexcept;
                Invoke invoke;   // the call, for the signature of the function holding this
            };

            template<class F, bool Copyable, Invoke I>
            struct InlineOps {
                static void copy(CallableStorage& to, const CallableStorage& from) {
                    if constexpr(Copyable) {
                        ::new(to._buffer) F(*static_cast<const F*>(get(from)));
                        to._manager = from._manager;
                    }
                }
                static void move(CallableStorage& to, CallableStorage& from) noexcept {
                    ::new(to._buffer) F(std::move(*static_cast<F*>(get(from))));
                    to._manager = from._manager;
                    destroy(from);
                }
                static void destroy(CallableStorage& s) noexcept {
                    static_cast<F*>(get(s))->~F();
                    s._manager = nullptr;
                }
                static void* get(const CallableStorage& s) noexcept {
                    return const_cast<unsigned char*>(s._buffer);
                }
                static constexpr Manager manager = {typeid(F), Copyable ? copy : nullptr, move, destroy, get, I};
            };

            template<class F, bool Copyable, Invoke I>
            struct ObjectOps {
                static void copy(CallableStorage& to, const CallableStorage& from) {
                    if constexpr(Copyable) {
                        to._set_object(make_tracked<F>(*static_cast<const F*>(get(from))).release());
                        to._manager = from._manager;
                    }
                }
                static void move(CallableStorage& to, CallableStorage& from) noexcept {
                    to._set_object(from._object());
                    to._manager = from._manager;
                    from._set_object(nullptr);
                    from._manager = nullptr;
                }
                static void destroy(CallableStorage& s) noexcept {
                    Collector::delete_unique(s._object());
                    s._set_object(nullptr);
                    s._manager = nullptr;
                }
                static void* get(const CallableStorage& s) noexcept {
                    return s._object();
                }
                static constexpr Manager manager = {typeid(F), Copyable ? copy : nullptr, move, destroy, get, I};
            };

            // The callable of type F held here, without the manager: for the
            // invoker, which knows F
            template<class F>
            static F* _callable(const CallableStorage& s) noexcept {
                if constexpr(Inline<F>) {
                    return static_cast<F*>(InlineOps<F, false, nullptr>::get(s));
                } else {
                    return static_cast<F*>(s._object());
                }
            }

            CallableStorage() noexcept = default;

            CallableStorage(const CallableStorage& o) {
                if (o._manager) {
                    o._manager->copy(*this, o);
                }
            }

            CallableStorage(CallableStorage&& o) noexcept {
                if (o._manager) {
                    o._manager->move(*this, o);
                }
            }

            ~CallableStorage() {
                _reset();
            }

            CallableStorage& operator=(const CallableStorage&) = delete;
            CallableStorage& operator=(CallableStorage&&) = delete;

            template<class F, bool Copyable, Invoke I, class... A>
            F& _emplace(A&&... a) {
                if constexpr(Inline<F>) {
                    auto p = ::new(_buffer) F(std::forward<A>(a)...);
                    _manager = &InlineOps<F, Copyable, I>::manager;
                    return *p;
                } else {
                    static_assert(sizeof(detail::Array<sizeof(F)>) <= detail::PageDataSize, "a callable larger than a page is not supported");
                    auto p = make_tracked<F>(std::forward<A>(a)...).release();
                    _set_object(p);
                    _manager = &ObjectOps<F, Copyable, I>::manager;
                    return *p;
                }
            }

            void _reset() noexcept {
                if (_manager) {
                    _manager->destroy(*this);
                }
            }

            void _swap(CallableStorage& o) noexcept {
                if (this == &o) {
                    return;
                }
                CallableStorage tmp(std::move(o));
                if (_manager) {
                    _manager->move(o, *this);
                }
                if (tmp._manager) {
                    tmp._manager->move(*this, tmp);
                }
            }

            void* _get() const noexcept {
                return _manager->get(*this);
            }

            void* _object() const noexcept {
                void* p;
                std::memcpy(&p, _word, sizeof(p));
                return p;
            }

            void _set_object(void* p) noexcept {
                std::memcpy(_word, &p, sizeof(p));
            }

            const Manager* _manager = nullptr;
            alignas(BufferAlign) unsigned char _word[sizeof(void*)] = {};
            alignas(BufferAlign) unsigned char _buffer[BufferSize];
        };

        // A null function pointer, member pointer or empty function: what
        // std::function treats as nothing to hold
        template<class F>
        bool callable_is_null(const F& f) noexcept {
            if constexpr(std::is_pointer_v<F> || std::is_member_pointer_v<F>) {
                return f == nullptr;
            } else if constexpr(requires { static_cast<bool>(!f); requires std::is_class_v<F>; }) {
                if constexpr(requires { typename F::result_type; f.target_type(); }) {   // a function of either library
                    return !f;
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }

        template<class T>
        struct IsInPlaceType : std::false_type {};

        template<class T>
        struct IsInPlaceType<std::in_place_type_t<T>> : std::true_type {};

        template<class R, class F, class... A>
        R invoke_as(F& f, A&&... a) {
            if constexpr(std::is_void_v<R>) {
                std::invoke(f, std::forward<A>(a)...);
            } else {
                return std::invoke(f, std::forward<A>(a)...);
            }
        }

        // The signature a class's operator() has: the deduction guide of
        // function from a functor
        template<class T>
        struct CallSignature;

        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...)> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) const> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) noexcept> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) const noexcept> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) &> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) const &> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) & noexcept> { using type = R(A...); };
        template<class G, class R, class... A>
        struct CallSignature<R (G::*)(A...) const & noexcept> { using type = R(A...); };
    }

    template<class Signature>
    class function;

    // A function with the interface of std::function whose closure may
    // capture tracked pointers: the closure lives in a managed object of
    // its own (detail::CallableStorage), traced through its own pointer
    // map, and a small closure without pointers in the function itself.
    // Copyable when the callable is (a copy of a closure in a managed
    // object is another managed object); the callable is called as an
    // lvalue, as std::function calls it; bad_function_call (the one of
    // std) on an empty function. Has no word of its own: it lives where
    // its closure may, as a member would: a closure capturing an
    // sgcl::tracked_ptr where a tracked_ptr may, one capturing a
    // gc::tracked_ptr anywhere; a closure without pointers anywhere.
    // 32 bytes.
    template<class R, class... Args>
    class function<R(Args...)> : detail::CallableStorage<R (*)(const void*, Args&&...)> {
        using Storage = detail::CallableStorage<R (*)(const void*, Args&&...)>;   // the invoker gets the storage as a void pointer

        template<class F>
        static constexpr bool Callable = !std::is_same_v<F, function> && std::is_invocable_r_v<R, F&, Args...>;

    public:
        using result_type = R;

        function() noexcept = default;

        function(std::nullptr_t) noexcept {
        }

        function(const function& o) = default;

        function(function&& o) noexcept = default;

        // From a callable: nothing to hold for a null function pointer,
        // a null member pointer or an empty function
        template<class F, class VF = std::decay_t<F>>
        requires Callable<VF> && std::is_copy_constructible_v<VF>
        function(F&& f) {
            if (!detail::callable_is_null(f)) {
                this->template _emplace<VF, true, &_call<VF>>(std::forward<F>(f));
            }
        }

        ~function() = default;

        function& operator=(const function& o) {
            function(o).swap(*this);
            return *this;
        }

        function& operator=(function&& o) noexcept {
            function(std::move(o)).swap(*this);
            return *this;
        }

        function& operator=(std::nullptr_t) noexcept {
            this->_reset();
            return *this;
        }

        template<class F, class VF = std::decay_t<F>>
        requires Callable<VF> && std::is_copy_constructible_v<VF>
        function& operator=(F&& f) {
            function(std::forward<F>(f)).swap(*this);
            return *this;
        }

        template<class F>
        requires Callable<std::reference_wrapper<F>>
        function& operator=(std::reference_wrapper<F> f) noexcept {
            function(f).swap(*this);
            return *this;
        }

        void swap(function& o) noexcept {
            this->_swap(o);
        }

        explicit operator bool() const noexcept {
            return this->_manager != nullptr;
        }

        R operator()(Args... a) const {
            if (!this->_manager) {
                throw bad_function_call();
            }
            return this->_manager->invoke(this, std::forward<Args>(a)...);
        }

        const std::type_info& target_type() const noexcept {
            return this->_manager ? this->_manager->type : typeid(void);
        }

        template<class T>
        T* target() noexcept {
            return this->_manager && this->_manager->type == typeid(T) ? static_cast<T*>(this->_get()) : nullptr;
        }

        template<class T>
        const T* target() const noexcept {
            return this->_manager && this->_manager->type == typeid(T) ? static_cast<const T*>(this->_get()) : nullptr;
        }

    private:
        template<class F>
        static R _call(const void* s, Args&&... a) {
            return detail::invoke_as<R>(*Storage::template _callable<F>(*static_cast<const Storage*>(s)), std::forward<Args>(a)...);
        }
    };

    template<class R, class... Args>
    function(R (*)(Args...)) -> function<R(Args...)>;

    template<class F>
    function(F) -> function<typename detail::CallSignature<decltype(&F::operator())>::type>;

    template<class R, class... Args>
    void swap(function<R(Args...)>& l, function<R(Args...)>& r) noexcept {
        l.swap(r);
    }

    template<class R, class... Args>
    bool operator==(const function<R(Args...)>& f, std::nullptr_t) noexcept {
        return !f;
    }

    namespace detail {
        // move_only_function's one implementation for the signatures with
        // and without const and noexcept: the qualifiers are the class's
        // parameters
        template<class R, bool Const, bool Noexcept, class... Args>
        class MoveOnlyFunction : CallableStorage<R (*)(const void*, Args&&...)> {
            using Storage = CallableStorage<R (*)(const void*, Args&&...)>;

            template<class F>
            using CallAs = std::conditional_t<Const, const F, F>;

            template<class F>
            static constexpr bool Callable = (Noexcept ? std::is_nothrow_invocable_r_v<R, CallAs<F>&, Args...> : std::is_invocable_r_v<R, CallAs<F>&, Args...>);

        public:
            using result_type = R;

            MoveOnlyFunction() noexcept = default;

            MoveOnlyFunction(std::nullptr_t) noexcept {
            }

            MoveOnlyFunction(MoveOnlyFunction&& o) noexcept = default;

            MoveOnlyFunction(const MoveOnlyFunction&) = delete;

            template<class F, class VF = std::decay_t<F>>
            requires (!std::is_same_v<VF, MoveOnlyFunction>) && (!IsInPlaceType<VF>::value) && Callable<VF> && std::is_constructible_v<VF, F>
            MoveOnlyFunction(F&& f) {
                if (!callable_is_null(f)) {
                    this->template _emplace<VF, false, &_call<VF>>(std::forward<F>(f));
                }
            }

            template<class T, class... A, class VF = std::decay_t<T>>
            requires Callable<VF> && std::is_constructible_v<VF, A...>
            explicit MoveOnlyFunction(std::in_place_type_t<T>, A&&... a) {
                this->template _emplace<VF, false, &_call<VF>>(std::forward<A>(a)...);
            }

            template<class T, class U, class... A, class VF = std::decay_t<T>>
            requires Callable<VF> && std::is_constructible_v<VF, std::initializer_list<U>&, A...>
            explicit MoveOnlyFunction(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a) {
                this->template _emplace<VF, false, &_call<VF>>(il, std::forward<A>(a)...);
            }

            ~MoveOnlyFunction() = default;

            MoveOnlyFunction& operator=(MoveOnlyFunction&& o) noexcept {
                MoveOnlyFunction(std::move(o)).swap(*this);
                return *this;
            }

            MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;

            MoveOnlyFunction& operator=(std::nullptr_t) noexcept {
                this->_reset();
                return *this;
            }

            template<class F>
            requires std::is_constructible_v<MoveOnlyFunction, F>
            MoveOnlyFunction& operator=(F&& f) {
                MoveOnlyFunction(std::forward<F>(f)).swap(*this);
                return *this;
            }

            void swap(MoveOnlyFunction& o) noexcept {
                this->_swap(o);
            }

            explicit operator bool() const noexcept {
                return this->_manager != nullptr;
            }

            // Calling an empty one is undefined, as with std (asserted here)
            R operator()(Args... a) noexcept(Noexcept)
            requires (!Const) {
                assert(this->_manager && "an empty move_only_function called");
                return this->_manager->invoke(this, std::forward<Args>(a)...);
            }

            R operator()(Args... a) const noexcept(Noexcept)
            requires Const {
                assert(this->_manager && "an empty move_only_function called");
                return this->_manager->invoke(this, std::forward<Args>(a)...);
            }

            friend void swap(MoveOnlyFunction& l, MoveOnlyFunction& r) noexcept {
                l.swap(r);
            }

            friend bool operator==(const MoveOnlyFunction& f, std::nullptr_t) noexcept {
                return !f;
            }

        private:
            template<class F>
            static R _call(const void* s, Args&&... a) noexcept(Noexcept) {
                return invoke_as<R>(*static_cast<CallAs<F>*>(Storage::template _callable<F>(*static_cast<const Storage*>(s))), std::forward<Args>(a)...);
            }
        };
    }

    // std::move_only_function for a closure with tracked pointers: the
    // storage of function, a callable that need not be copyable, the
    // signature's const and noexcept honoured (the reference qualifiers
    // are not supported). Calling an empty one is undefined.
    template<class Signature>
    class move_only_function;

    template<class R, class... Args>
    class move_only_function<R(Args...)> : public detail::MoveOnlyFunction<R, false, false, Args...> {
        using Base = detail::MoveOnlyFunction<R, false, false, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };

    template<class R, class... Args>
    class move_only_function<R(Args...) const> : public detail::MoveOnlyFunction<R, true, false, Args...> {
        using Base = detail::MoveOnlyFunction<R, true, false, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };

    template<class R, class... Args>
    class move_only_function<R(Args...) noexcept> : public detail::MoveOnlyFunction<R, false, true, Args...> {
        using Base = detail::MoveOnlyFunction<R, false, true, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };

    template<class R, class... Args>
    class move_only_function<R(Args...) const noexcept> : public detail::MoveOnlyFunction<R, true, true, Args...> {
        using Base = detail::MoveOnlyFunction<R, true, true, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };
}
