//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/pointer_word.h"

#include <array>
#include <compare>
#include <concepts>
#include <exception>
#include <functional>
#include <initializer_list>
#include <utility>
#include <variant>

namespace sgcl {
    using std::bad_variant_access;
    using std::monostate;
    using std::variant_npos;

    template<class... Ts>
    class variant;

    template<class T>
    struct variant_size;

    template<class... Ts>
    struct variant_size<variant<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

    template<class T>
    struct variant_size<const T> : variant_size<T> {};

    template<class T>
    inline constexpr size_t variant_size_v = variant_size<T>::value;

    template<size_t I, class T>
    struct variant_alternative;

    template<size_t I, class... Ts>
    struct variant_alternative<I, variant<Ts...>> {
        static_assert(I < sizeof...(Ts), "the index is out of the variant's alternatives");
        using type = std::tuple_element_t<I, std::tuple<Ts...>>;
    };

    template<size_t I, class T>
    struct variant_alternative<I, const T> {
        using type = std::add_const_t<typename variant_alternative<I, T>::type>;
    };

    template<size_t I, class T>
    using variant_alternative_t = typename variant_alternative<I, T>::type;

    namespace detail {
        // The layout of a variant's alternatives over its two storages:
        // every pointer word (IsPointerWord) at offset 0 of the tracked
        // storage, one word shared by all of them; every other type that
        // may hold a pointer at an offset of its own after it; the types
        // that cannot hold one over the data storage, all at offset 0.
        template<class... Ts>
        struct VariantLayout {
            static constexpr size_t N = sizeof...(Ts);
            static constexpr Region regions[N] = {region_of<Ts>()...};
            static constexpr size_t sizes[N] = {sizeof(Ts)...};
            static constexpr size_t aligns[N] = {alignof(Ts)...};

            struct Result {
                std::array<size_t, N> offsets = {};
                size_t tracked_size = 0;
                size_t tracked_align = 1;
                size_t data_size = 0;
                size_t data_align = 1;
            };

            static constexpr Result compute() noexcept {
                Result r;
                size_t word = 0;
                for (size_t i = 0; i < N; ++i) {
                    if (regions[i] == Region::Word) {
                        word = sizes[i] > word ? sizes[i] : word;
                        r.tracked_align = aligns[i] > r.tracked_align ? aligns[i] : r.tracked_align;
                    }
                }
                size_t cursor = word;
                for (size_t i = 0; i < N; ++i) {
                    if (regions[i] == Region::Word) {
                        r.offsets[i] = 0;
                    } else if (regions[i] == Region::Tracked) {
                        cursor = (cursor + aligns[i] - 1) / aligns[i] * aligns[i];
                        r.offsets[i] = cursor;
                        cursor += sizes[i];
                        r.tracked_align = aligns[i] > r.tracked_align ? aligns[i] : r.tracked_align;
                    } else {
                        r.offsets[i] = 0;
                        r.data_size = sizes[i] > r.data_size ? sizes[i] : r.data_size;
                        r.data_align = aligns[i] > r.data_align ? aligns[i] : r.data_align;
                    }
                }
                r.tracked_size = (cursor + r.tracked_align - 1) / r.tracked_align * r.tracked_align;
                return r;
            }

            static constexpr Result result = compute();
        };

        // The alternative a converting construction or assignment selects:
        // the overload resolution of std::variant, an imaginary function
        // per alternative that takes it, only when Ti x[] = {u} would
        // not narrow
        template<size_t I, class Ti>
        struct VariantOverload {
            template<class U>
            requires requires { std::type_identity_t<Ti[]>{std::declval<U>()}; }
            static std::integral_constant<size_t, I> select(Ti);
        };

        template<class Seq, class... Ts>
        struct VariantOverloads;

        template<size_t... Is, class... Ts>
        struct VariantOverloads<std::index_sequence<Is...>, Ts...> : VariantOverload<Is, Ts>... {
            using VariantOverload<Is, Ts>::select...;
        };

