//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Function<R(Args...)>: a callable held on the
// managed heap, so that a Ptr it captured is traced. MoveOnlyFunction
// for a callable that cannot be copied.
#pragma once

#include "../../core/function.h"

#include <functional>

namespace Sgcl {
    template<class Signature>
    class Function;

    template<class Signature>
    class MoveOnlyFunction;

    namespace detail {
        template<class F>
        struct IsFunctionWrapper : std::false_type {};

        template<class S>
        struct IsFunctionWrapper<Function<S>> : std::true_type {};

        template<class S>
        struct IsFunctionWrapper<MoveOnlyFunction<S>> : std::true_type {};

        // What the inner function is made from: a Function or a
        // MoveOnlyFunction of another signature hands over its inner
        // function, so that an empty one makes an empty one (sgcl's
        // function knows its own kind; the wrapper it would take for a
        // callable holding an empty function and call into it)
        template<class F>
        decltype(auto) inner_callable(F&& f) noexcept {
            if constexpr(IsFunctionWrapper<std::remove_cvref_t<F>>::value) {
                if constexpr(std::is_lvalue_reference_v<F>) {
                    return f.Inner();
                } else {
                    return std::move(f.Inner());
                }
            } else {
                return std::forward<F>(f);
            }
        }
    }

    template<class R, class... Args>
    class Function<R(Args...)> {
    public:
        using ResultType = R;
        using InnerType = sgcl::function<R(Args...)>;

        Function() noexcept = default;

        Function(std::nullptr_t) noexcept
        : _f(nullptr) {
        }

        Function(const Function&) = default;
        Function(Function&&) noexcept = default;
        Function& operator=(const Function&) = default;
        Function& operator=(Function&&) noexcept = default;

        template<class F>
        requires (!std::is_same_v<std::remove_cvref_t<F>, Function>) && (!std::is_same_v<std::remove_cvref_t<F>, InnerType>) && std::is_constructible_v<InnerType, F&&>
        Function(F&& f)
        : _f(detail::inner_callable(std::forward<F>(f))) {
        }

        template<class I>
        requires std::is_same_v<std::remove_cvref_t<I>, InnerType>
        explicit Function(I&& f) noexcept   // a template, as Any's: no conversion of a Function to sgcl::function tried
        : _f(std::forward<I>(f)) {
        }

        Function& operator=(std::nullptr_t) noexcept {
            _f = nullptr;
            return *this;
        }

        template<class F>
        Function& operator=(std::reference_wrapper<F> f) noexcept {
            _f = f;
            return *this;
        }

        template<class F>
        requires (!std::is_same_v<std::remove_cvref_t<F>, Function>) && (!std::is_same_v<std::remove_cvref_t<F>, InnerType>) && std::is_constructible_v<InnerType, F&&>
        Function& operator=(F&& f) {
            _f = detail::inner_callable(std::forward<F>(f));
            return *this;
        }

        R operator()(Args... a) const {
            return _f(std::forward<Args>(a)...);
        }

        explicit operator bool() const noexcept {
            return (bool)_f;
        }

        const std::type_info& TargetType() const noexcept {
            return _f.target_type();
        }

        template<class T>
        T* Target() noexcept {
            return _f.template target<T>();
        }

        template<class T>
        const T* Target() const noexcept {
            return _f.template target<T>();
        }

        void Swap(Function& o) noexcept {
            _f.swap(o._f);
        }

        InnerType& Inner() noexcept {
            return _f;
        }

        const InnerType& Inner() const noexcept {
            return _f;
        }

        friend bool operator==(const Function& f, std::nullptr_t) noexcept {
            return !f;
        }

    private:
        InnerType _f;
    };

    template<class R, class... Args>
    Function(R (*)(Args...)) -> Function<R(Args...)>;

    template<class F>
    Function(F) -> Function<typename sgcl::detail::CallSignature<decltype(&F::operator())>::type>;

    template<class R, class... Args>
    void swap(Function<R(Args...)>& a, Function<R(Args...)>& b) noexcept {
        a.Swap(b);
    }

    template<class Signature>
    class MoveOnlyFunction {
    public:
        using InnerType = sgcl::move_only_function<Signature>;

        MoveOnlyFunction() noexcept = default;

        MoveOnlyFunction(std::nullptr_t) noexcept
        : _f(nullptr) {
        }

        MoveOnlyFunction(MoveOnlyFunction&&) noexcept = default;
        MoveOnlyFunction& operator=(MoveOnlyFunction&&) noexcept = default;
        MoveOnlyFunction(const MoveOnlyFunction&) = delete;
        MoveOnlyFunction& operator=(const MoveOnlyFunction&) = delete;

        template<class F>
        requires (!std::is_same_v<std::remove_cvref_t<F>, MoveOnlyFunction>) && (!std::is_same_v<std::remove_cvref_t<F>, InnerType>) && std::is_constructible_v<InnerType, F&&>
        MoveOnlyFunction(F&& f)
        : _f(detail::inner_callable(std::forward<F>(f))) {
        }

        template<class T, class... A>
        explicit MoveOnlyFunction(std::in_place_type_t<T> t, A&&... a)
        : _f(t, std::forward<A>(a)...) {
        }

        template<class I>
        requires std::is_same_v<std::remove_cvref_t<I>, InnerType>
        explicit MoveOnlyFunction(I&& f) noexcept
        : _f(std::forward<I>(f)) {
        }

        template<class... A>
        decltype(auto) operator()(A&&... a) {
            return _f(std::forward<A>(a)...);
        }

        template<class... A>
        decltype(auto) operator()(A&&... a) const {
            return _f(std::forward<A>(a)...);
        }

        explicit operator bool() const noexcept {
            return (bool)_f;
        }

        void Swap(MoveOnlyFunction& o) noexcept {
            _f.swap(o._f);
        }

        InnerType& Inner() noexcept {
            return _f;
        }

        const InnerType& Inner() const noexcept {
            return _f;
        }

    private:
        InnerType _f;
    };

    template<class R, class... Args>
    MoveOnlyFunction(R (*)(Args...)) -> MoveOnlyFunction<R(Args...)>;

    template<class F>
    MoveOnlyFunction(F) -> MoveOnlyFunction<typename sgcl::detail::CallSignature<decltype(&F::operator())>::type>;
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

