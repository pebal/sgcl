//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "operation.h"
#include "../core/atomic.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"

#include <atomic>
#include <cassert>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A wait group: add(n) counts the work, done() counts it off, wait()
    // waits for the count to reach zero; a group whose count came back
    // from zero (add after the work was done) is waited for again, as
    // Go's is. Under it a channel per round, closed when the count
    // reaches zero and replaced by the add that starts the next round;
    // the old ones are the collector's.
    class wait_group {
    public:
        wait_group()
        : _round(make_tracked<channel<void>>()) {
        }

        wait_group(const wait_group&) = delete;
        wait_group& operator=(const wait_group&) = delete;

        void add(long n = 1) {
            if (n < 0) {   // taken off, as done() takes one off: the round closed when the count reaches zero (Go's Add(-1) releases the waiters)
                tracked_ptr<channel<void>> round = _round.load(std::memory_order_acquire);
                if (_count.fetch_add(n, std::memory_order_acq_rel) + n == 0) {
                    round->close();
                }
                return;
            }
            if (_count.fetch_add(n, std::memory_order_acq_rel) == 0 && n > 0) {
                tracked_ptr<channel<void>> round = _round.load(std::memory_order_acquire);
                if (round->closed()) {   // a new round: the channel of the last one stays closed for its waiters
                    _round.compare_exchange_strong(round, tracked_ptr<channel<void>>(make_tracked<channel<void>>()));
                }
            }
        }

        // The round is read before the count is taken off: once the count
        // is zero wait() may have returned and the group may be gone, so
        // the last done() touches nothing of it after its decrement
        void done() {
            tracked_ptr<channel<void>> round = _round.load(std::memory_order_acquire);
            if (_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                round->close();
            }
        }

        long count() const noexcept {
            return _count.load(std::memory_order_acquire);
        }

        // A case of a select: f() when the count is zero (the channel of
        // the current round, closed at zero)
        template<class F>
        auto on_done(F f) {
            return _round.load(std::memory_order_acquire)->on_receive(std::move(f));
        }

        // Waits for zero: `g.wait()` on a thread, `co_await g` in a task,
        // as a task is waited for
        void wait() {
            assert(!detail::on_worker() && "wait() blocks the worker: co_await the group from a task");
            _wait();
        }

        auto operator co_await() {
            return detail::either([this] { return _co_wait(); }, [this] { _wait(); });
        }

    private:
        void _wait() {
            while (_count.load(std::memory_order_acquire) > 0) {
                tracked_ptr<channel<void>> round = _round.load(std::memory_order_acquire);
                if (_count.load(std::memory_order_acquire) == 0) {
                    return;
                }
                (void)round->receive().wait();
            }
        }

        std::atomic<long> _count = {0};
        atomic<tracked_ptr<channel<void>>> _round;

        // the two halves of the operations above: a thread's and a task's
        task<> _co_wait() {
            while (_count.load(std::memory_order_acquire) > 0) {
                tracked_ptr<channel<void>> round = _round.load(std::memory_order_acquire);
                if (_count.load(std::memory_order_acquire) == 0) {
                    co_return;
                }
                co_await round->receive();
            }
        }
    };
}