        template<class U, class... Ts>
        using VariantSelected = decltype(VariantOverloads<std::index_sequence_for<Ts...>, Ts...>::template select<U>(std::declval<U>()));

        template<class T>
        struct IsInPlace : std::false_type {};

        template<class T>
        struct IsInPlace<std::in_place_type_t<T>> : std::true_type {};

        template<size_t I>
        struct IsInPlace<std::in_place_index_t<I>> : std::true_type {};

        // A call of f with the index as a compile-time constant, through a
        // table of one function per index
        template<class F, size_t... Is>
        decltype(auto) variant_dispatch(size_t index, F&& f, std::index_sequence<Is...>) {
            using R = decltype(std::forward<F>(f)(std::integral_constant<size_t, 0>()));
            constexpr R (*table[])(F&&) = {[](F&& f) -> R { return std::forward<F>(f)(std::integral_constant<size_t, Is>()); }...};
            return table[index](std::forward<F>(f));
        }
    }

    // A variant with the interface of std::variant, safe to hold tracked
    // pointers next to other alternatives. std::variant keeps every
    // alternative at the same offset, so a tracked_ptr shares its word
    // with the data of the others; the collector's pointer map, built by
    // elimination, finds data there in some object and drops the offset
    // for good, and the pointer is no longer followed (README: Pointer
    // maps). Here the alternatives are laid out by what they hold
    // (detail/pointer_word.h): the pointer words (tracked_ptr of either
    // kind, weak_ptr) share one word that holds null or an address and
    // nothing else, an alternative that may hold pointers among its data
    // gets a place of its own, and the alternatives that cannot hold a
    // pointer share the data storage. The interface is that of
    // std::variant (the constructors and their overload resolution,
    // emplace, index, valueless_by_exception, get, get_if,
    // holds_alternative, visit, swap, the comparisons, hash, variant_size
    // and variant_alternative), without constexpr: the alternatives live
    // in raw storage. A variant is never trivially copyable, and the
    // variant of pointer-free alternatives is what std::variant is for.
    // Lives where its alternatives may: where a tracked_ptr may with
    // sgcl::tracked_ptr alternatives, anywhere with gc::tracked_ptr ones.
    template<class... Ts>
    class variant {
        static_assert(sizeof...(Ts) > 0, "a variant has at least one alternative");
        static_assert((!std::is_reference_v<Ts> && ...), "a variant holds no references");
        static_assert((!std::is_array_v<Ts> && ...), "a variant holds no arrays");
        static_assert((std::is_object_v<Ts> && ...) && (std::is_destructible_v<Ts> && ...), "an alternative is a destructible object type");

        using Layout = detail::VariantLayout<Ts...>;
        static constexpr auto L = Layout::result;
        static constexpr size_t N = sizeof...(Ts);
        using Index = std::conditional_t<(N < 255), unsigned char, size_t>;
        static constexpr Index Npos = Index(-1);

        template<size_t I>
        using Alt = std::tuple_element_t<I, std::tuple<Ts...>>;

        template<class T>
        static constexpr size_t index_of = [] {
            constexpr bool same[] = {std::is_same_v<T, Ts>...};
            size_t index = variant_npos;
            size_t count = 0;
            for (size_t i = 0; i < N; ++i) {
                if (same[i]) {
                    index = i;
                    ++count;
                }
            }
            return count == 1 ? index : variant_npos;
        }();

        template<class U>
        static constexpr bool convertible = !std::is_same_v<std::remove_cvref_t<U>, variant> && !detail::IsInPlace<std::remove_cvref_t<U>>::value && requires { typename detail::VariantSelected<U, Ts...>; };

        template<class U>
        static constexpr size_t selected = detail::VariantSelected<U, Ts...>::value;

    public:
        variant() noexcept(std::is_nothrow_default_constructible_v<Alt<0>>)
        requires std::is_default_constructible_v<Alt<0>> {
            _construct<0>();
        }

