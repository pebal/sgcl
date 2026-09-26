//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
        // Fills the whole buffer: its size; the stream ending before the
        // buffer is full is errc::unexpected_eof, unless it ended before the
        // first byte, which is 0
        expected<size_t, error> read_full(const slice<byte>& buffer) {
            return io::read_full(_self(), buffer);
        }

        async::task<expected<size_t, error>> async_read_full(const slice<byte>& buffer) requires req::async_reader<Derived&> {
            return io::async_read_full(_self(), buffer);
        }

        // Everything to the end of the stream
        expected<vector<byte>, error> read_all() {
            return io::read_all(_self());
        }

        async::task<expected<vector<byte>, error>> async_read_all() requires req::async_reader<Derived&> {
            return io::async_read_all(_self());
        }

        expected<string, error> read_all_text() {
            return io::read_all_text(_self());
        }

        async::task<expected<string, error>> async_read_all_text() requires req::async_reader<Derived&> {
            return io::async_read_all_text(_self());
        }

        // This stream to its end, written to w: the bytes copied
        template<req::writer W>
        expected<size_t, error> copy_to(W&& w) {
            return io::copy(std::forward<W>(w), _self());
        }

        template<req::async_writer W>
        async::task<expected<size_t, error>> async_copy_to(W&& w) requires req::async_reader<Derived&> {
            return io::async_copy(std::forward<W>(w), _self());
        }

    protected:
        reader() = default;
        ~reader() = default;

    private:
        Derived& _self() noexcept {
            return static_cast<Derived&>(*this);
        }
    };
}
