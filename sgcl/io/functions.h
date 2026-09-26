//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "req.h"
#include "detail/bytes.h"
#include "../core/config.h"
#include "../core/make_tracked.h"
#include "../core/string.h"
#include "../core/vector.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace sgcl::io {
    // The algorithms over the streams, written once for every stream that
    // meets the requirement (req.h): a type with the method, a callable of
    // the same shape, an object, a reference, a pointer or a tracked_ptr.
    // The name without a prefix does its work now, on the calling thread;
    // async_ is the same for a task, which gives its worker back while it
    // waits. The async forms take a stream given as a temporary into their
    // frame; one given by reference is the caller's to keep alive until
    // the task is done, as a co_await in the same statement does.

    namespace detail {
        // A parameter of an async form: a reference stays one, a temporary
        // is moved into the frame
        template<class T>
        using Held = std::conditional_t<std::is_lvalue_reference_v<T>, T, std::decay_t<T>>;

        // The bytes of a text, from where it lies
        inline slice<const byte> text_bytes(const string& t) noexcept {
            return bytes_of(t);
        }

        inline slice<const byte> text_bytes(const slice<const char>& t) noexcept {
            return bytes_of(t);
        }

        inline slice<const byte> text_bytes(const slice<char>& t) noexcept {
            return bytes_of(slice<const char>(t));
        }

        inline slice<const byte> text_bytes(std::string_view t) noexcept {
            return bytes_of(t);
        }

        // A literal or a character array: up to its first NUL, never past
        // its end (an array filled to the brim has none)
        template<size_t N>
        slice<const byte> text_bytes(const char (&t)[N]) noexcept {
            const char* nul = std::char_traits<char>::find(t, N, '\0');
            return bytes_of(std::string_view(t, nul ? size_t(nul - t) : N));
        }

        // A C string, to its NUL; a null pointer does not compile
        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        slice<const byte> text_bytes(P t) noexcept {
            return bytes_of(std::string_view(t));
        }

        template<class T>
        concept Text = requires(const T& t) { text_bytes(t); };

        template<class R>
        expected<size_t, error> read_full(R& r, const slice<byte>& b) {
            size_t n = 0;
            while (n < b.size()) {
                auto got = call_read(r, b.subspan(n));
                if (!got) {
                    return fail(got);
                }
                if (*got == 0) {
                    return fail(error(errc::unexpected_eof, "read"));   // fewer than asked, none included, as Go's ReadFull
                }
                n += *got;
            }
            return n;
        }

        template<class R>
        async::task<expected<size_t, error>> async_read_full(R r, slice<byte> b) {
            auto& s = target(r);
            size_t n = 0;
            while (n < b.size()) {
                auto got = co_await call_async_read(s, b.subspan(n));
                if (!got) {
                    co_return fail(got);
                }
                if (*got == 0) {
                    co_return fail(error(errc::unexpected_eof, "read"));
                }
                n += *got;
            }
            co_return n;
        }

        template<class R>
        expected<vector<byte>, error> read_all(R& r) {
            vector<byte> out;
            size_t n = 0;
            for (;;) {
                if (n == out.size()) {
                    out.resize(n ? n * 2 : config::io_buffer_size);
                }
                auto got = call_read(r, out.as_slice(n));
                if (!got) {
                    return fail(got);
                }
                if (*got == 0) {
                    break;
                }
                n += *got;
            }
            out.resize(n);
            return out;
        }

        template<class R>
        async::task<expected<vector<byte>, error>> async_read_all(R r) {
            auto& s = target(r);
            vector<byte> out;
            size_t n = 0;
            for (;;) {
                if (n == out.size()) {
                    out.resize(n ? n * 2 : config::io_buffer_size);
                }
                auto got = co_await call_async_read(s, out.as_slice(n));
                if (!got) {
                    co_return fail(got);
                }
                if (*got == 0) {
                    break;
                }
                n += *got;
            }
            out.resize(n);
            co_return out;
        }

        template<class R>
        async::task<expected<string, error>> async_read_all_text(R r) {
            auto all = co_await detail::async_read_all<R&>(r);
            if (!all) {
                co_return fail(all);
            }
            co_return text_of(as_bytes(all->as_slice()));
        }

        // A reader with a way of its own to hand its bytes to a writer
        // (Go's WriterTo): a buffer writes what it holds in one call
        template<class R, class W>
        concept WritesTo = requires(R& r, W& w) { { r.write_to(w) } -> std::convertible_to<expected<size_t, error>>; };

        template<class R, class W>
        concept AsyncWritesTo = requires(R& r, W& w) { { r.async_write_to(w) } -> std::same_as<async::task<expected<size_t, error>>>; };

        template<class W, class R>
        expected<size_t, error> copy(W& w, R& r) {
            if constexpr (WritesTo<R, W>) {
                return r.write_to(w);
            } else {
                tracked_ptr<CopyBlock> block = make_tracked<CopyBlock>();
                size_t total = 0;
                for (;;) {
                    slice<byte> room(block, block->data(), block->size());
                    auto got = call_read(r, room);
                    if (!got) {
                        return fail(got);
                    }
                    if (*got == 0) {
                        return total;
                    }
                    auto put = call_write(w, slice<const byte>(room.first(*got)));
                    if (!put) {
                        return fail(put);
                    }
                    total += *got;
                }
            }
        }

        template<class W, class R>
        async::task<expected<size_t, error>> async_copy(W w, R r) {
            auto& dst = target(w);
            auto& src = target(r);
            if constexpr (AsyncWritesTo<std::remove_reference_t<decltype(src)>, std::remove_reference_t<decltype(dst)>>) {
                co_return co_await src.async_write_to(dst);
            } else {
                tracked_ptr<CopyBlock> block = make_tracked<CopyBlock>();
                size_t total = 0;
                for (;;) {
                    slice<byte> room(block, block->data(), block->size());
                    auto got = co_await call_async_read(src, room);
                    if (!got) {
                        co_return fail(got);
                    }
                    if (*got == 0) {
                        co_return total;
                    }
                    auto put = co_await call_async_write(dst, slice<const byte>(room.first(*got)));
                    if (!put) {
                        co_return fail(put);
                    }
                    total += *got;
                }
            }
        }

        template<class W>
        async::task<expected<size_t, error>> async_write_bytes(W w, slice<const byte> b) {
            co_return co_await call_async_write(target(w), b);
        }

        template<class W>
        async::task<expected<size_t, error>> async_write_byte(W w, byte b) {
            co_return co_await call_async_write(target(w), slice<const byte>(&b, 1));   // the byte in the frame, alive across the wait
        }
    }

    // Fills the whole buffer: its size, or an error; the stream ending
    // before the buffer is full is errc::unexpected_eof, before the first
    // byte too (Go's ReadFull tells the two by io.EOF; here e.is_eof()
    // answers both, and a loop over records ends on it)
    template<req::reader R>
    expected<size_t, error> read_full(R&& r, const slice<byte>& buffer) {
        return detail::read_full(detail::target(r), buffer);
    }

    template<req::async_reader R>
    async::task<expected<size_t, error>> async_read_full(R&& r, const slice<byte>& buffer) {
        return detail::async_read_full<detail::Held<R>>(std::forward<R>(r), buffer);
    }

    // Everything to the end of the stream
    template<req::reader R>
    expected<vector<byte>, error> read_all(R&& r) {
        return detail::read_all(detail::target(r));
    }

    template<req::async_reader R>
    async::task<expected<vector<byte>, error>> async_read_all(R&& r) {
        return detail::async_read_all<detail::Held<R>>(std::forward<R>(r));
    }

    // Everything to the end of the stream, as text
    template<req::reader R>
    expected<string, error> read_all_text(R&& r) {
        auto all = detail::read_all(detail::target(r));
        if (!all) {
            return detail::fail(all);
        }
        return detail::text_of(as_bytes(all->as_slice()));
    }

    template<req::async_reader R>
    async::task<expected<string, error>> async_read_all_text(R&& r) {
        return detail::async_read_all_text<detail::Held<R>>(std::forward<R>(r));
    }

    // Data written, the type telling what it is: bytes (a slice, a vector,
    // an array), text (a string, a text slice such as a line of a
    // buffered_reader, a literal or a character array to its first NUL, a C
    // string, a std::string_view), or one byte; each from where it lies, no
    // string made
    template<req::writer W, class D>
    requires detail::Text<D> || std::convertible_to<const D&, slice<const byte>>
    expected<size_t, error> write(W&& w, const D& data) {
        if constexpr (detail::Text<D>) {
            return detail::call_write(detail::target(w), detail::text_bytes(data));
        } else {
            return detail::call_write(detail::target(w), slice<const byte>(data));
        }
    }

    template<req::writer W>
    expected<size_t, error> write(W&& w, byte b) {
        return detail::call_write(detail::target(w), slice<const byte>(&b, 1));
    }

    // The data is written from where it lies: a string, a slice or a
    // vector keeps its owner in the frame, a literal is static; a
    // std::string_view and a character array are the caller's to keep until
    // the task is done
    template<req::async_writer W, class D>
    requires detail::Text<D> || std::convertible_to<const D&, slice<const byte>>
    async::task<expected<size_t, error>> async_write(W&& w, const D& data) {
        if constexpr (detail::Text<D>) {
            return detail::async_write_bytes<detail::Held<W>>(std::forward<W>(w), detail::text_bytes(data));
        } else {
            return detail::async_write_bytes<detail::Held<W>>(std::forward<W>(w), slice<const byte>(data));
        }
    }

    template<req::async_writer W>
    async::task<expected<size_t, error>> async_write(W&& w, byte b) {
        return detail::async_write_byte<detail::Held<W>>(std::forward<W>(w), b);
    }

    // Copies r to its end into w: the bytes copied, config::io_copy_buffer_size
    // (32 KB) at a time through one managed block, or in one call when r
    // has a way of its own (write_to: a buffer hands over what it holds)
    template<req::writer W, req::reader R>
    expected<size_t, error> copy(W&& w, R&& r) {
        return detail::copy(detail::target(w), detail::target(r));
    }

    template<req::async_writer W, req::async_reader R>
    async::task<expected<size_t, error>> async_copy(W&& w, R&& r) {
        return detail::async_copy<detail::Held<W>, detail::Held<R>>(std::forward<W>(w), std::forward<R>(r));
    }
}
