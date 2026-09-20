//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/coroutine.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../detail/bytes.h"
#include "../error.h"

#include <cstddef>
#include <string_view>

namespace sgcl::io {
    class reader;

    namespace mixin {
        // The mixin over Derived::write(span<const byte>) -> result<size_t>:
        // writes the whole span, as Go's Write, and returns its size; fewer
        // only with an error, which says how far it got. write_text is the
        // same bytes (a name of its own, so that a class overriding write
        // does not hide it): a string, a text slice (a line of a
        // buffered_reader, a piece of a string), a literal or a
        // std::string_view (a std::string's), the last three written from
        // where they lie, no string made; the literal's overload is also what
        // keeps it from being ambiguous between the string and the slice.
        template<class Derived>
        class writer {
        public:
            result<size_t> write_text(const string& text) {
                return _self().write(detail::bytes_of(text));
            }

            result<size_t> write_text(const slice<const char>& text) {
                return _self().write(detail::bytes_of(text));
            }

            result<size_t> write_text(const char* text) {
                return _self().write(detail::bytes_of(text));
            }

            result<size_t> write_text(std::string_view text) {
                return _self().write(detail::bytes_of(text));
            }

            result<size_t> write_byte(std::byte b) {
                return _self().write(slice<const std::byte>(&b, 1));
            }

            // Everything from r to its end, written here: the bytes copied
            result<size_t> copy_from(io::reader& r);

            task<result<size_t>> async_write_text(const string& text) {
                co_return co_await _self().async_write(detail::bytes_of(text));
            }

            task<result<size_t>> async_write_text(const slice<const char>& text) {
                co_return co_await _self().async_write(detail::bytes_of(text));
            }

            task<result<size_t>> async_write_text(const char* text) {
                co_return co_await _self().async_write(detail::bytes_of(text));
            }

            task<result<size_t>> async_write_text(std::string_view text) {
                co_return co_await _self().async_write(detail::bytes_of(text));
            }

            task<result<size_t>> async_copy_from(io::reader& r);

        protected:
            writer() = default;
            ~writer() = default;

        private:
            Derived& _self() noexcept {
                return static_cast<Derived&>(*this);
            }
        };
    }
}
