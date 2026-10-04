//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/operation.h"
#include "../../async/coroutine.h"
#include "../../core/array.h"
#include "../../core/make_tracked.h"
#include "../../core/req.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../io/detail/bytes.h"
#include "../../io/error.h"
#include "../../io/file.h"
#include "../../io/stream.h"

#include <concepts>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

namespace sgcl::hash {
    namespace detail {
        // The bytes of a text as a slice with no owner: three words, no
        // barrier and no registration of the thread, which is what a short
        // key can afford. It is sound because every caller below holds the
        // text for the whole call (a const string& keeps the string alive)
        // and no hasher keeps a slice past the call it was given in.
        SGCL_INLINE_HOT slice<const byte> text_bytes(const char* data, size_t size) noexcept {
            return slice<const byte>(reinterpret_cast<const byte*>(data), size);
        }

        // The bytes of every form update() takes, one overload a form: what
        // the mixin's update() hands to the class's, and what of() hands to
        // a class's one-shot. A slice of bytes is passed on as it is, by
        // reference (a form that converts to one, as a std::vector does,
        // lives to the end of the call it was made for)
        SGCL_INLINE_HOT const slice<const byte>& as_bytes(const slice<const byte>& data) noexcept {
            return data;
        }

        SGCL_INLINE_HOT slice<const byte> as_bytes(const string& text) noexcept {
            return text_bytes(text.data(), text.size());
        }

        SGCL_INLINE_HOT slice<const byte> as_bytes(const slice<const char>& text) noexcept {
            return text_bytes(text.data(), text.size());
        }

        SGCL_INLINE_HOT slice<const byte> as_bytes(const slice<char>& text) noexcept {
            return text_bytes(text.data(), text.size());
        }

        template<size_t N>
        SGCL_INLINE_HOT slice<const byte> as_bytes(const char (&text)[N]) noexcept {
            const char* nul = std::char_traits<char>::find(text, N, '\0');
            return text_bytes(text, nul ? size_t(nul - text) : N);
        }

        template<class P>
        requires std::same_as<P, const char*> || std::same_as<P, char*>
        SGCL_INLINE_HOT slice<const byte> as_bytes(P text) noexcept {
            return text_bytes(text, std::strlen(text));
        }

        SGCL_INLINE_HOT slice<const byte> as_bytes(std::string_view text) noexcept {
            return text_bytes(text.data(), text.size());
        }

        template<size_t N>
        SGCL_INLINE_HOT slice<const byte> as_bytes(const array<byte, N>& digest) noexcept {
            return slice<const byte>(digest.data(), N);
        }

        template<class S>
        requires std::same_as<S, std::span<byte>> || std::same_as<S, std::span<const byte>>
        SGCL_INLINE_HOT slice<const byte> as_bytes(S bytes) noexcept {
            return slice<const byte>(bytes.data(), bytes.size());
        }
    }

    namespace mixin {
        // The mixin of every hasher, over one primitive of the class that
        // carries it: Derived::update(const slice<const byte>&), which
        // takes in bytes and never fails. What it gives is the rest of the
        // common shape, so that a reader learns it once:
        //
        //   update() for text — a string, a text slice, a literal or a
        //     character array, a pointer to a C string, a std::string_view —
        //     each hashed as its UTF-8 bytes where they lie, no string made
        //     and no owner copied. An array of char is read up to its first
        //     NUL or its end, whichever comes first, so a buffer with no NUL
        //     in it is not read past; a literal with a NUL inside is cut
        //     there, as a std::string_view made from it would be. A pointer
        //     is a template of its own so that an array never decays to it;
        //   update() for the other forms bytes come in that the slice does
        //     not take by itself: a digest (array<byte, N>, so that one
        //     hash can go into another) and a std::span of bytes, const or
        //     not. A class that declares its own update() hides all these,
        //     so it names them with `using hasher::update;`;
        //   of(), the one-shot form: `crc32::of(data)` is
        //     `crc32 h; h.update(data); return h.value();`, for bytes or
        //     text (a slice of bytes takes both) or a C string, and
        //     `xxh3_64::of(data, seed)` for a class with a seed or a key,
        //     which hashes in one call with no state made;
        //   copy_from(): a stream read to its end
        //     into the hasher, as Go's io.Copy(h, r), the bytes read or the
        //     stream's error;
        //   of_file(): the one-shot form of a whole file, read through
        //     copy_from, what of() of its bytes gives, or the file's error.
        //
        // The class itself declares the rest: digest_size and block_size,
        // value() (the result in its natural type, and the hasher goes on),
        // digest() (the same as big-endian bytes) and reset().
        //
        // A hasher is a plain value, not an io::writer: a writer is a
        // managed object with a virtual write and a coroutine behind every
        // write, which an update of a few bytes would pay for on
        // every call. copy_from is the bridge to streams instead.
        template<class Derived>
        class hasher {
        public:
            SGCL_INLINE_HOT void update(const string& text) noexcept {
                _self().update(detail::as_bytes(text));
            }

