//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Variant<Ts...>: one of the types, a Ptr among them
// traced. GetIf<T>() is the value as a pointer, null when it is another
// type; Get<T>() the reference (std::bad_variant_access otherwise).
#pragma once

#include "../../core/variant.h"

namespace Sgcl {
    template<class... Ts>
    class Variant {
    public:
        using InnerType = sgcl::variant<Ts...>;

        static constexpr size_t Count = sizeof...(Ts);

        Variant() = default;
        Variant(const Variant&) = default;
        Variant(Variant&&) = default;
        Variant& operator=(const Variant&) = default;
        Variant& operator=(Variant&&) = default;

        template<class U>
        requires (!std::is_same_v<std::remove_cvref_t<U>, Variant>) && (!std::is_same_v<std::remove_cvref_t<U>, InnerType>) && std::is_constructible_v<InnerType, U&&>
        Variant(U&& u)
        : _v(std::forward<U>(u)) {
        }

        template<class T, class... A>
        explicit Variant(std::in_place_type_t<T> t, A&&... a)
        : _v(t, std::forward<A>(a)...) {
        }

        template<size_t I, class... A>
        explicit Variant(std::in_place_index_t<I> i, A&&... a)
        : _v(i, std::forward<A>(a)...) {
        }

        template<class I>
        requires std::is_same_v<std::remove_cvref_t<I>, InnerType>
        explicit Variant(I&& v)   // a template, as Any's: no conversion of a Variant to sgcl::variant tried
        : _v(std::forward<I>(v)) {
        }

        template<class U>
        requires (!std::is_same_v<std::remove_cvref_t<U>, Variant>) && (!std::is_same_v<std::remove_cvref_t<U>, InnerType>) && std::is_assignable_v<InnerType&, U&&>
        Variant& operator=(U&& u) {
            _v = std::forward<U>(u);
            return *this;
        }

        template<class T, class... A>
        T& Emplace(A&&... a) {
            return _v.template emplace<T>(std::forward<A>(a)...);
        }

        template<size_t I, class... A>
        decltype(auto) Emplace(A&&... a) {
            return _v.template emplace<I>(std::forward<A>(a)...);
        }

        // Which of the types is held
        size_t Index() const noexcept {
            return _v.index();
        }

        template<class T>
        bool Is() const noexcept {
            return sgcl::holds_alternative<T>(_v);
        }

        bool IsValueless() const noexcept {
            return _v.valueless_by_exception();
        }

        template<class T>
        T& Get() {
            return sgcl::get<T>(_v);
        }

        template<class T>
        const T& Get() const {
            return sgcl::get<T>(_v);
        }

        template<size_t I>
        decltype(auto) Get() {
            return sgcl::get<I>(_v);
        }

        template<size_t I>
        decltype(auto) Get() const {
            return sgcl::get<I>(_v);
        }

        template<class T>
        T* GetIf() noexcept {
            return sgcl::get_if<T>(&_v);
        }

        template<class T>
        const T* GetIf() const noexcept {
            return sgcl::get_if<T>(&_v);
        }

        template<size_t I>
        auto GetIf() noexcept {
            return sgcl::get_if<I>(&_v);
        }

        // f called with the value held, whichever type it is
        template<class F>
        decltype(auto) Visit(F&& f) {
            return sgcl::visit(std::forward<F>(f), _v);
        }

        template<class F>
        decltype(auto) Visit(F&& f) const {
            return sgcl::visit(std::forward<F>(f), _v);
        }

        void Swap(Variant& o) noexcept(noexcept(std::declval<InnerType&>().swap(std::declval<InnerType&>()))) {
            _v.swap(o._v);
        }

        InnerType& Inner() noexcept {
            return _v;
        }

        const InnerType& Inner() const noexcept {
            return _v;
        }

        friend bool operator==(const Variant& a, const Variant& b) {
            return a._v == b._v;
        }

        friend auto operator<=>(const Variant& a, const Variant& b) {
            return a._v <=> b._v;
        }

    private:
        InnerType _v;
    };

    template<class... Ts>
    void swap(Variant<Ts...>& a, Variant<Ts...>& b) noexcept(noexcept(a.Swap(b))) {
        a.Swap(b);
    }

    template<class F, class... Vs>
    decltype(auto) Visit(F&& f, Vs&&... vs) {
        return sgcl::visit(std::forward<F>(f), std::forward<Vs>(vs).Inner()...);
    }
}

template<class... Ts>
struct std::hash<Sgcl::Variant<Ts...>> {
    size_t operator()(const Sgcl::Variant<Ts...>& v) const {
        return std::hash<sgcl::variant<Ts...>>()(v.Inner());
    }
};

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