        variant(const variant& o)
        requires (std::is_copy_constructible_v<Ts> && ...) {
            _construct_from(o);
        }

        variant(variant&& o) noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
        requires (std::is_move_constructible_v<Ts> && ...) {
            _construct_from(std::move(o));
        }

        // From a value: the alternative std::variant's overload resolution
        // selects for it
        template<class U>
        requires convertible<U> && std::is_constructible_v<Alt<selected<U>>, U>
        variant(U&& u) noexcept(std::is_nothrow_constructible_v<Alt<selected<U>>, U>) {
            _construct<selected<U>>(std::forward<U>(u));
        }

        template<class T, class... A>
        requires (index_of<T> != variant_npos) && std::is_constructible_v<T, A...>
        explicit variant(std::in_place_type_t<T>, A&&... a) {
            _construct<index_of<T>>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A>
        requires (index_of<T> != variant_npos) && std::is_constructible_v<T, std::initializer_list<U>&, A...>
        explicit variant(std::in_place_type_t<T>, std::initializer_list<U> il, A&&... a) {
            _construct<index_of<T>>(il, std::forward<A>(a)...);
        }

        template<size_t I, class... A>
        requires (I < N) && std::is_constructible_v<Alt<I>, A...>
        explicit variant(std::in_place_index_t<I>, A&&... a) {
            _construct<I>(std::forward<A>(a)...);
        }

        template<size_t I, class U, class... A>
        requires (I < N) && std::is_constructible_v<Alt<I>, std::initializer_list<U>&, A...>
        explicit variant(std::in_place_index_t<I>, std::initializer_list<U> il, A&&... a) {
            _construct<I>(il, std::forward<A>(a)...);
        }

        ~variant() {
            _destroy();
        }

        variant& operator=(const variant& o)
        requires ((std::is_copy_constructible_v<Ts> && std::is_copy_assignable_v<Ts>) && ...) {
            if (this == &o) {
                return *this;
            }
            if (o.valueless_by_exception()) {
                _destroy();
                return *this;
            }
            detail::variant_dispatch(o._index, [&]<size_t I>(std::integral_constant<size_t, I>) {
                if (_index == I) {
                    _get<I>() = o._get<I>();
                } else if constexpr(std::is_nothrow_copy_constructible_v<Alt<I>> || !std::is_nothrow_move_constructible_v<Alt<I>>) {
                    emplace<I>(o._get<I>());
                } else {
                    emplace<I>(Alt<I>(o._get<I>()));
                }
            }, std::make_index_sequence<N>());
            return *this;
        }

        variant& operator=(variant&& o) noexcept(((std::is_nothrow_move_constructible_v<Ts> && std::is_nothrow_move_assignable_v<Ts>) && ...))
        requires ((std::is_move_constructible_v<Ts> && std::is_move_assignable_v<Ts>) && ...) {
            if (this == &o) {
                return *this;
            }
            if (o.valueless_by_exception()) {
                _destroy();
                return *this;
            }
            detail::variant_dispatch(o._index, [&]<size_t I>(std::integral_constant<size_t, I>) {
                if (_index == I) {
                    _get<I>() = std::move(o._get<I>());
                } else {
                    emplace<I>(std::move(o._get<I>()));
                }
            }, std::make_index_sequence<N>());
            return *this;
        }

        template<class U>
        requires convertible<U> && std::is_constructible_v<Alt<selected<U>>, U> && std::is_assignable_v<Alt<selected<U>>&, U>
        variant& operator=(U&& u) noexcept(std::is_nothrow_constructible_v<Alt<selected<U>>, U> && std::is_nothrow_assignable_v<Alt<selected<U>>&, U>) {
            constexpr size_t J = selected<U>;
            if (_index == J) {
                _get<J>() = std::forward<U>(u);
            } else if constexpr(std::is_nothrow_constructible_v<Alt<J>, U> || !std::is_nothrow_move_constructible_v<Alt<J>>) {
                emplace<J>(std::forward<U>(u));
            } else {
                emplace<J>(Alt<J>(std::forward<U>(u)));
            }
            return *this;
        }

        template<class T, class... A>
        requires (index_of<T> != variant_npos) && std::is_constructible_v<T, A...>
        T& emplace(A&&... a) {
            return emplace<index_of<T>>(std::forward<A>(a)...);
        }

        template<class T, class U, class... A>
        requires (index_of<T> != variant_npos) && std::is_constructible_v<T, std::initializer_list<U>&, A...>
        T& emplace(std::initializer_list<U> il, A&&... a) {
            return emplace<index_of<T>>(il, std::forward<A>(a)...);
        }

        template<size_t I, class... A>
        requires (I < N) && std::is_constructible_v<Alt<I>, A...>
        Alt<I>& emplace(A&&... a) {
            _destroy();
            return _construct<I>(std::forward<A>(a)...);
        }

        template<size_t I, class U, class... A>
        requires (I < N) && std::is_constructible_v<Alt<I>, std::initializer_list<U>&, A...>
        Alt<I>& emplace(std::initializer_list<U> il, A&&... a) {
            _destroy();
            return _construct<I>(il, std::forward<A>(a)...);
        }

        bool valueless_by_exception() const noexcept {
            return _index == Npos;
        }

        size_t index() const noexcept {
            return _index == Npos ? variant_npos : _index;
        }

        void swap(variant& o) noexcept(((std::is_nothrow_move_constructible_v<Ts> && std::is_nothrow_swappable_v<Ts>) && ...))
        requires ((std::is_move_constructible_v<Ts> && std::is_swappable_v<Ts>) && ...) {
            if (valueless_by_exception() && o.valueless_by_exception()) {
                return;
            }
            if (_index == o._index) {
                detail::variant_dispatch(_index, [&]<size_t I>(std::integral_constant<size_t, I>) {
                    using std::swap;
                    swap(_get<I>(), o._get<I>());
                }, std::make_index_sequence<N>());
                return;
            }
            variant tmp(std::move(o));
            o._destroy();
            if (!valueless_by_exception()) {
                o._construct_from(std::move(*this));
            }
            _destroy();
            if (!tmp.valueless_by_exception()) {
                _construct_from(std::move(tmp));
            }
        }

    private:
        template<size_t I>
        void* _slot() noexcept {
            if constexpr(Layout::regions[I] == detail::Region::Data) {
                return _data.bytes;
            } else {
                return _tracked.bytes + L.offsets[I];
            }
        }

        template<size_t I>
        const void* _slot() const noexcept {
            return const_cast<variant*>(this)->_slot<I>();
        }

        template<size_t I>
        Alt<I>& _get() noexcept {
            return *std::launder(static_cast<Alt<I>*>(_slot<I>()));
        }

        template<size_t I>
        const Alt<I>& _get() const noexcept {
            return *std::launder(static_cast<const Alt<I>*>(_slot<I>()));
        }

        // The alternative constructed in its place; valueless while the
        // constructor runs, in case it throws
        template<size_t I, class... A>
        Alt<I>& _construct(A&&... a) {
            _index = Npos;
            auto p = ::new(_slot<I>()) Alt<I>(std::forward<A>(a)...);
            _index = I;
            return *p;
        }

        template<class V>
        void _construct_from(V&& o) {
            if (o.valueless_by_exception()) {
                _index = Npos;
                return;
            }
            detail::variant_dispatch(o._index, [&]<size_t I>(std::integral_constant<size_t, I>) {
                if constexpr(std::is_rvalue_reference_v<V&&>) {
                    _construct<I>(std::move(o.template _get<I>()));
                } else {
                    _construct<I>(o.template _get<I>());
                }
            }, std::make_index_sequence<N>());
        }

        // The alternative destroyed: a tracked_ptr leaves null in its word
        void _destroy() noexcept {
            if (_index != Npos) {
                detail::variant_dispatch(_index, [&]<size_t I>(std::integral_constant<size_t, I>) {
                    _get<I>().~Alt<I>();
                }, std::make_index_sequence<N>());
                _index = Npos;
            }
        }

        // The pointer storage starts zeroed: every word of it is null or
        // an address from the first cycle on, wherever the variant lives
        [[no_unique_address]] detail::RawStorage<L.tracked_size, L.tracked_align> _tracked = {};
        [[no_unique_address]] detail::RawStorage<L.data_size, L.data_align> _data;
        Index _index = Npos;

        template<size_t I, class... Us>
        friend variant_alternative_t<I, variant<Us...>>& get(variant<Us...>&);
        template<size_t I, class... Us>
        friend const variant_alternative_t<I, variant<Us...>>& get(const variant<Us...>&);
        template<size_t I, class... Us>
        friend std::add_pointer_t<variant_alternative_t<I, variant<Us...>>> get_if(variant<Us...>*) noexcept;
        template<size_t I, class... Us>
        friend std::add_pointer_t<const variant_alternative_t<I, variant<Us...>>> get_if(const variant<Us...>*) noexcept;
    };

