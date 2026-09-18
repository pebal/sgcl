//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "channel.h"

#include <cstddef>
#include <utility>

namespace sgcl {
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

        void acquire() {
            _ch.receive();
        }

        bool try_acquire() {
            return _ch.try_receive();
        }

        void release() {
            _ch.try_send();
        }

        auto async_acquire() noexcept {
            return _ch.async_receive();
        }

        template<class F>
        auto on_acquire(F f) {
            return _ch.on_receive(std::move(f));
        }

        // The permits free now
        size_t available() const noexcept {
            return _ch.size();
        }

    private:
        channel<void> _ch;
    };
}
