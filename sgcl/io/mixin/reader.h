//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/coroutine.h"
#include "../../containers/vector.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../detail/bytes.h"
#include "../error.h"

#include <algorithm>
#include <cstddef>

namespace sgcl::io {
    class writer;

    namespace mixin {
        // The mixin over Derived::read(span) -> result<size_t>: the bytes
        // read, 0 at the end of the stream (a read of an empty span returns 0
        // without touching the stream), or the error. A read may return
        // fewer bytes than asked; read_full reads until the span is full or
        // the stream ends.
        template<class Derived>
        class reader {
        public:
            // Fills the whole span: the size, or an error; the stream ending
            // before the span is full is errc::unexpected_eof, unless it
            // ended before the first byte, which is 0.
            result<size_t> read_full(slice<std::byte> buffer) {
                size_t n = 0;
                while (n < buffer.size()) {
                    auto r = _self().read(buffer.subspan(n));
                    if (!r) {
                        return detail::fail(r);
                    }
                    if (*r == 0) {
                        return n ? detail::fail(error(errc::unexpected_eof, "read")) : result<size_t>(0);
                    }
                    n += *r;
                }
                return n;
            }

            // Everything to the end of the stream
            result<vector<std::byte>> read_all() {
                vector<std::byte> out;
                size_t n = 0;
                for (;;) {
                    if (n == out.size()) {
                        out.resize(n ? n * 2 : config::IoBufferSize);
                    }
                    auto r = _self().read(out.as_slice(n));
                    if (!r) {
                        return detail::fail(r);
                    }
                    if (*r == 0) {
                        break;
                    }
                    n += *r;
                }
                out.resize(n);
                return out;
            }

            result<string> read_all_text() {
                auto r = read_all();
                if (!r) {
                    return detail::fail(r);
                }
                return detail::text_of(as_bytes(r->as_slice()));
            }

            // Everything to the end of the stream, written to w: the bytes
            // copied, config::IoCopyBufferSize (32 KB) at a time through a
            // managed array
            result<size_t> copy_to(io::writer& w);

            // The same, the task giving its worker back while it waits
            task<result<size_t>> async_read_full(slice<std::byte> buffer) {
                size_t n = 0;
                while (n < buffer.size()) {
                    auto r = co_await _self().async_read(buffer.subspan(n));
                    if (!r) {
                        co_return detail::fail(r);
                    }
                    if (*r == 0) {
                        co_return n ? result<size_t>(detail::fail(error(errc::unexpected_eof, "read"))) : result<size_t>(0);
                    }
                    n += *r;
                }
                co_return n;
            }

            task<result<vector<std::byte>>> async_read_all() {
                vector<std::byte> out;
                size_t n = 0;
                for (;;) {
                    if (n == out.size()) {
                        out.resize(n ? n * 2 : config::IoBufferSize);
                    }
                    auto r = co_await _self().async_read(out.as_slice(n));
                    if (!r) {
                        co_return detail::fail(r);
                    }
                    if (*r == 0) {
                        break;
                    }
                    n += *r;
                }
                out.resize(n);
                co_return out;
            }

            task<result<string>> async_read_all_text() {
                auto r = co_await async_read_all();
                if (!r) {
                    co_return detail::fail(r);
                }
                co_return detail::text_of(as_bytes(r->as_slice()));
            }

            task<result<size_t>> async_copy_to(io::writer& w);

        protected:
            reader() = default;
            ~reader() = default;

        private:
            Derived& _self() noexcept {
                return static_cast<Derived&>(*this);
            }
        };
    }
}
