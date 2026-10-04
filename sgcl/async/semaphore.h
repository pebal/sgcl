//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"
#include "operation.h"

#include <cstddef>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A semaphore of n permits: acquire() takes one, release() gives one back
    class semaphore {
    public:
        // A max of zero is the permits, or one when they are zero too: a
        // semaphore made closed (semaphore(0)) is opened by a release, which
        // a channel of capacity zero (a rendezvous) would lose with nobody
        // waiting for it
        explicit semaphore(size_t permits, size_t max = 0)
        : _ch(max ? max : permits ? permits : 1) {
            for (size_t i = 0; i < permits; ++i) {
                _ch.try_send();
            }
        }

        semaphore(const semaphore&) = delete;
        semaphore& operator=(const semaphore&) = delete;

        // Takes a permit, waiting for one: `co_await s.acquire()` in a task,
        // `s.acquire().wait()` on a thread
        SGCL_INLINE_HOT auto acquire() noexcept {
            return _ch.receive();
        }

        // noexcept: the receive wakes no sender, since release never
        // waits (mutex.h: MutexState::lock)
        SGCL_INLINE_HOT bool try_acquire() noexcept {
            return _ch.try_receive();
        }

        SGCL_INLINE_HOT void release() {
            _ch.try_send();
        }

        template<class F>
        SGCL_INLINE_HOT auto on_acquire(F f) noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _ch.on_receive(std::move(f));
        }

        // The permits free now
        SGCL_INLINE_HOT size_t available() const noexcept {
            return _ch.size();
        }

    private:
        detail::ChannelState<void> _ch;
    };
}