    template<class T, class... Ts>
    bool holds_alternative(const variant<Ts...>& v) noexcept {
        constexpr size_t I = [] {
            constexpr bool same[] = {std::is_same_v<T, Ts>...};
            for (size_t i = 0; i < sizeof...(Ts); ++i) {
                if (same[i]) {
                    return i;
                }
            }
            return variant_npos;
        }();
        static_assert(I != variant_npos, "the type is not an alternative of the variant");
        return v.index() == I;
    }

    template<size_t I, class... Ts>
    variant_alternative_t<I, variant<Ts...>>& get(variant<Ts...>& v) {
        static_assert(I < sizeof...(Ts), "the index is out of the variant's alternatives");
        if (v.index() != I) {
            throw bad_variant_access();
        }
        return v.template _get<I>();
    }

    template<size_t I, class... Ts>
    const variant_alternative_t<I, variant<Ts...>>& get(const variant<Ts...>& v) {
        static_assert(I < sizeof...(Ts), "the index is out of the variant's alternatives");
        if (v.index() != I) {
            throw bad_variant_access();
        }
        return v.template _get<I>();
    }

    template<size_t I, class... Ts>
    variant_alternative_t<I, variant<Ts...>>&& get(variant<Ts...>&& v) {
        return std::move(get<I>(v));
    }

