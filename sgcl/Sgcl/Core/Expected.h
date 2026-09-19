//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Expected<T, E>: a value or an error, one of them a Ptr traced. Value()
// without one is BadExpectedAccess, as
// std::expected; HasValue() first, or ValueOr.
#pragma once

#include "../../core/expected.h"

#include <functional>

namespace Sgcl {
    // An error on its way into an Expected: `return Unexpected("text");`
    template<class E>
    class Unexpected {
    public:
        using ErrorType = E;
        using InnerType = sgcl::unexpected<E>;

        template<class Err = E>
        requires (!std::is_same_v<std::remove_cvref_t<Err>, Unexpected>) && (!std::is_same_v<std::remove_cvref_t<Err>, InnerType>) && std::is_constructible_v<E, Err&&>
        explicit Unexpected(Err&& e)
        : _u(std::forward<Err>(e)) {
        }

        template<class... A>
        explicit Unexpected(std::in_place_t, A&&... a)
        : _u(std::in_place, std::forward<A>(a)...) {
        }

        Unexpected(InnerType u)
        : _u(std::move(u)) {
        }

        E& Error() & noexcept {
            return _u.error();
        }

        const E& Error() const& noexcept {
            return _u.error();
        }

        E&& Error() && noexcept {
            return std::move(_u).error();
        }

        void Swap(Unexpected& o) noexcept {
            _u.swap(o._u);
        }

        InnerType& Inner() noexcept {
            return _u;
        }

        const InnerType& Inner() const noexcept {
            return _u;
        }

        operator const InnerType&() const noexcept {
            return _u;
        }

        template<class E2>
        friend bool operator==(const Unexpected& a, const Unexpected<E2>& b) {
            return a.Error() == b.Error();
        }

    private:
        InnerType _u;
    };

    template<class E>
    Unexpected(E) -> Unexpected<E>;

    // What Value() throws without a value: the error, by Error()
    template<class E>
    class BadExpectedAccess : public sgcl::bad_expected_access<E> {
    public:
        using sgcl::bad_expected_access<E>::bad_expected_access;

        E& Error() & noexcept {
            return this->error();
        }

        const E& Error() const& noexcept {
            return this->error();
        }
    };

    using UnexpectType = sgcl::unexpect_t;
    inline constexpr UnexpectType Unexpect{};

    template<class E>
    Unexpected<std::decay_t<E>> MakeUnexpected(E&& e) {
        return Unexpected<std::decay_t<E>>(std::forward<E>(e));
    }

    template<class T, class E>
    class Expected;

    namespace detail {
        template<class X>
        struct IsExpected : std::false_type {};

        template<class T, class E>
        struct IsExpected<Expected<T, E>> : std::true_type {};
    }

    template<class T, class E>
    class Expected {
    public:
        using ValueType = T;
        using ErrorType = E;
        using InnerType = sgcl::expected<T, E>;

        Expected() = default;
        Expected(const Expected&) = default;
        Expected(Expected&&) = default;
        Expected& operator=(const Expected&) = default;
        Expected& operator=(Expected&&) = default;

        // A value; never an Expected of other types (those convert below,
        // and a bool would otherwise be made from has_value: LWG 3836)
        template<class U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, Expected>) && (!std::is_same_v<std::remove_cvref_t<U>, InnerType>) && (!detail::IsExpected<std::remove_cvref_t<U>>::value) && std::is_constructible_v<InnerType, U&&>
        Expected(U&& v)
        : _e(std::forward<U>(v)) {
        }

        // From an Expected of other types: the value converted, or the
        // error, whichever it holds (the inner expected's converting
        // constructors, with the standard's constraints); explicit
        // where the value's or the error's conversion is
        template<class U, class G>
        requires (!std::is_same_v<Expected<U, G>, Expected>) && std::is_constructible_v<InnerType, const sgcl::expected<U, G>&>
        explicit(!std::is_convertible_v<const U&, T> || !std::is_convertible_v<const G&, E>)
        Expected(const Expected<U, G>& o)
        : _e(o.Inner()) {
        }

        template<class U, class G>
        requires (!std::is_same_v<Expected<U, G>, Expected>) && std::is_constructible_v<InnerType, sgcl::expected<U, G>&&>
        explicit(!std::is_convertible_v<U&&, T> || !std::is_convertible_v<G&&, E>)
        Expected(Expected<U, G>&& o)
        : _e(std::move(o.Inner())) {
        }

        template<class G>
        Expected(const Unexpected<G>& u)
        : _e(u.Inner()) {
        }

        template<class G>
        Expected(Unexpected<G>&& u)
        : _e(std::move(u.Inner())) {
        }