            SGCL_INLINE_HOT void update(const slice<const char>& text) noexcept {
                _self().update(detail::as_bytes(text));
            }

            SGCL_INLINE_HOT void update(const slice<char>& text) noexcept {
                _self().update(detail::as_bytes(text));
            }

            template<size_t N>
            SGCL_INLINE_HOT void update(const char (&text)[N]) noexcept {
                _self().update(detail::as_bytes(text));
            }

            template<class P>
            requires std::same_as<P, const char*> || std::same_as<P, char*>
            SGCL_INLINE_HOT void update(P text) noexcept {
                _self().update(detail::as_bytes(text));
            }

            SGCL_INLINE_HOT void update(std::string_view text) noexcept {
                _self().update(detail::as_bytes(text));
            }

            template<size_t N>
            SGCL_INLINE_HOT void update(const array<byte, N>& digest) noexcept {
                _self().update(detail::as_bytes(digest));
            }

            template<class S>
            requires std::same_as<S, std::span<byte>> || std::same_as<S, std::span<const byte>>
            SGCL_INLINE_HOT void update(S bytes) noexcept {
                _self().update(detail::as_bytes(bytes));
            }

            // The hash of the data in one call: bytes, a slice or anything
            // one is made from (a vector<byte>, a digest, uint8_t[N]); text
            // and the other forms update() takes below. A class
            // with a seed or a key takes it after the data
            // (xxh3_64::of(data, seed), siphash::of(data, key)), and so does
            // only such a class: it says so with a one-shot of its own,
            // _of(bytes, arguments...), which of() calls with no state made.
            // Any other class is made, updated once and asked its value
            template<class... Args>
            requires (sizeof...(Args) == 0 ? std::default_initializable<Derived>
                                           : requires(const Args&... args) { Derived::_of(std::declval<const slice<const byte>&>(), args...); })
            SGCL_INLINE_HOT static auto of(const slice<const byte>& data, const Args&... args) noexcept {
                if constexpr (requires { Derived::_of(data, args...); }) {
                    return Derived::_of(data, args...);
                } else {
                    Derived h;
                    h.update(data);
                    return h.value();
                }
            }

            // The other forms update() takes, each as update() hands it on:
            // a text as a slice with no owner (the owner a string's
            // conversion to bytes carries is a tracked_ptr's copy, which
            // costs a short key more than its hash), and what no slice is
            // made from — a C string to its NUL, a std::span of mutable
            // bytes, a std::string by its std::string_view. By reference,
            // so that an array does not decay to a pointer here
            template<class Data, class... Args>
            requires (!std::is_same_v<Data, slice<const byte>>)
                  && requires(const Data& data) { detail::as_bytes(data); }
                  && requires(const slice<const byte>& bytes, const Args&... args) { hasher::of(bytes, args...); }
            SGCL_INLINE_HOT static auto of(const Data& data, const Args&... args) noexcept {
                return hasher::of(detail::as_bytes(data), args...);
            }

