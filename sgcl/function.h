//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/value_storage.h"

#include <cassert>
#include <functional>
#include <typeinfo>
#include <utility>

namespace sgcl {
    using std::bad_function_call;

    namespace detail {
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

    template<class Signature, template<class> class Ptr = tracked_ptr>
    class function;

    // A function with the interface of std::function whose closure may
    // capture tracked pointers: the closure lives in a managed node of
    // its own (detail/value_storage.h), held by a pointer in the function's
    // word and traced through its own pointer map, so that a closure
    // capturing the object that holds the function is a cycle collected
    // like any other; a small closure without pointers lives in the
    // function itself. Copyable when the callable is (a copy of a closure
    // in a node is a node of its own); the callable is called as an
    // lvalue, as std::function calls it; bad_function_call (the one of
    // std) on an empty function; the closure destroyed the moment the
    // function drops it. Ptr is the kind of the word, and so where the
    // function lives: tracked_ptr on a stack or in a managed object,
    // gc::tracked_ptr (gc::function) anywhere. 32 bytes.
    template<class R, class... Args, template<class> class Ptr>
    class function<R(Args...), Ptr> : detail::ValueStorage<Ptr, R (*)(const void*, Args&&...)> {
        using Storage = detail::ValueStorage<Ptr, R (*)(const void*, Args&&...)>;   // the invoker gets the storage as a void pointer

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
            return this->_manager->extra(this, std::forward<Args>(a)...);
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
            return detail::invoke_as<R>(*Storage::template _value<F>(*static_cast<const Storage*>(s)), std::forward<Args>(a)...);
        }
    };

    template<class R, class... Args>
    function(R (*)(Args...)) -> function<R(Args...)>;

    template<class F>
    function(F) -> function<typename detail::CallSignature<decltype(&F::operator())>::type>;

    template<class R, class... Args, template<class> class Ptr>
    void swap(function<R(Args...), Ptr>& l, function<R(Args...), Ptr>& r) noexcept {
        l.swap(r);
    }

    template<class R, class... Args, template<class> class Ptr>
    bool operator==(const function<R(Args...), Ptr>& f, std::nullptr_t) noexcept {
        return !f;
    }

    namespace detail {
        // move_only_function's one implementation for the signatures with
        // and without const and noexcept: the qualifiers are the class's
        // parameters
        template<template<class> class Ptr, class R, bool Const, bool Noexcept, class... Args>
        class MoveOnlyFunction : ValueStorage<Ptr, R (*)(const void*, Args&&...)> {
            using Storage = ValueStorage<Ptr, R (*)(const void*, Args&&...)>;

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
                return this->_manager->extra(this, std::forward<Args>(a)...);
            }

            R operator()(Args... a) const noexcept(Noexcept)
            requires Const {
                assert(this->_manager && "an empty move_only_function called");
                return this->_manager->extra(this, std::forward<Args>(a)...);
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
                return invoke_as<R>(*static_cast<CallAs<F>*>(Storage::template _value<F>(*static_cast<const Storage*>(s))), std::forward<Args>(a)...);
            }
        };
    }

    // std::move_only_function for a closure with tracked pointers: the
    // storage of function, a callable that need not be copyable, the
    // signature's const and noexcept honoured (the reference qualifiers
    // are not supported). Calling an empty one is undefined.
    template<class Signature, template<class> class Ptr = tracked_ptr>
    class move_only_function;

    template<class R, class... Args, template<class> class Ptr>
    class move_only_function<R(Args...), Ptr> : public detail::MoveOnlyFunction<Ptr, R, false, false, Args...> {
        using Base = detail::MoveOnlyFunction<Ptr, R, false, false, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };

    template<class R, class... Args, template<class> class Ptr>
    class move_only_function<R(Args...) const, Ptr> : public detail::MoveOnlyFunction<Ptr, R, true, false, Args...> {
        using Base = detail::MoveOnlyFunction<Ptr, R, true, false, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };

    template<class R, class... Args, template<class> class Ptr>
    class move_only_function<R(Args...) noexcept, Ptr> : public detail::MoveOnlyFunction<Ptr, R, false, true, Args...> {
        using Base = detail::MoveOnlyFunction<Ptr, R, false, true, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };

    template<class R, class... Args, template<class> class Ptr>
    class move_only_function<R(Args...) const noexcept, Ptr> : public detail::MoveOnlyFunction<Ptr, R, true, true, Args...> {
        using Base = detail::MoveOnlyFunction<Ptr, R, true, true, Args...>;
    public:
        using Base::Base;
        using Base::operator=;
    };
}
