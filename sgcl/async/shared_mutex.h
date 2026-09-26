//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"
#include "mutex.h"
#include "operation.h"

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace sgcl::async {
    namespace detail { using namespace sgcl::detail; }
    // A shared mutex: any number of readers at once, or one writer; Go's
    // sync.RWMutex, Java's ReentrantReadWriteLock. Not a channel under a
    // name, as the others are: a lock a reader takes by adding one to a
    // word and gives back by subtracting one, so that readers, the common
    // case, never meet a channel; the channels are for the waits. One
    // word holds the count of the readers that hold or wait for the lock
    // (Go's readerCount) and, in its low bit, whether a writer holds or
    // waits for it; a writer sets that bit and waits for the readers
    // counted before it to leave, a reader that finds it set waits for
    // that writer to leave. Writers are served one at a time through a
    // mutex of the module (`_writers`), the last reader out of a pending
    // writer's way sends it a signal (`_drained`, a channel of one), and
    // the readers a writer holds back wait on a channel closed by its
    // unlock (`_round`: made by the first reader to wait, closed and let
    // go of by the writer). The preference is Go's: a writer waiting
    // blocks the readers that arrive after it, so that a stream of
    // readers cannot starve it, and the readers it held back get in
    // before the next writer, because they are counted (the next writer
    // waits for them), so that a stream of writers cannot starve them.
    // The writer's generation in the high half of the word tells a
    // held-back reader that the writer it waited for is gone even if
    // another has taken its place (the reader is counted by that one:
    // it goes in and the writer waits for it).
    //
    // What a shared lock costs: one atomic add and one atomic subtract
    // (the channel-built mutex: a receive and a send through its ring, a
    // few times more; the numbers are on the page). What the rest costs:
    // the writers' mutex, the drained channel and, per round of held-back
    // readers, one channel of signals. The waits have the blocking form
    // and the awaitable one; no select case: a select waits on channels,
    // and the lock is a word.
    class shared_mutex {
        static constexpr uint64_t Writer = 1;                       // a writer holds or waits
        static constexpr uint64_t Reader = 2;                       // one reader holding or waiting: bits 1..31
        static constexpr uint64_t Generation = uint64_t(1) << 32;   // one writer more: bits 32..63

        static constexpr uint64_t _readers(uint64_t w) noexcept {
            return (w & 0xffffffff) / Reader;
        }

        static constexpr uint32_t _generation(uint64_t w) noexcept {
            return (uint32_t)(w >> 32);
        }

    public:
        shared_mutex() = default;
        shared_mutex(const shared_mutex&) = delete;
        shared_mutex& operator=(const shared_mutex&) = delete;

        // The readers' lock: counted in, and waiting for the writer to
        // leave when one holds or waits
        void lock_shared() {
            uint32_t writer;
            if (!_enter_shared(writer)) {
                _wait_shared(writer);
            }
        }

        // Taken when no writer holds or waits
        bool try_lock_shared() noexcept {
            auto w = _word.load(std::memory_order_acquire);
            while (!(w & Writer)) {
                if (_word.compare_exchange_weak(w, w + Reader, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            }
            return false;
        }

        // Counted out; the last reader a pending writer waits for signals it
        void unlock_shared() {
            auto w = _word.fetch_sub(Reader, std::memory_order_seq_cst);
            if ((w & Writer) && _reader_wait.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                _drained.try_send();
            }
        }

        // The writer's lock: the writers' mutex, then the bit, then the
        // wait for the readers counted before the bit
        void lock() {
            _writers.lock();
            if (!_claim()) {
                (void)_drained.receive().wait();
            }
        }

        bool try_lock() {
            if (!_writers.try_lock()) {
                return false;
            }
            auto w = _word.load(std::memory_order_acquire);
            while (_readers(w) == 0) {
                if (_word.compare_exchange_weak(w, w + Generation + Writer, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            }
            _writers.unlock();
            return false;
        }

        // The bit cleared, the readers held back woken, the next writer let in
        void unlock() {
            _word.fetch_sub(Writer, std::memory_order_seq_cst);
            std::atomic_thread_fence(std::memory_order_seq_cst);   // the word before the look at the round (a reader: the round before its look at the word)
            if (auto round = _round.exchange(nullptr, std::memory_order_acq_rel)) {
                round->close();
            }
            _writers.unlock();
        }

        // The guards: `auto g = co_await m.scoped_lock_shared();` for
        // a reader, `co_await m.scoped_lock()` for the writer
        // (std::shared_lock and std::lock_guard do the same for a thread)
        class shared_guard {
        public:
            explicit shared_guard(shared_mutex& m) noexcept
            : _m(&m) {
            }

            shared_guard(shared_guard&& o) noexcept
            : _m(std::exchange(o._m, nullptr)) {
            }

            shared_guard(const shared_guard&) = delete;
            shared_guard& operator=(const shared_guard&) = delete;

            ~shared_guard() {
                if (_m) {
                    _m->unlock_shared();
                }
            }

            // The mutex let go of by the guard, still locked: the caller
            // unlocks it (std::unique_lock::release)
            shared_mutex* release() noexcept {
                return std::exchange(_m, nullptr);
            }

        private:
            shared_mutex* _m;
        };

        class guard {
        public:
            explicit guard(shared_mutex& m) noexcept
            : _m(&m) {
            }

            guard(guard&& o) noexcept
            : _m(std::exchange(o._m, nullptr)) {
            }

            guard(const guard&) = delete;
            guard& operator=(const guard&) = delete;

            ~guard() {
                if (_m) {
                    _m->unlock();
                }
            }

            // The mutex let go of by the guard, still locked: the caller
            // unlocks it (std::unique_lock::release)
            shared_mutex* release() noexcept {
                return std::exchange(_m, nullptr);
            }

        private:
            shared_mutex* _m;
        };

        // The awaitables: the fast path (the word) in await_ready, so
        // that a lock nobody contends allocates nothing; the wait, when
        // there is one, a task awaited from await_suspend
        template<bool Shared, bool Scoped>
        class lock_op {
        public:
            bool await_ready() {
                if constexpr (Shared) {
                    return _m->_enter_shared(_writer);
                } else {
                    return _m->_enter(_stage);
                }
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                if constexpr (Shared) {
                    _slow = _m->_async_wait_shared(_writer);
                } else {
                    _slow = _m->_async_wait(_stage);
                }
                _await.emplace(_slow.operator co_await());
                return _await->await_suspend(h);
            }

            auto await_resume() {
                if (_await) {
                    _await->await_resume();
                }
                if constexpr (Scoped) {
                    return std::conditional_t<Shared, shared_guard, guard>(*_m);
                }
            }

        private:
            friend class shared_mutex;

            explicit lock_op(shared_mutex& m) noexcept
            : _m(&m) {
            }

            shared_mutex* _m;
            uint32_t _writer = 0;   // the generation of the writer a reader waits for
            int _stage = 0;         // how far the writer's fast path got
            task<> _slow;
            optional<task<>::awaiter> _await;
        };

        // The locks held for a scope, the only forms a task has (lock() and
        // lock_shared() are the standard's, for std::unique_lock and
        // std::shared_lock on a thread): `auto g = co_await m.scoped_lock();`,
        // `co_await m.scoped_lock_shared()`, and `.wait()` on a thread
        auto scoped_lock_shared() {
            return operation([this](auto how) {
                if constexpr (std::is_same_v<decltype(how), detail::awaited_t>) {
                    return lock_op<true, true>(*this);
                } else {
                    lock_shared();
                    return shared_guard(*this);
                }
            });
        }

        auto scoped_lock() {
            return operation([this](auto how) {
                if constexpr (std::is_same_v<decltype(how), detail::awaited_t>) {
                    return lock_op<false, true>(*this);
                } else {
                    lock();
                    return guard(*this);
                }
            });
        }

    private:
        // A reader counted in: true when no writer holds or waits; else
        // the generation of the writer to wait for
        bool _enter_shared(uint32_t& writer) noexcept {
            auto w = _word.fetch_add(Reader, std::memory_order_seq_cst);
            if (!(w & Writer)) {
                return true;
            }
            writer = _generation(w);
            return false;
        }

        // Whether the writer of that generation is gone: the bit clear,
        // or another writer's (which counted this reader and waits for it)
        bool _writer_gone(uint32_t writer) const noexcept {
            auto w = _word.load(std::memory_order_seq_cst);
            return !(w & Writer) || _generation(w) != writer;
        }

        // The round the held-back readers wait on: the one there, or a new one
        tracked_ptr<channel<void>> _round_or_new() {
            tracked_ptr<channel<void>> round = _round.load(std::memory_order_acquire);
            while (!round) {
                if (_round.compare_exchange_strong(round, tracked_ptr<channel<void>>(make_tracked<channel<void>>()), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    round = _round.load(std::memory_order_acquire);
                }
            }
            return round;
        }

        void _wait_shared(uint32_t writer) {
            for (;;) {
                tracked_ptr<channel<void>> round = _round_or_new();
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_writer_gone(writer)) {
                    return;
                }
                (void)round->receive().wait();
            }
        }

        task<> _async_wait_shared(uint32_t writer) {
            for (;;) {
                tracked_ptr<channel<void>> round = _round_or_new();
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_writer_gone(writer)) {
                    co_return;
                }
                co_await round->receive();
            }
        }

        // The writer's bit set (the writers' mutex held): true when no
        // reader holds the lock; else the readers counted are waited for,
        // through _reader_wait (Go's readerWait: a reader that leaves
        // between the bit and this add has counted itself off already)
        bool _claim() noexcept {
            auto w = _word.fetch_add(Generation + Writer, std::memory_order_seq_cst);
            auto n = (long)_readers(w);
            return n == 0 || _reader_wait.fetch_add(n, std::memory_order_acq_rel) + n == 0;
        }

        // The writer's fast path: the writers' mutex without waiting (stage
        // 1), then the bit and no reader to wait for (true)
        bool _enter(int& stage) {
            if (!_writers.try_lock()) {
                stage = 0;
                return false;
            }
            stage = 1;
            return _claim();
        }

        task<> _async_wait(int stage) {
            if (stage == 0) {
                co_await _writers._ch.receive();
                if (_claim()) {
                    co_return;
                }
            }
            co_await _drained.receive();
        }

        atomic<uint64_t> _word = {0};
        std::atomic<long> _reader_wait = {0};
        mutex _writers;
        channel<void> _drained{1};
        atomic<tracked_ptr<channel<void>>> _round;
    };
}
