//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../functions.h"

#include <cstddef>

namespace sgcl::io::mixin {
    // The mixin over Derived::write(slice<const byte>) -> expected<size_t, error>
    // (all of it, or an error that says how far it got, as Go's Write) and,
    // where Derived has it, async_write: write of any data, the type
    // telling what it is — a string, a text slice (a line of a
    // buffered_reader, a piece of a string), a literal or a character
    // array (to its first NUL), a C string, a std::string_view, one byte —
    // and copy_from, each the algorithm of functions.h over this stream.
    // A class that defines write hides the base's overloads of the name,
    // as C++ hides a base's name, and brings them back by using:
    //     using mixin::writer<file>::write;
    //     using mixin::writer<file>::async_write;
    // The async overloads carry no constraint of their own (it would ask
    // whether Derived has an async_write, which these are): they are
    // instantiated only where they are called, over a Derived that has one.
    template<class Derived>
    class writer {
    public:
        template<class D>
        requires detail::Text<D>
        expected<size_t, error> write(const D& text) {
            return io::write(_self(), text);
        }

        expected<size_t, error> write(byte b) {
            return io::write(_self(), b);
        }

        template<class D>
        requires detail::Text<D>
        async::task<expected<size_t, error>> async_write(const D& text) {
            return io::async_write(_self(), text);
        }

        async::task<expected<size_t, error>> async_write(byte b) {
            return io::async_write(_self(), b);
        }

        // Everything from r to its end, written here: the bytes copied
        template<req::reader R>
        expected<size_t, error> copy_from(R&& r) {
            return io::copy(_self(), std::forward<R>(r));
        }

        template<req::async_reader R>
        async::task<expected<size_t, error>> async_copy_from(R&& r) requires req::async_writer<Derived&> {
            return io::async_copy(_self(), std::forward<R>(r));
        }

    protected:
        writer() = default;
        ~writer() = default;

    private:
        Derived& _self() noexcept {
            return static_cast<Derived&>(*this);
        }
    };
}
