//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "../async/coroutine.h"
#include "../core/aliases.h"
#include "../core/slice.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace sgcl::io {
    enum class seek_from { begin, current, end };

    namespace detail {
        using namespace sgcl::detail;

        // What a stream argument stands for: the object a tracked_ptr (or a
        // unique_ptr from make_tracked, or a root_ptr) points at, or the
        // argument itself. No raw pointer: an object of the library lives
        // on a stack or in a global, given by reference, or on the managed
        // heap, given by its tracked_ptr.
        template<class T>
        concept PointerLike = !std::is_pointer_v<std::remove_cvref_t<T>>
            && requires(const std::remove_cvref_t<T>& t) { t.get(); t.operator->(); *t; };

        template<class T>
        decltype(auto) target(T&& t) noexcept {
            if constexpr (PointerLike<T>) {
                return (*t);
            } else {
                return (t);
            }
        }

        template<class T>
        using Target = std::remove_reference_t<decltype(target(std::declval<T&>()))>;

        template<class R>
        concept ByteCount = std::convertible_to<R, expected<size_t, error>>;

        template<class R>
        concept ByteTask = std::same_as<std::remove_cvref_t<R>, async::task<expected<size_t, error>>>;

        // The two ways a stream offers a primitive: a method of that name,
        // or the object itself called (a lambda, a class with operator()).
        // A method wins where both are there.
        template<class T>
        concept MemberRead = requires(T& t, slice<byte> b) { { t.read(b) } -> ByteCount; };

        template<class T>
        concept MemberAsyncRead = requires(T& t, slice<byte> b) { { t.async_read(b) } -> ByteTask; };

        template<class T>
        concept MemberWrite = requires(T& t, slice<const byte> b) { { t.write(b) } -> ByteCount; };

        template<class T>
        concept MemberAsyncWrite = requires(T& t, slice<const byte> b) { { t.async_write(b) } -> ByteTask; };

        template<class T>
        concept HasMembers = requires { &T::read; } || requires { &T::write; } || requires { &T::async_read; } || requires { &T::async_write; };

        template<class T>
        concept CalledRead = !HasMembers<T> && requires(T& f, slice<byte> b) { { f(b) } -> ByteCount; };

        template<class T>
        concept CalledAsyncRead = !HasMembers<T> && requires(T& f, slice<byte> b) { { f(b) } -> ByteTask; };

        template<class T>
        concept CalledWrite = !HasMembers<T> && requires(T& f, slice<const byte> b) { { f(b) } -> ByteCount; };

        template<class T>
        concept CalledVoidWrite = !HasMembers<T> && requires(T& f, slice<const byte> b) { { f(b) } -> std::same_as<void>; };

        template<class T>
        concept CalledAsyncWrite = !HasMembers<T> && requires(T& f, slice<const byte> b) { { f(b) } -> ByteTask; };
    }

    // What io takes as a stream: whatever has the method, or is a callable
    // of the same shape; an object, a reference, a pointer, a tracked_ptr.
    // A read returns the bytes read, 0 at the end of the stream; a write
    // writes all of it or fails. A plain size_t in place of expected<size_t, error>
    // is a stream that does not fail.
    namespace req {
        template<class T>
        concept reader = detail::MemberRead<detail::Target<T>> || detail::CalledRead<detail::Target<T>>;

        template<class T>
        concept async_reader = detail::MemberAsyncRead<detail::Target<T>> || detail::CalledAsyncRead<detail::Target<T>>;

        template<class T>
        concept writer = detail::MemberWrite<detail::Target<T>> || detail::CalledWrite<detail::Target<T>> || detail::CalledVoidWrite<detail::Target<T>>;

        template<class T>
        concept async_writer = detail::MemberAsyncWrite<detail::Target<T>> || detail::CalledAsyncWrite<detail::Target<T>>;

        template<class T>
        concept closer = requires(detail::Target<T>& c) { { c.close() } -> std::convertible_to<expected<void, error>>; };

        template<class T>
        concept async_closer = requires(detail::Target<T>& c) { { c.async_close() } -> std::same_as<async::task<expected<void, error>>>; };

        template<class T>
        concept seeker = requires(detail::Target<T>& s, int64_t offset, seek_from from) { { s.seek(offset, from) } -> std::convertible_to<expected<uint64_t, error>>; };
    }

    namespace detail {
        // The primitives called the way the stream offers them
        template<class T>
        expected<size_t, error> call_read(T& t, const slice<byte>& b) {
            if constexpr (MemberRead<T>) {
                return t.read(b);
            } else {
                return t(b);
            }
        }

        template<class T>
        async::task<expected<size_t, error>> call_async_read(T& t, const slice<byte>& b) {
            if constexpr (MemberAsyncRead<T>) {
                return t.async_read(b);
            } else {
                return t(b);
            }
        }

        template<class T>
        expected<size_t, error> call_write(T& t, const slice<const byte>& b) {
            if constexpr (MemberWrite<T>) {
                return t.write(b);
            } else if constexpr (CalledVoidWrite<T>) {
                t(b);
                return b.size();
            } else {
                return t(b);
            }
        }

        template<class T>
        async::task<expected<size_t, error>> call_async_write(T& t, const slice<const byte>& b) {
            if constexpr (MemberAsyncWrite<T>) {
                return t.async_write(b);
            } else {
                return t(b);
            }
        }
    }
}