        template<class... A>
        explicit Expected(std::in_place_t, A&&... a)
        : _e(std::in_place, std::forward<A>(a)...) {
        }

        template<class... A>
        explicit Expected(UnexpectType, A&&... a)
        : _e(sgcl::unexpect, std::forward<A>(a)...) {
        }

        template<class I>
        requires std::is_same_v<std::remove_cvref_t<I>, InnerType>
        Expected(I&& e)   // a template, as Any's: no conversion of an Expected to sgcl::expected tried
        : _e(std::forward<I>(e)) {
        }

        template<class... A>
        T& Emplace(A&&... a) {
            return _e.emplace(std::forward<A>(a)...);
        }

        // The monadic operations: f on the value, the error passed through
        // (AndThen: f returns an Expected<U, E>; Transform: f returns a U,
        // the result an Expected<U, E>), or f on the error, the value
        // passed through (OrElse: f returns an Expected<T, G>;
        // TransformError: f returns a G, the result an Expected<T, G>)
        template<class F>
        auto AndThen(F&& f) & {
            using R = std::remove_cvref_t<std::invoke_result_t<F, T&>>;
            static_assert(detail::IsExpected<R>::value && std::is_same_v<typename R::ErrorType, E>, "AndThen's function returns an Expected with the same error type");
            return HasValue() ? R(std::invoke(std::forward<F>(f), **this)) : R(Unexpected<E>(Error()));
        }

        template<class F>
        auto AndThen(F&& f) const& {
            using R = std::remove_cvref_t<std::invoke_result_t<F, const T&>>;
            static_assert(detail::IsExpected<R>::value && std::is_same_v<typename R::ErrorType, E>, "AndThen's function returns an Expected with the same error type");
            return HasValue() ? R(std::invoke(std::forward<F>(f), **this)) : R(Unexpected<E>(Error()));
        }

        template<class F>
        auto OrElse(F&& f) & {
            using R = std::remove_cvref_t<std::invoke_result_t<F, E&>>;
            static_assert(detail::IsExpected<R>::value && std::is_same_v<typename R::ValueType, T>, "OrElse's function returns an Expected with the same value type");
            return HasValue() ? R(std::in_place, **this) : R(std::invoke(std::forward<F>(f), Error()));
        }

        template<class F>
        auto OrElse(F&& f) const& {
            using R = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
            static_assert(detail::IsExpected<R>::value && std::is_same_v<typename R::ValueType, T>, "OrElse's function returns an Expected with the same value type");
            return HasValue() ? R(std::in_place, **this) : R(std::invoke(std::forward<F>(f), Error()));
        }

        template<class F>
        auto Transform(F&& f) & {
            using U = std::remove_cv_t<std::invoke_result_t<F, T&>>;
            return HasValue() ? Expected<U, E>(std::in_place, std::invoke(std::forward<F>(f), **this)) : Expected<U, E>(Unexpected<E>(Error()));
        }

        template<class F>
        auto Transform(F&& f) const& {
            using U = std::remove_cv_t<std::invoke_result_t<F, const T&>>;
            return HasValue() ? Expected<U, E>(std::in_place, std::invoke(std::forward<F>(f), **this)) : Expected<U, E>(Unexpected<E>(Error()));
        }

        template<class F>
        auto TransformError(F&& f) & {
            using G = std::remove_cv_t<std::invoke_result_t<F, E&>>;
            return HasValue() ? Expected<T, G>(std::in_place, **this) : Expected<T, G>(Unexpected<G>(std::invoke(std::forward<F>(f), Error())));
        }

        template<class F>
        auto TransformError(F&& f) const& {
            using G = std::remove_cv_t<std::invoke_result_t<F, const E&>>;
            return HasValue() ? Expected<T, G>(std::in_place, **this) : Expected<T, G>(Unexpected<G>(std::invoke(std::forward<F>(f), Error())));
        }

        bool HasValue() const noexcept {
            return _e.has_value();
        }

        explicit operator bool() const noexcept {
            return _e.has_value();
        }

        // The value; without one, a BadExpectedAccess<E> carrying the error
        T& Value() & {
            _check();
            return *_e;
        }

        const T& Value() const& {
            _check();
            return *_e;
        }

        T&& Value() && {
            _check();
            return std::move(*_e);
        }

        E& Error() & noexcept {
            return _e.error();
        }

        const E& Error() const& noexcept {
            return _e.error();
        }

        template<class U>
        T ValueOr(U&& v) const& {
            return _e.value_or(std::forward<U>(v));
        }

        template<class U>
        T ValueOr(U&& v) && {
            return std::move(_e).value_or(std::forward<U>(v));
        }