    template<size_t I, class... Ts>
    const variant_alternative_t<I, variant<Ts...>>&& get(const variant<Ts...>&& v) {
        return std::move(get<I>(v));
    }

    namespace detail {
        template<class T, class... Ts>
        inline constexpr size_t variant_index_of = [] {
            constexpr bool same[] = {std::is_same_v<T, Ts>...};
            size_t index = variant_npos;
            size_t count = 0;
            for (size_t i = 0; i < sizeof...(Ts); ++i) {
                if (same[i]) {
                    index = i;
                    ++count;
                }
            }
            return count == 1 ? index : variant_npos;
        }();
    }

    template<class T, class... Ts>
    T& get(variant<Ts...>& v) {
        static_assert(detail::variant_index_of<T, Ts...> != variant_npos, "the type is not exactly one alternative of the variant");
        return get<detail::variant_index_of<T, Ts...>>(v);
    }

    template<class T, class... Ts>
    const T& get(const variant<Ts...>& v) {
        static_assert(detail::variant_index_of<T, Ts...> != variant_npos, "the type is not exactly one alternative of the variant");
        return get<detail::variant_index_of<T, Ts...>>(v);
    }

    template<class T, class... Ts>
    T&& get(variant<Ts...>&& v) {
        return std::move(get<T>(v));
    }

    template<class T, class... Ts>
    const T&& get(const variant<Ts...>&& v) {
        return std::move(get<T>(v));
    }

