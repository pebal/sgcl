//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../functions.h"

#include <cstddef>

namespace sgcl::io::mixin {
    // The mixin over Derived::read(slice<byte>) -> expected<size_t, error> and,
    // where Derived has it, async_read: the rest of what a reader does,
    // each the algorithm of functions.h over this stream. The async forms
    // exist where Derived has async_read.
    template<class Derived>
    class reader {
    public:
        // Fills the whole buffer: its size; the stream ending part way is
        // errc::unexpected_eof, the bytes read the error's count(); ending
        // before the first byte is 0, the end of the stream (Go's ReadFull)
        SGCL_INLINE_HOT expected<size_t, error> read_full(const slice<byte>& buffer) noexcept(noexcept(io::read_full(std::declval<Derived&>(), buffer))) {
            return io::read_full(_self(), buffer);
        }

        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_read_full(const slice<byte>& buffer) noexcept requires req::async_reader<Derived&> {
            return io::async_read_full(_self(), buffer);
        }

        // Everything to the end of the stream
        SGCL_INLINE_HOT expected<vector<byte>, error> read_all() noexcept(noexcept(io::read_all(std::declval<Derived&>()))) {
            return io::read_all(_self());
        }

        SGCL_INLINE_HOT async::task<expected<vector<byte>, error>> async_read_all() noexcept requires req::async_reader<Derived&> {
            return io::async_read_all(_self());
        }

        SGCL_INLINE_HOT expected<string, error> read_all_text() {
            return io::read_all_text(_self());
        }

        SGCL_INLINE_HOT async::task<expected<string, error>> async_read_all_text() noexcept requires req::async_reader<Derived&> {
            return io::async_read_all_text(_self());
        }

        // This stream to its end, written to w: the bytes copied
        template<req::writer W>
        SGCL_INLINE_HOT expected<size_t, error> copy_to(W&& w) noexcept(noexcept(io::copy(std::forward<W>(w), std::declval<Derived&>()))) {
            return io::copy(std::forward<W>(w), _self());
        }

        template<req::async_writer W>
        SGCL_INLINE_HOT async::task<expected<size_t, error>> async_copy_to(W&& w) noexcept(detail::NothrowHeld<W>) requires req::async_reader<Derived&> {
            return io::async_copy(std::forward<W>(w), _self());
        }

    protected:
        reader() = default;
        ~reader() = default;

    private:
        SGCL_INLINE_HOT Derived& _self() noexcept {
            return static_cast<Derived&>(*this);
        }
    };
}