        template<class G>
        E ErrorOr(G&& e) const& {
            return _e.error_or(std::forward<G>(e));
        }

        T& operator*() & noexcept {
            return *_e;
        }

        const T& operator*() const& noexcept {
            return *_e;
        }

        T* operator->() noexcept {
            return _e.operator->();
        }

        const T* operator->() const noexcept {
            return _e.operator->();
        }

        void _check() const {
            if (!_e.has_value()) {
                throw BadExpectedAccess<E>(_e.error());
            }
        }

        void Swap(Expected& o) noexcept(noexcept(std::declval<InnerType&>().swap(std::declval<InnerType&>()))) {
            _e.swap(o._e);
        }

        InnerType& Inner() noexcept {
            return _e;
        }

        const InnerType& Inner() const noexcept {
            return _e;
        }

        friend bool operator==(const Expected& a, const Expected& b) {
            return a._e == b._e;
        }

        template<class T2>
        requires (!std::is_same_v<T2, Expected>)
        friend bool operator==(const Expected& a, const T2& v) {
            return a._e == v;
        }

        template<class G>
        friend bool operator==(const Expected& a, const Unexpected<G>& u) {
            return a._e == u.Inner();
        }

    private:
        InnerType _e;
    };

    template<class E>
    class Expected<void, E> {
    public:
        using ValueType = void;
        using ErrorType = E;
        using InnerType = sgcl::expected<void, E>;

        Expected() noexcept = default;
        Expected(const Expected&) = default;
        Expected(Expected&&) = default;
        Expected& operator=(const Expected&) = default;
        Expected& operator=(Expected&&) = default;

        template<class G>
        Expected(const Unexpected<G>& u)
        : _e(u.Inner()) {
        }

        template<class G>
        Expected(Unexpected<G>&& u)
        : _e(std::move(u.Inner())) {
        }

        template<class... A>
        explicit Expected(UnexpectType, A&&... a)
        : _e(sgcl::unexpect, std::forward<A>(a)...) {
        }

        template<class I>
        requires std::is_same_v<std::remove_cvref_t<I>, InnerType>
        Expected(I&& e)
        : _e(std::forward<I>(e)) {
        }

        void Emplace() noexcept {
            _e.emplace();
        }

        template<class F>
        auto AndThen(F&& f) const {
            using R = std::remove_cvref_t<std::invoke_result_t<F>>;
            static_assert(detail::IsExpected<R>::value && std::is_same_v<typename R::ErrorType, E>, "AndThen's function returns an Expected with the same error type");
            return HasValue() ? R(std::invoke(std::forward<F>(f))) : R(Unexpected<E>(Error()));
        }

        template<class F>
        auto OrElse(F&& f) const {
            using R = std::remove_cvref_t<std::invoke_result_t<F, const E&>>;
            static_assert(detail::IsExpected<R>::value && std::is_void_v<typename R::ValueType>, "OrElse's function returns an Expected<void, G>");
            return HasValue() ? R() : R(std::invoke(std::forward<F>(f), Error()));
        }

        template<class F>
        auto Transform(F&& f) const {
            using U = std::remove_cv_t<std::invoke_result_t<F>>;
            if constexpr (std::is_void_v<U>) {
                if (HasValue()) {
                    std::invoke(std::forward<F>(f));
                    return Expected<void, E>();
                }
                return Expected<void, E>(Unexpected<E>(Error()));
            } else {
                return HasValue() ? Expected<U, E>(std::in_place, std::invoke(std::forward<F>(f))) : Expected<U, E>(Unexpected<E>(Error()));
            }
        }

        template<class F>
        auto TransformError(F&& f) const {
            using G = std::remove_cv_t<std::invoke_result_t<F, const E&>>;
            return HasValue() ? Expected<void, G>() : Expected<void, G>(Unexpected<G>(std::invoke(std::forward<F>(f), Error())));
        }

        bool HasValue() const noexcept {
            return _e.has_value();
        }

        explicit operator bool() const noexcept {
            return _e.has_value();
        }

        void Value() const {
            if (!_e.has_value()) {
                throw BadExpectedAccess<E>(_e.error());
            }
        }

        E& Error() & noexcept {
            return _e.error();
        }

        const E& Error() const& noexcept {
            return _e.error();
        }

        template<class G>
        E ErrorOr(G&& e) const& {
            return _e.error_or(std::forward<G>(e));
        }

        InnerType& Inner() noexcept {
            return _e;
        }

        const InnerType& Inner() const noexcept {
            return _e;
        }

        friend bool operator==(const Expected& a, const Expected& b) {
            return a._e == b._e;
        }

    private:
        InnerType _e;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