    template<size_t I, class... Ts>
    std::add_pointer_t<variant_alternative_t<I, variant<Ts...>>> get_if(variant<Ts...>* v) noexcept {
        static_assert(I < sizeof...(Ts), "the index is out of the variant's alternatives");
        return v && v->index() == I ? &v->template _get<I>() : nullptr;
    }

    template<size_t I, class... Ts>
    std::add_pointer_t<const variant_alternative_t<I, variant<Ts...>>> get_if(const variant<Ts...>* v) noexcept {
        static_assert(I < sizeof...(Ts), "the index is out of the variant's alternatives");
        return v && v->index() == I ? &v->template _get<I>() : nullptr;
    }

    template<class T, class... Ts>
    std::add_pointer_t<T> get_if(variant<Ts...>* v) noexcept {
        static_assert(detail::variant_index_of<T, Ts...> != variant_npos, "the type is not exactly one alternative of the variant");
        return get_if<detail::variant_index_of<T, Ts...>>(v);
    }

    template<class T, class... Ts>
    std::add_pointer_t<const T> get_if(const variant<Ts...>* v) noexcept {
        static_assert(detail::variant_index_of<T, Ts...> != variant_npos, "the type is not exactly one alternative of the variant");
        return get_if<detail::variant_index_of<T, Ts...>>(v);
    }

    namespace detail {
        template<class V>
        using VariantOf = std::remove_cvref_t<V>;

        // visit over one variant: the visitor called with the alternative,
        // by index; over several: the first one, then the rest inside
        template<class R, class F, class V>
        R variant_visit(F&& f, V&& v) {
            if (v.valueless_by_exception()) {
                throw bad_variant_access();
            }
            return variant_dispatch(v.index(), [&]<size_t I>(std::integral_constant<size_t, I>) -> R {
                return std::forward<F>(f)(get<I>(std::forward<V>(v)));
            }, std::make_index_sequence<variant_size_v<VariantOf<V>>>());
        }

        template<class R, class F, class V, class... Vs>
        requires (sizeof...(Vs) > 0)
        R variant_visit(F&& f, V&& v, Vs&&... vs) {
            return variant_visit<R>([&]<class A>(A&& a) -> R {
                return variant_visit<R>([&]<class... B>(B&&... b) -> R {
                    return std::forward<F>(f)(std::forward<A>(a), std::forward<B>(b)...);
                }, std::forward<Vs>(vs)...);
            }, std::forward<V>(v));
        }

        template<class F, class... Vs>
        struct VisitResult;

        template<class F, class V, class... Vs>
        struct VisitResult<F, V, Vs...> {
            template<class... A>
            using type = typename VisitResult<F, Vs...>::template type<A..., decltype(get<0>(std::declval<V>()))>;
        };

        template<class F>
        struct VisitResult<F> {
            template<class... A>
            using type = std::invoke_result_t<F, A...>;
        };
    }

    // The result type is that of the visitor on the first alternatives,
    // which every combination must convert to (as with std::visit)
    template<class F, class... Vs>
    decltype(auto) visit(F&& f, Vs&&... vs) {
        using R = typename detail::VisitResult<F, Vs...>::template type<>;
        return detail::variant_visit<R>(std::forward<F>(f), std::forward<Vs>(vs)...);
    }

    template<class R, class F, class... Vs>
    R visit(F&& f, Vs&&... vs) {
        return detail::variant_visit<R>(std::forward<F>(f), std::forward<Vs>(vs)...);
    }

    template<class... Ts>
    requires ((std::is_move_constructible_v<Ts> && std::is_swappable_v<Ts>) && ...)
    void swap(variant<Ts...>& l, variant<Ts...>& r) noexcept(noexcept(l.swap(r))) {
        l.swap(r);
    }

    namespace detail {
        template<class F, class... Ts>
        bool variant_compare(const variant<Ts...>& l, const variant<Ts...>& r, F&& f) {
            return variant_dispatch(l.index(), [&]<size_t I>(std::integral_constant<size_t, I>) {
                return f(get<I>(l), get<I>(r));
            }, std::make_index_sequence<sizeof...(Ts)>());
        }
    }