            // Everything from r to its end, hashed here: the bytes read, or
            // the stream's error (what was read before it is hashed). One
            // block of io's copy size per call, io::copy's: on the stack of
            // copy_from, managed for async_copy_from (its reads may run on
            // the pool); the bytes handed to update() straight from it.
            // `copy_from(...)` on this thread, `co_await async_copy_from(...)` in a task
            SGCL_INLINE_HOT expected<size_t, io::error> copy_from(const io::reader& r) {
                return _block_copy_from(r);
            }

            SGCL_INLINE_HOT async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r) noexcept {
                return _co_copy_from(r);
            }

            // The hash of the whole file at path, what of() gives of its
            // bytes: the file opened, read to its end by copy_from (the
            // block on the stack; managed for async_of_file, whose reads
            // may run on the pool) and closed, never held whole in memory;
            // or the error of the open or of a read. For a class made
            // without a seed or a key, as of(data) is.
            // `of_file(...)` on this thread, `co_await async_of_file(...)` in a task
            SGCL_INLINE_HOT static auto of_file(const string& path) requires std::default_initializable<Derived> {
                return _block_of_file(path);
            }

            SGCL_INLINE_HOT static auto async_of_file(const string& path) noexcept requires std::default_initializable<Derived> {
                return _co_of_file(path);
            }

        protected:
            hasher() noexcept = default;
            ~hasher() = default;

        private:
            SGCL_INLINE_HOT Derived& _self() noexcept {
                return static_cast<Derived&>(*this);
            }

            // the two halves of the operations above: a thread's and a task's
            expected<size_t, io::error> _block_copy_from(const io::reader& r)  {
                io::detail::StackCopyBlock block;
                size_t total = 0;
                for (;;) {
                    auto n = r.read(slice<byte>(block.data(), block.size()));
                    if (!n) {
                        return io::detail::fail(n);
                    }
                    if (*n == 0) {
                        return total;
                    }
                    _self().update(slice<const byte>(block.data(), *n));
                    total += *n;
                }
            }

            async::task<expected<size_t, io::error>> _co_copy_from(io::reader r) noexcept {
                tracked_ptr<io::detail::CopyBlock> block = make_tracked<io::detail::CopyBlock>();   // managed: the read may run on the pool, its slice holds the block
                size_t total = 0;
                for (;;) {
                    auto n = co_await r.async_read(slice<byte>(block, block->data(), block->size()));
                    if (!n) {
                        co_return io::detail::fail(n);
                    }
                    if (*n == 0) {
                        co_return total;
                    }
                    _self().update(slice<const byte>(block->data(), *n));
                    total += *n;
                }
            }

            // what of() returns, or the error of the file: the value's type
            // named where Derived is complete
            template<class D = Derived>
            using _file_result = expected<decltype(std::declval<const D&>().value()), io::error>;

            static auto _block_of_file(const string& path) {
                using result = _file_result<>;
                auto f = io::open(path);
                if (!f) {
                    return result(io::detail::fail(f));
                }
                Derived h;
                auto n = h.copy_from(*f);
                (void)f->close();
                if (!n) {
                    return result(io::detail::fail(n));
                }
                return result(h.value());
            }

            template<class D = Derived>
            static async::task<_file_result<D>> _co_of_file(string path) noexcept {   // by value: a task is lazy, the caller's string may be gone before it runs
                auto f = co_await io::async_open(path);
                if (!f) {
                    co_return io::detail::fail(f);
                }
                D h;
                auto n = co_await h.async_copy_from(*f);
                (void)f->close();
                if (!n) {
                    co_return io::detail::fail(n);
                }
                co_return h.value();
            }
        };
    }

    namespace req {
        // A hasher: what said "I am a hasher" by deriving from
        // mixin::hasher<H> — nominal, as the requirements of containers
        // are, so that the error for anything else is one line at the call.
        // `void verify(req::hasher auto& h)` takes a crc32 and, later, a
        // crypto::sha256 alike.
        template<class H>
        concept hasher = sgcl::detail::Declares<H, mixin::hasher>;
    }
}
