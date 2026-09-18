//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "make_tracked.h"
#include "root_ptr.h"
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

    // The error the exception carries lives in a managed object held by a
    // root_ptr: an exception object is unmanaged memory (the runtime
    // allocates it), so an error holding a tracked pointer (a string, a
    // tracked_ptr) could not lie in it directly; a root_ptr may, and keeps
    // the error alive for as long as the exception exists, through the
    // copies the runtime makes of it. One managed allocation per throw.
    template<class E>
    class bad_expected_access : public bad_expected_access<void> {
    public:
        explicit bad_expected_access(E e)
        : _error(make_tracked<E>(std::move(e))) {
        }

        const E& error() const& noexcept { return *_error; }
        E& error() & noexcept { return *_error; }
        const E&& error() const&& noexcept { return std::move(*_error); }
        E&& error() && noexcept { return std::move(*_error); }

    private:
        root_ptr<E> _error;
    };

    // An expected with the interface of std::expected, safe to hold a
    // tracked pointer as its value or its error: std::expected keeps the
    // two in a union, where a pointer shares its word with the other's
    // data (README: Pointer maps); here they lie in a variant (variant.h),
    // laid out by what they hold. The interface of std::expected (C++23):
    // the constructors, in_place and unexpect, emplace, swap, operator->
    // and *, has_value, value (bad_expected_access<E> without one), error,
    // value_or, error_or, and_then, or_else, transform, transform_error,
    // the comparisons; not constexpr. Lives where its variant may: where
    // a tracked_ptr may.
    template<class T, class E>
    class expected {
        static_assert(!std::is_reference_v<T> && !std::is_function_v<T> && !std::is_same_v<std::remove_cv_t<T>, std::in_place_t> && !std::is_same_v<std::remove_cv_t<T>, unexpect_t> && !detail::IsUnexpected<std::remove_cv_t<T>>::value, "the value type of an expected is an object type, not in_place_t, unexpect_t or an unexpected");
        static_assert(std::is_object_v<E> && !std::is_array_v<E> && !std::is_const_v<E> && !std::is_volatile_v<E> && !detail::IsUnexpected<E>::value, "the error type of an expected is a plain object type");

        using Storage = variant<T, E>;

        // A bool value is not constructed from an expected<U, G> as a
        // whole (LWG 3836): every expected converts to bool, which would
        // make expected<bool, int>(expected<int, int>(0)) true
        static constexpr bool is_bool = std::is_same_v<std::remove_cv_t<T>, bool>;

        template<class U, class G>
        static constexpr bool converts_from_other = (!is_bool && (std::is_constructible_v<T, expected<U, G>&> || std::is_constructible_v<T, expected<U, G>> || std::is_constructible_v<T, const expected<U, G>&> || std::is_constructible_v<T, const expected<U, G>>
            || std::is_convertible_v<expected<U, G>&, T> || std::is_convertible_v<expected<U, G>&&, T> || std::is_convertible_v<const expected<U, G>&, T> || std::is_convertible_v<const expected<U, G>&&, T>))
            || std::is_constructible_v<unexpected<E>, expected<U, G>&> || std::is_constructible_v<unexpected<E>, expected<U, G>> || std::is_constructible_v<unexpected<E>, const expected<U, G>&> || std::is_constructible_v<unexpected<E>, const expected<U, G>>;

        // An assignment that replaces the value with the error or the
        // other way round never leaves the expected without either
        // (std's rule): the new one is constructed without throwing, or
        // moved without throwing from a temporary, or the old one is
        // moved out without throwing and put back if the construction
        // throws. So one of the three must hold.
        template<class New, class... A>
        static constexpr bool reinit_safe = std::is_nothrow_constructible_v<New, A...> || std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>;

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
        requires (!std::is_same_v<std::remove_cvref_t<U>, std::in_place_t>) && (!std::is_same_v<std::remove_cvref_t<U>, expected>) && (!detail::IsUnexpected<std::remove_cvref_t<U>>::value) && (!is_bool || !detail::IsExpected<std::remove_cvref_t<U>>::value) && std::is_constructible_v<T, U>
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

        // The assignments as std::expected's: the one held assigned when
        // both sides hold the same kind, the other kind put in its place
        // through _reinit otherwise, so that the expected always holds a
        // value or an error, never nothing (the variant alone would be
        // valueless after a constructor that throws)
        expected& operator=(const expected& o)
        requires std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> && std::is_copy_constructible_v<E> && std::is_copy_assignable_v<E> && (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>) {
            if (has_value()) {
                if (o.has_value()) {
                    get<0>(_s) = *o;
                } else {
                    _reinit<1>(o.error());
                }
            } else if (o.has_value()) {
                _reinit<0>(*o);
            } else {
                get<1>(_s) = o.error();
            }
            return *this;
        }

        expected& operator=(expected&& o) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<E>)
        requires std::is_move_constructible_v<T> && std::is_move_assignable_v<T> && std::is_move_constructible_v<E> && std::is_move_assignable_v<E> && (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>) {
            if (has_value()) {
                if (o.has_value()) {
                    get<0>(_s) = std::move(*o);
                } else {
                    _reinit<1>(std::move(o.error()));
                }
            } else if (o.has_value()) {
                _reinit<0>(std::move(*o));
            } else {
                get<1>(_s) = std::move(o.error());
            }
            return *this;
        }

        template<class U = T>
        requires (!std::is_same_v<std::remove_cvref_t<U>, expected>) && (!detail::IsUnexpected<std::remove_cvref_t<U>>::value) && std::is_constructible_v<T, U> && std::is_assignable_v<T&, U> && reinit_safe<T, U>
        expected& operator=(U&& v) {
            if (has_value()) {
                get<0>(_s) = std::forward<U>(v);
            } else {
                _reinit<0>(std::forward<U>(v));
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&> && reinit_safe<E, const G&>
        expected& operator=(const unexpected<G>& u) {
            if (has_value()) {
                _reinit<1>(u.error());
            } else {
                get<1>(_s) = u.error();
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G> && reinit_safe<E, G>
        expected& operator=(unexpected<G>&& u) {
            if (has_value()) {
                _reinit<1>(std::move(u.error()));
            } else {
                get<1>(_s) = std::move(u.error());
            }
            return *this;
        }

        // A construction that cannot throw only: the one held is destroyed
        // first, and nothing could be put back
        template<class... A>
        requires std::is_nothrow_constructible_v<T, A...>
        T& emplace(A&&... a) noexcept {
            return _s.template emplace<0>(std::forward<A>(a)...);
        }

        template<class U, class... A>
        requires std::is_nothrow_constructible_v<T, std::initializer_list<U>&, A...>
        T& emplace(std::initializer_list<U> il, A&&... a) noexcept {
            return _s.template emplace<0>(il, std::forward<A>(a)...);
        }

        // std's swap: the two of one kind swapped; a value against an error
        // through a temporary of the one that moves without throwing, the
        // other side put back as it was if a move throws
        void swap(expected& o) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T> && std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
        requires std::is_swappable_v<T> && std::is_swappable_v<E> && std::is_move_constructible_v<T> && std::is_move_constructible_v<E> && (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>) {
            using std::swap;
            if (has_value()) {
                if (o.has_value()) {
                    swap(get<0>(_s), get<0>(o._s));
                } else if constexpr(std::is_nothrow_move_constructible_v<E>) {
                    E tmp(std::move(o.error()));
                    try {
                        o._s.template emplace<0>(std::move(get<0>(_s)));
                    } catch (...) {
                        o._s.template emplace<1>(std::move(tmp));
                        throw;
                    }
                    _s.template emplace<1>(std::move(tmp));
                } else {
                    T tmp(std::move(get<0>(_s)));
                    try {
                        _s.template emplace<1>(std::move(o.error()));
                    } catch (...) {
                        _s.template emplace<0>(std::move(tmp));
                        throw;
                    }
                    o._s.template emplace<0>(std::move(tmp));
                }
            } else if (o.has_value()) {
                o.swap(*this);
            } else {
                swap(get<1>(_s), get<1>(o._s));
            }
        }

        friend void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y)))
        requires requires { x.swap(y); } {
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

        // Alternative I constructed in place of the other one, std's
        // reinit-expected: in place when its construction cannot throw;
        // from a temporary when it moves without throwing; otherwise the
        // old alternative moved out first (it moves without throwing:
        // reinit_safe) and put back if the construction throws, so the
        // expected is never left valueless
        template<size_t I, class... A>
        void _reinit(A&&... a) {
            using New = variant_alternative_t<I, Storage>;
            using Old = variant_alternative_t<1 - I, Storage>;
            if constexpr(std::is_nothrow_constructible_v<New, A...>) {
                _s.template emplace<I>(std::forward<A>(a)...);
            } else if constexpr(std::is_nothrow_move_constructible_v<New>) {
                New tmp(std::forward<A>(a)...);
                _s.template emplace<I>(std::move(tmp));
            } else {
                static_assert(std::is_nothrow_move_constructible_v<Old>);
                Old tmp(std::move(get<1 - I>(_s)));
                try {
                    _s.template emplace<I>(std::forward<A>(a)...);
                } catch (...) {
                    _s.template emplace<1 - I>(std::move(tmp));
                    throw;
                }
            }
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

        // The assignments as std::expected<void, E>'s: a success is always
        // there to fall back on, so an error whose construction throws
        // leaves a success behind (_error), never nothing
        expected& operator=(const expected& o)
        requires std::is_copy_constructible_v<E> && std::is_copy_assignable_v<E> {
            if (o.has_value()) {
                emplace();
            } else if (has_value()) {
                _error(o.error());
            } else {
                get<1>(_s) = o.error();
            }
            return *this;
        }

        expected& operator=(expected&& o) noexcept(std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<E>)
        requires std::is_move_constructible_v<E> && std::is_move_assignable_v<E> {
            if (o.has_value()) {
                emplace();
            } else if (has_value()) {
                _error(std::move(o.error()));
            } else {
                get<1>(_s) = std::move(o.error());
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>
        expected& operator=(const unexpected<G>& u) {
            if (has_value()) {
                _error(u.error());
            } else {
                get<1>(_s) = u.error();
            }
            return *this;
        }

        template<class G>
        requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
        expected& operator=(unexpected<G>&& u) {
            if (has_value()) {
                _error(std::move(u.error()));
            } else {
                get<1>(_s) = std::move(u.error());
            }
            return *this;
        }

        void emplace() noexcept {
            _s.template emplace<0>();
        }

        // std's swap: the errors swapped, or the one error moved to the
        // other side (that side still a success if the move throws)
        void swap(expected& o) noexcept(std::is_nothrow_move_constructible_v<E> && std::is_nothrow_swappable_v<E>)
        requires std::is_swappable_v<E> && std::is_move_constructible_v<E> {
            if (has_value()) {
                if (!o.has_value()) {
                    _error(std::move(o.error()));
                    o.emplace();
                }
            } else if (o.has_value()) {
                o.swap(*this);
            } else {
                using std::swap;
                swap(get<1>(_s), get<1>(o._s));
            }
        }

        friend void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y)))
        requires requires { x.swap(y); } {
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

        // The error constructed in place of the success; the success put
        // back (it costs nothing) if the construction throws
        template<class... A>
        void _error(A&&... a) {
            try {
                _s.template emplace<1>(std::forward<A>(a)...);
            } catch (...) {
                _s.template emplace<0>();
                throw;
            }
        }

        Storage _s;
    };
}