    template<class... Ts>
    requires (requires (const Ts& a, const Ts& b) { { a == b } -> std::convertible_to<bool>; } && ...)
    bool operator==(const variant<Ts...>& l, const variant<Ts...>& r) {
        if (l.index() != r.index()) {
            return false;
        }
        if (l.valueless_by_exception()) {
            return true;
        }
        return detail::variant_compare(l, r, [](const auto& a, const auto& b) { return a == b; });
    }

    template<class... Ts>
    requires (requires (const Ts& a, const Ts& b) { { a == b } -> std::convertible_to<bool>; } && ...)
    bool operator!=(const variant<Ts...>& l, const variant<Ts...>& r) {
        return !(l == r);
    }

    template<class... Ts>
    requires (requires (const Ts& a, const Ts& b) { { a < b } -> std::convertible_to<bool>; } && ...)
    bool operator<(const variant<Ts...>& l, const variant<Ts...>& r) {
        if (r.valueless_by_exception()) {
            return false;
        }
        if (l.valueless_by_exception()) {
            return true;
        }
        if (l.index() != r.index()) {
            return l.index() < r.index();
        }
        return detail::variant_compare(l, r, [](const auto& a, const auto& b) { return a < b; });
    }

    template<class... Ts>
    requires (requires (const Ts& a, const Ts& b) { { a < b } -> std::convertible_to<bool>; } && ...)
    bool operator>(const variant<Ts...>& l, const variant<Ts...>& r) {
        return r < l;
    }

    template<class... Ts>
    requires (requires (const Ts& a, const Ts& b) { { a < b } -> std::convertible_to<bool>; } && ...)
    bool operator<=(const variant<Ts...>& l, const variant<Ts...>& r) {
        return !(r < l);
    }

    template<class... Ts>
    requires (requires (const Ts& a, const Ts& b) { { a < b } -> std::convertible_to<bool>; } && ...)
    bool operator>=(const variant<Ts...>& l, const variant<Ts...>& r) {
        return !(l < r);
    }

    template<class... Ts>
    requires (std::three_way_comparable<Ts> && ...)
    std::common_comparison_category_t<std::compare_three_way_result_t<Ts>...> operator<=>(const variant<Ts...>& l, const variant<Ts...>& r) {
        using R = std::common_comparison_category_t<std::compare_three_way_result_t<Ts>...>;
        if (l.valueless_by_exception() && r.valueless_by_exception()) {
            return R::equivalent;
        }
        if (l.valueless_by_exception()) {
            return R::less;
        }
        if (r.valueless_by_exception()) {
            return R::greater;
        }
        if (auto c = l.index() <=> r.index(); c != 0) {
            return c;
        }
        return detail::variant_dispatch(l.index(), [&]<size_t I>(std::integral_constant<size_t, I>) -> R {
            return get<I>(l) <=> get<I>(r);
        }, std::make_index_sequence<sizeof...(Ts)>());
    }
}

namespace std {
    template<class... Ts>
    struct variant_size<sgcl::variant<Ts...>> : integral_constant<size_t, sizeof...(Ts)> {};

    template<size_t I, class... Ts>
    struct variant_alternative<I, sgcl::variant<Ts...>> : sgcl::variant_alternative<I, sgcl::variant<Ts...>> {};

    template<class... Ts>
    requires (is_default_constructible_v<hash<remove_const_t<Ts>>> && ...)
    struct hash<sgcl::variant<Ts...>> {
        size_t operator()(const sgcl::variant<Ts...>& v) const {
            if (v.valueless_by_exception()) {
                return 0x9E3779B97F4A7C15ull;
            }
            auto h = sgcl::visit([](const auto& a) { return hash<remove_cvref_t<decltype(a)>>()(a); }, v);
            return h ^ (v.index() * 0x9E3779B97F4A7C15ull);
        }
    };
}
