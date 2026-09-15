//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "variant.h"

#include <exception>
#include <functional>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace sgcl {
    template<class E>
    class unexpected;

    template<class T, class E>
    class expected;

    namespace detail {
        template<class T>
        struct IsUnexpected : std::false_type {};

        template<class E>
        struct IsUnexpected<unexpected<E>> : std::true_type {};

        template<class T>
        struct IsExpected : std::false_type {};

        template<class T, class E>
        struct IsExpected<expected<T, E>> : std::true_type {};
    }

    // The error of an expected, as std::unexpected: the wrapper that tells
    // the constructor of an expected it is getting an error
    template<class E>
    class unexpected {
        static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E> && !std::is_volatile_v<E> && !detail::IsUnexpected<E>::value, "the error type of an unexpected is a plain object type");

    public:
        unexpected(const unexpected&) = default;
        unexpected(unexpected&&) = default;

        template<class Err = E>
        requires (!std::is_same_v<std::remove_cvref_t<Err>, unexpected>) && (!std::is_same_v<std::remove_cvref_t<Err>, std::in_place_t>) && std::is_constructible_v<E, Err>
        explicit unexpected(Err&& e)
        : _error(std::forward<Err>(e)) {
        }

        template<class... A>
        requires std::is_constructible_v<E, A...>
        explicit unexpected(std::in_place_t, A&&... a)
        : _error(std::forward<A>(a)...) {
        }

        template<class U, class... A>
        requires std::is_constructible_v<E, std::initializer_list<U>&, A...>
        explicit unexpected(std::in_place_t, std::initializer_list<U> il, A&&... a)
        : _error(il, std::forward<A>(a)...) {
        }

        unexpected& operator=(const unexpected&) = default;
        unexpected& operator=(unexpected&&) = default;

        const E& error() const& noexcept { return _error; }
        E& error() & noexcept { return _error; }
        const E&& error() const&& noexcept { return std::move(_error); }
        E&& error() && noexcept { return std::move(_error); }

        void swap(unexpected& o) noexcept(std::is_nothrow_swappable_v<E>)
        requires std::is_swappable_v<E> {
            using std::swap;
            swap(_error, o._error);
        }

        template<class E2>
        friend bool operator==(const unexpected& x, const unexpected<E2>& y) {
            return x.error() == y.error();
        }

        friend void swap(unexpected& x, unexpected& y) noexcept(noexcept(x.swap(y)))
        requires std::is_swappable_v<E> {
            x.swap(y);
        }

    private:
        E _error;
    };

    template<class E>
    unexpected(E) -> unexpected<E>;

    struct unexpect_t {
        explicit unexpect_t() = default;
    };

    inline constexpr unexpect_t unexpect{};

    template<class E>
    class bad_expected_access;

    template<>
    class bad_expected_access<void> : public std::exception {
    public:
        const char* what() const noexcept override {
            return "bad access to sgcl::expected without a value";
        }

    protected:
        bad_expected_access() noexcept = default;
        bad_expected_access(const bad_expected_access&) = default;
        bad_expected_access(bad_expected_access&&) = default;
        bad_expected_access& operator=(const bad_expected_access&) = default;
        bad_expected_access& operator=(bad_expected_access&&) = default;
        ~bad_expected_access() = default;
    };

    template<class E>
    class bad_expected_access : public bad_expected_access<void> {
    public:
        explicit bad_expected_access(E e)
        : _error(std::move(e)) {
        }

        const E& error() const& noexcept { return _error; }
        E& error() & noexcept { return _error; }
        const E&& error() const&& noexcept { return std::move(_error); }
        E&& error() && noexcept { return std::move(_error); }

    private:
        E _error;
    };

    // An expected with the interface of std::expected, safe to hold a
    // tracked pointer as its value or its error: std::expected keeps the
    // two in a union, where a pointer shares its word with the other's
    // data (README: Pointer maps); here they lie in a variant (variant.h),
    // laid out by what they hold. The interface of std::expected (C++23):
    // the constructors, in_place and unexpect, emplace, swap, operator->
    // and *, has_value, value (bad_expected_access<E> without one), error,
    // value_or, error_or, and_then, or_else, transform, transform_error,
    // the comparisons; not constexpr. Lives where its variant may: with
    // sgcl::tracked_ptrs inside where a tracked_ptr may, with
    // gc::tracked_ptrs anywhere; gc::expected is the same type.
    template<class T, class E>
    class expected {
        static_assert(!std::is_reference_v<T> && !std::is_function_v<T> && !std::is_same_v<std::remove_cv_t<T>, std::in_place_t> && !std::is_same_v<std::remove_cv_t<T>, unexpect_t> && !detail::IsUnexpected<std::remove_cv_t<T>>::value, "the value type of an expected is an object type, not in_place_t, unexpect_t or an unexpected");
        static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E> && !std::is_volatile_v<E> && !detail::IsUnexpected<E>::value, "the error type of an expected is a plain object type");

        using Storage = variant<T, E>;

        template<class U, class G>
        static constexpr bool converts_from_other = std::is_constructible_v<T, expected<U, G>&> || std::is_constructible_v<T, expected<U, G>> || std::is_constructible_v<T, const expected<U, G>&> || std::is_constructible_v<T, const expected<U, G>>
            || std::is_convertible_v<expected<U, G>&, T> || std::is_convertible_v<expected<U, G>&&, T> || std::is_convertible_v<const expected<U, G>&, T> || std::is_convertible_v<const expected<U, G>&&, T>
            || std::is_constructible_v<unexpected<E>, expected<U, G>&> || std::is_constructible_v<unexpected<E>, expected<U, G>> || std::is_constructible_v<unexpected<E>, const expected<U, G>&> || std::is_constructible_v<unexpected<E>, const expected<U, G>>;

    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        template<class U>
        using rebind = expected<U, error_type>;

        expected()
        requires std::is_default_constructible_v<T>
        : _s(std::in_place_index<0>) {
        }

        expected(const expected&) = default;
        expected(expected&&) = default;

        template<class U, class G>
        requires std::is_constructible_v<T, const U&> && std::is_constructible_v<E, const G&> && (!converts_from_other<U, G>)
        explicit(!std::is_convertible_v<const U&, T> || !std::is_convertible_v<const G&, E>)
        expected(const expected<U, G>& o)
        : _s(o.has_value() ? Storage(std::in_place_index<0>, *o) : Storage(std::in_place_index<1>, o.error())) {
        }

        template<class U, class G>
        requires std::is_constructible_v<T, U> && std::is_constructible_v<E, G> && (!converts_from_other<U, G>)
        explicit(!std::is_convertible_v<U, T> || !std::is_convertible_v<G, E>)
        expected(expected<U, G>&& o)
        : _s(o.has_value() ? Storage(std::in_place_index<0>, std::move(*o)) : Storage(std::in_place_index<1>, std::move(o.error()))) {
        }

        template<class U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) && (!std::is_same_v<std::remove_cvref_t<U>, expected>) && (!detail::IsUnexpected<std::remove_cvref_t<U>>::value) && std::is_constructible_v<T, U>
        explicit(!std::is_convertible_v<U, T>)
        expected(U&& v)
        : _s(std::in_place_index<0>, std::forward<U>(v)) {
        }

        template<class G>
        requires std::is_constructible_v<E, const G&>
        explicit(!std::is_convertible_v<const G&, E>)
        expected(const unexpected<G>& u)
        : _s(std::in_place_index<1>, u.error()) {
        }

        template<class G>
        requires std::is_constructible_v<E, G>
        explicit(!std::is_convertible_v<G, E>)
        expected(unexpected<G>&& u)
        : _s(std::in_place_index<1>, std::move(u.error())) {
        }

        template<class... A>
        requires std::is_constructible_v<T, A...>
        explicit expected(std::in_place_t, A&&... a)
        : _s(std::in_place_index<0>, std::forward<A>(a)...) {
        }

        template<class U, class... A>
        requires std::is_constructible_v<T, std::initializer_list<U>&, A...>
        explicit expected(std::in_place_t, std::initializer_list<U> il, A&&... a)
        : _s(std::in_place_index<0>, il, std::forward<A>(a)...) {
        }

        template<class... A>
        requires std::is_constructible_v<E, A...>
        explicit expected(unexpect_t, A&&... a)
        : _s(std::in_place_index<1>, std::forward<A>(a)...) {
        }

        template<class U, class... A>
        requires std::is_constructible_v<E, std::initializer_list<U>&, A...>
        explicit expected(unexpect_t, std::initializer_list<U> il, A&&... a)
        : _s(std::in_place_index<1>, il, std::forward<A>(a)...) {
        }

        expected& operator=(const expected&) = default;
        expected& operator=(expected&&) = default;

        template<class U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, expected>) && (!detail::IsUnexpected<std::remove_cvref_t<U>>::value) && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U>
        expected& operator=(U&& v) {
            if (has_value()) {
                get<0>(_s) = std::forward<U>(v);
            } else {
                _s.template emplace<0>(std::forward<U>(v));
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>
        expected& operator=(const unexpected<G>& u) {
            if (has_value()) {
                _s.template emplace<1>(u.error());
            } else {
                get<1>(_s) = u.error();
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
        expected& operator=(unexpected<G>&& u) {
            if (has_value()) {
                _s.template emplace<1>(std::move(u.error()));
            } else {
                get<1>(_s) = std::move(u.error());
            }
            return *this;
        }

        template<class... A>
        requires std::is_constructible_v<T, A...>
        T& emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>) {
            return _s.template emplace<0>(std::forward<A>(a)...);
        }

        template<class U, class... A>
        requires std::is_constructible_v<T, std::initializer_list<U>&, A...>
        T& emplace(std::initializer_list<U> il, A&&... a) noexcept(std::is_nothrow_constructible_v<T, std::initializer_list<U>&, A...>) {
            return _s.template emplace<0>(il, std::forward<A>(a)...);
        }

        void swap(expected& o) noexcept(noexcept(std::declval<Storage&>().swap(std::declval<Storage&>()))) {
            _s.swap(o._s);
        }

        friend void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y))) {
            x.swap(y);
        }

        const T* operator->() const noexcept { return &get<0>(_s); }
        T* operator->() noexcept { return &get<0>(_s); }
        const T& operator*() const& noexcept { return get<0>(_s); }
        T& operator*() & noexcept { return get<0>(_s); }
        const T&& operator*() const&& noexcept { return std::move(get<0>(_s)); }
        T&& operator*() && noexcept { return std::move(get<0>(_s)); }

        explicit operator bool() const noexcept {
            return has_value();
        }

        bool has_value() const noexcept {
            return _s.index() == 0;
        }

        // The value, or bad_expected_access<E> carrying the error
        const T& value() const& {
            if (!has_value()) {
                throw bad_expected_access<std::decay_t<E>>(error());
            }
            return get<0>(_s);
        }

        T& value() & {
            if (!has_value()) {
                throw bad_expected_access<std::decay_t<E>>(error());
            }
            return get<0>(_s);
        }

        const T&& value() const&& {
            if (!has_value()) {
                throw bad_expected_access<std::decay_t<E>>(std::move(error()));
            }
            return std::move(get<0>(_s));
        }

        T&& value() && {
            if (!has_value()) {
                throw bad_expected_access<std::decay_t<E>>(std::move(error()));
            }
            return std::move(get<0>(_s));
        }

        const E& error() const& noexcept { return get<1>(_s); }
        E& error() & noexcept { return get<1>(_s); }
        const E&& error() const&& noexcept { return std::move(get<1>(_s)); }
        E&& error() && noexcept { return std::move(get<1>(_s)); }

        template<class U>
        T value_or(U&& v) const& {
            return has_value() ? **this : static_cast<T>(std::forward<U>(v));
        }

        template<class U>
        T value_or(U&& v) && {
            return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
        }

        template<class G = E>
        E error_or(G&& e) const& {
            return has_value() ? static_cast<E>(std::forward<G>(e)) : error();
        }

        template<class G = E>
        E error_or(G&& e) && {
            return has_value() ? static_cast<E>(std::forward<G>(e)) : std::move(error());
        }

        // The monadic operations: f on the value, an expected with the
        // same error type (and_then); f on the error, an expected with
        // the same value type (or_else); f's result as the new value
        // (transform) or error (transform_error)
        template<class F> auto and_then(F&& f) & { return _and_then(*this, std::forward<F>(f)); }
        template<class F> auto and_then(F&& f) const& { return _and_then(*this, std::forward<F>(f)); }
        template<class F> auto and_then(F&& f) && { return _and_then(std::move(*this), std::forward<F>(f)); }
        template<class F> auto and_then(F&& f) const&& { return _and_then(std::move(*this), std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) & { return _or_else(*this, std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) const& { return _or_else(*this, std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) && { return _or_else(std::move(*this), std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) const&& { return _or_else(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform(F&& f) & { return _transform(*this, std::forward<F>(f)); }
        template<class F> auto transform(F&& f) const& { return _transform(*this, std::forward<F>(f)); }
        template<class F> auto transform(F&& f) && { return _transform(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform(F&& f) const&& { return _transform(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) & { return _transform_error(*this, std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) const& { return _transform_error(*this, std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) && { return _transform_error(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) const&& { return _transform_error(std::move(*this), std::forward<F>(f)); }

        template<class T2, class E2>
        requires (!std::is_void_v<T2>)
        friend bool operator==(const expected& x, const expected<T2, E2>& y) {
            if (x.has_value() != y.has_value()) {
                return false;
            }
            return x.has_value() ? *x == *y : x.error() == y.error();
        }

        template<class T2>
        requires (!detail::IsExpected<T2>::value) && (!detail::IsUnexpected<T2>::value)
        friend bool operator==(const expected& x, const T2& v) {
            return x.has_value() && *x == v;
        }

        template<class E2>
        friend bool operator==(const expected& x, const unexpected<E2>& e) {
            return !x.has_value() && x.error() == e.error();
        }

    private:
        template<class Self, class F>
        static auto _and_then(Self&& self, F&& f) {
            using U = std::remove_cvref_t<std::invoke_result_t<F, decltype(*std::forward<Self>(self))>>;
            static_assert(detail::IsExpected<U>::value && std::is_same_v<typename U::error_type, E>, "and_then's function returns an expected with the same error type");
            if (self.has_value()) {
                return std::invoke(std::forward<F>(f), *std::forward<Self>(self));
            }
            return U(unexpect, std::forward<Self>(self).error());
        }

        template<class Self, class F>
        static auto _or_else(Self&& self, F&& f) {
            using G = std::remove_cvref_t<std::invoke_result_t<F, decltype(std::forward<Self>(self).error())>>;
            static_assert(detail::IsExpected<G>::value && std::is_same_v<typename G::value_type, T>, "or_else's function returns an expected with the same value type");
            if (self.has_value()) {
                return G(std::in_place, *std::forward<Self>(self));
            }
            return std::invoke(std::forward<F>(f), std::forward<Self>(self).error());
        }

        template<class Self, class F>
        static auto _transform(Self&& self, F&& f) {
            using U = std::remove_cv_t<std::invoke_result_t<F, decltype(*std::forward<Self>(self))>>;
            if constexpr(std::is_void_v<U>) {
                if (self.has_value()) {
                    std::invoke(std::forward<F>(f), *std::forward<Self>(self));
                    return expected<void, E>();
                }
                return expected<void, E>(unexpect, std::forward<Self>(self).error());
            } else {
                if (self.has_value()) {
                    return expected<U, E>(std::in_place, std::invoke(std::forward<F>(f), *std::forward<Self>(self)));
                }
                return expected<U, E>(unexpect, std::forward<Self>(self).error());
            }
        }

        template<class Self, class F>
        static auto _transform_error(Self&& self, F&& f) {
            using G = std::remove_cv_t<std::invoke_result_t<F, decltype(std::forward<Self>(self).error())>>;
            if (self.has_value()) {
                return expected<T, G>(std::in_place, *std::forward<Self>(self));
            }
            return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), std::forward<Self>(self).error()));
        }

        Storage _s;
    };

    // expected<void, E>: a success without a value, or an error
    template<class T, class E>
    requires std::is_void_v<T>
    class expected<T, E> {
        static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E> && !std::is_volatile_v<E> && !detail::IsUnexpected<E>::value, "the error type of an expected is a plain object type");

        using Storage = variant<monostate, E>;

    public:
        using value_type = T;
        using error_type = E;
        using unexpected_type = unexpected<E>;

        template<class U>
        using rebind = expected<U, error_type>;

        expected() noexcept
        : _s(std::in_place_index<0>) {
        }

        expected(const expected&) = default;
        expected(expected&&) = default;

        template<class U, class G>
        requires std::is_void_v<U> && std::is_constructible_v<E, const G&>
        explicit(!std::is_convertible_v<const G&, E>)
        expected(const expected<U, G>& o)
        : _s(o.has_value() ? Storage(std::in_place_index<0>) : Storage(std::in_place_index<1>, o.error())) {
        }

        template<class U, class G>
        requires std::is_void_v<U> && std::is_constructible_v<E, G>
        explicit(!std::is_convertible_v<G, E>)
        expected(expected<U, G>&& o)
        : _s(o.has_value() ? Storage(std::in_place_index<0>) : Storage(std::in_place_index<1>, std::move(o.error()))) {
        }

        template<class G>
        requires std::is_constructible_v<E, const G&>
        explicit(!std::is_convertible_v<const G&, E>)
        expected(const unexpected<G>& u)
        : _s(std::in_place_index<1>, u.error()) {
        }

        template<class G>
        requires std::is_constructible_v<E, G>
        explicit(!std::is_convertible_v<G, E>)
        expected(unexpected<G>&& u)
        : _s(std::in_place_index<1>, std::move(u.error())) {
        }

        explicit expected(std::in_place_t) noexcept
        : _s(std::in_place_index<0>) {
        }

        template<class... A>
        requires std::is_constructible_v<E, A...>
        explicit expected(unexpect_t, A&&... a)
        : _s(std::in_place_index<1>, std::forward<A>(a)...) {
        }

        template<class U, class... A>
        requires std::is_constructible_v<E, std::initializer_list<U>&, A...>
        explicit expected(unexpect_t, std::initializer_list<U> il, A&&... a)
        : _s(std::in_place_index<1>, il, std::forward<A>(a)...) {
        }

        expected& operator=(const expected&) = default;
        expected& operator=(expected&&) = default;

        template<class G>
        requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>
        expected& operator=(const unexpected<G>& u) {
            if (has_value()) {
                _s.template emplace<1>(u.error());
            } else {
                get<1>(_s) = u.error();
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
        expected& operator=(unexpected<G>&& u) {
            if (has_value()) {
                _s.template emplace<1>(std::move(u.error()));
            } else {
                get<1>(_s) = std::move(u.error());
            }
            return *this;
        }

        void emplace() noexcept {
            _s.template emplace<0>();
        }

        void swap(expected& o) noexcept(noexcept(std::declval<Storage&>().swap(std::declval<Storage&>()))) {
            _s.swap(o._s);
        }

        friend void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y))) {
            x.swap(y);
        }

        void operator*() const noexcept {
        }

        explicit operator bool() const noexcept {
            return has_value();
        }

        bool has_value() const noexcept {
            return _s.index() == 0;
        }

        void value() const& {
            if (!has_value()) {
                throw bad_expected_access<std::decay_t<E>>(error());
            }
        }

        void value() && {
            if (!has_value()) {
                throw bad_expected_access<std::decay_t<E>>(std::move(error()));
            }
        }

        const E& error() const& noexcept { return get<1>(_s); }
        E& error() & noexcept { return get<1>(_s); }
        const E&& error() const&& noexcept { return std::move(get<1>(_s)); }
        E&& error() && noexcept { return std::move(get<1>(_s)); }

        template<class G = E>
        E error_or(G&& e) const& {
            return has_value() ? static_cast<E>(std::forward<G>(e)) : error();
        }

        template<class G = E>
        E error_or(G&& e) && {
            return has_value() ? static_cast<E>(std::forward<G>(e)) : std::move(error());
        }

        template<class F> auto and_then(F&& f) & { return _and_then(*this, std::forward<F>(f)); }
        template<class F> auto and_then(F&& f) const& { return _and_then(*this, std::forward<F>(f)); }
        template<class F> auto and_then(F&& f) && { return _and_then(std::move(*this), std::forward<F>(f)); }
        template<class F> auto and_then(F&& f) const&& { return _and_then(std::move(*this), std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) & { return _or_else(*this, std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) const& { return _or_else(*this, std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) && { return _or_else(std::move(*this), std::forward<F>(f)); }
        template<class F> auto or_else(F&& f) const&& { return _or_else(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform(F&& f) & { return _transform(*this, std::forward<F>(f)); }
        template<class F> auto transform(F&& f) const& { return _transform(*this, std::forward<F>(f)); }
        template<class F> auto transform(F&& f) && { return _transform(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform(F&& f) const&& { return _transform(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) & { return _transform_error(*this, std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) const& { return _transform_error(*this, std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) && { return _transform_error(std::move(*this), std::forward<F>(f)); }
        template<class F> auto transform_error(F&& f) const&& { return _transform_error(std::move(*this), std::forward<F>(f)); }

        template<class T2, class E2>
        requires std::is_void_v<T2>
        friend bool operator==(const expected& x, const expected<T2, E2>& y) {
            if (x.has_value() != y.has_value()) {
                return false;
            }
            return x.has_value() || x.error() == y.error();
        }

        template<class E2>
        friend bool operator==(const expected& x, const unexpected<E2>& e) {
            return !x.has_value() && x.error() == e.error();
        }

    private:
        template<class Self, class F>
        static auto _and_then(Self&& self, F&& f) {
            using U = std::remove_cvref_t<std::invoke_result_t<F>>;
            static_assert(detail::IsExpected<U>::value && std::is_same_v<typename U::error_type, E>, "and_then's function returns an expected with the same error type");
            if (self.has_value()) {
                return std::invoke(std::forward<F>(f));
            }
            return U(unexpect, std::forward<Self>(self).error());
        }

        template<class Self, class F>
        static auto _or_else(Self&& self, F&& f) {
            using G = std::remove_cvref_t<std::invoke_result_t<F, decltype(std::forward<Self>(self).error())>>;
            static_assert(detail::IsExpected<G>::value && std::is_void_v<typename G::value_type>, "or_else's function returns an expected<void, G>");
            if (self.has_value()) {
                return G();
            }
            return std::invoke(std::forward<F>(f), std::forward<Self>(self).error());
        }

        template<class Self, class F>
        static auto _transform(Self&& self, F&& f) {
            using U = std::remove_cv_t<std::invoke_result_t<F>>;
            if constexpr(std::is_void_v<U>) {
                if (self.has_value()) {
                    std::invoke(std::forward<F>(f));
                    return expected<void, E>();
                }
                return expected<void, E>(unexpect, std::forward<Self>(self).error());
            } else {
                if (self.has_value()) {
                    return expected<U, E>(std::in_place, std::invoke(std::forward<F>(f)));
                }
                return expected<U, E>(unexpect, std::forward<Self>(self).error());
            }
        }

        template<class Self, class F>
        static auto _transform_error(Self&& self, F&& f) {
            using G = std::remove_cv_t<std::invoke_result_t<F, decltype(std::forward<Self>(self).error())>>;
            if (self.has_value()) {
                return expected<void, G>();
            }
            return expected<void, G>(unexpect, std::invoke(std::forward<F>(f), std::forward<Self>(self).error()));
        }

        Storage _s;
    };
}
