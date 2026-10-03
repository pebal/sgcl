//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/handle_word.h"
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
    namespace detail {
        // A wait group: add(n) counts the work, done() counts it off, wait()
        // waits for the count to reach zero; a group whose count came back
        // from zero (add after the work was done) is waited for again, as
        // Go's is. Under it a channel per round, closed when the count
        // reaches zero and replaced by the add that starts the next round;
        // the old ones are the collector's.
        class WaitGroupState {
        public:
            // At zero the round is over: its channel closed from the start,
            // so that on_done serves a group that never counted up (the
            // first add opens a new round, as every later one does)
            WaitGroupState() noexcept   // the close of a channel just made wakes nobody
            : _round(detail::make_linked_state<void>()) {
                _round.load(std::memory_order_relaxed)->close();
            }

            WaitGroupState(const WaitGroupState&) = delete;
            WaitGroupState& operator=(const WaitGroupState&) = delete;

            void add(long n = 1) {
                if (n < 0) {   // taken off, as done() takes one off: the round closed when the count reaches zero (Go's Add(-1) releases the waiters)
                    tracked_ptr<detail::ChannelState<void>> round = _round.load(std::memory_order_acquire);
                    if (_count.fetch_add(n, std::memory_order_acq_rel) + n == 0) {
                        round->close();
                    }
                    return;
                }
                if (_count.fetch_add(n, std::memory_order_acq_rel) == 0 && n > 0) {
                    tracked_ptr<detail::ChannelState<void>> round = _round.load(std::memory_order_acquire);
                    if (round->closed()) {   // a new round: the channel of the last one stays closed for its waiters
                        _round.compare_exchange_strong(round, detail::make_linked_state<void>());
                    }
                }
            }

            // The round is read before the count is taken off: once the count
            // is zero wait() may have returned and the group may be gone, so
            // the last done() touches nothing of it after its decrement
            void done() {
                tracked_ptr<detail::ChannelState<void>> round = _round.load(std::memory_order_acquire);
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
            auto on_done(F f) noexcept(std::is_nothrow_move_constructible_v<F>) {
                return _round.load(std::memory_order_acquire)->on_receive(std::move(f));
            }

            // Waits for zero: `g.wait()` on a thread, `co_await g` in a task,
            // as a task is waited for
            // noexcept: nobody sends on a round's channel, the count reaching
            // zero closes it (mutex.h: MutexState::lock)
            void wait() noexcept {
                assert(!detail::on_worker() && "wait() blocks the worker: co_await the group from a task");
                _wait();
            }

            auto operator co_await() noexcept {
                return detail::either([this] { return _co_wait(); }, [this] { _wait(); });
            }

        private:
            void _wait() noexcept {
                while (_count.load(std::memory_order_acquire) > 0) {
                    tracked_ptr<detail::ChannelState<void>> round = _round.load(std::memory_order_acquire);
                    if (_count.load(std::memory_order_acquire) == 0) {
                        return;
                    }
                    (void)round->receive().wait();
                }
            }

            std::atomic<long> _count = {0};
            atomic<tracked_ptr<detail::ChannelState<void>>> _round;

            // the two halves of the operations above: a thread's and a task's
            task<> _co_wait() {
                while (_count.load(std::memory_order_acquire) > 0) {
                    tracked_ptr<detail::ChannelState<void>> round = _round.load(std::memory_order_acquire);
                    if (_count.load(std::memory_order_acquire) == 0) {
                        co_return;
                    }
                    co_await round->receive();
                }
            }
        };
    }

    // A handle: one word, a tracked_ptr to the state (detail::WaitGroupState),
    // which copies share — passed by value into the tasks that count off,
    // a member of a managed object, a local; in a global or a std
    // container, a rooted<async::wait_group>. Made at zero by its
    // constructor; there is no empty group.
    class wait_group {
    public:
        wait_group() noexcept
        : _s(make_tracked<detail::WaitGroupState>()) {
        }

        wait_group(const wait_group&) noexcept = default;
        wait_group(wait_group&&) noexcept = default;
        wait_group& operator=(const wait_group&) noexcept = default;
        wait_group& operator=(wait_group&&) noexcept = default;

        void add(long n = 1) const {
            _s->add(n);
        }

        void done() const {
            _s->done();
        }

        long count() const noexcept {
            return _s->count();
        }

        // A case of a select: f() when the count is zero
        template<class F>
        auto on_done(F f) const noexcept(std::is_nothrow_move_constructible_v<F>) {
            return _s->on_done(std::move(f));
        }

        // Waits for zero: `g.wait()` on a thread, `co_await g` in a task,
        // as a task is waited for
        void wait() const noexcept {
            _s->wait();
        }

        auto operator co_await() const noexcept {
            return _s->operator co_await();
        }

        // The same group: the same state
        friend bool operator==(const wait_group& a, const wait_group& b) noexcept {
            return a._s == b._s;
        }

    private:
        // The handle's word, for the atomics (core/detail/handle_word.h)
        friend struct sgcl::detail::HandleWord;

        wait_group(sgcl::detail::FromWord, const tracked_ptr<detail::WaitGroupState>& w) noexcept
        : _s(w) {
        }

        tracked_ptr<detail::WaitGroupState>& _handle_word() noexcept {
            return _s;
        }

        const tracked_ptr<detail::WaitGroupState>& _handle_word() const noexcept {
            return _s;
        }

        tracked_ptr<detail::WaitGroupState> _s;
    };
}
