//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/detail/os.h"
#include "../core/vector.h"
#include "../core/detail/backoff.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // A priority queue shared by any number of threads: a binary heap under
    // a lock, the counterpart of Java's PriorityBlockingQueue (a heap under
    // a ReentrantLock), with the element that is least by Compare at the
    // front, as std::priority_queue with its comparison reversed: std::less
    // gives the smallest first. Not lock-free, on purpose: the front of a
    // priority queue is one place every consumer writes, and the lock-free
    // structures for it (the skip-list priority queue of Shavit and Lotan,
    // which this class was first) pay for every pop with a race on the
    // cache lines of that front, while a heap under a lock keeps them in
    // one thread's cache at a time; measured here, the skip list took two
    // to fourteen times longer per operation than this heap at one to
    // sixteen threads (concurrent/benchmarks.md), which is what the authors
    // of java.util.concurrent found before. The lock is a
    // test-and-test-and-set with the backoff of the Treiber stack
    // (core/detail/backoff.h) and a park on the word after the spin, so that a
    // thread finding the lock taken for the few dozen nanoseconds of a
    // push or a pop spins through it and only a thread that would wait
    // longer goes to the kernel; an unlock wakes a parked thread only
    // when there is one.
    //
    // Equal elements come out in the order they went in: every element
    // carries the number its push took from a counter, and the heap orders
    // by (value, number), which a plain heap does not (FIFO among equals,
    // as the skip-list queue gave). The heap is a vector on the managed
    // heap, so an element holding a tracked_ptr is traced in it; the
    // container holds that vector, the lock and two counters, and lives
    // where a tracked_ptr may (on a stack or inside a managed object). An
    // element is moved out at the pop and destroyed then, as
    // std::priority_queue's is; T need not be copyable, except for
    // try_top, which copies the least element under the lock. pop() waits
    // on an empty queue, on the count of the pushes, which every push
    // raises after its element is in the heap.
    template<class T, class Compare = std::less<T>>
    class priority_queue {
        struct Entry {
            T value;
            uint64_t seq;
        };

        // The heap's order: the greatest of the heap is the least by
        // Compare, and of two equal the one pushed first
        struct Order {
            bool operator()(const Entry& a, const Entry& b) const {
                if (comp(b.value, a.value)) {
                    return true;
                }
                if (comp(a.value, b.value)) {
                    return false;
                }
                return b.seq < a.seq;
            }

            [[no_unique_address]] Compare comp;
        };

    public:
        using value_type = T;
        using value_compare = Compare;
        using size_type = size_t;

        priority_queue()
        : priority_queue(Compare()) {
        }

        explicit priority_queue(const Compare& comp)
        : _order{comp} {
        }

        template<std::input_iterator InputIt>
        priority_queue(InputIt first, InputIt last, const Compare& comp = Compare())
        : priority_queue(comp) {
            for (; first != last; ++first) {
                emplace(*first);
            }
        }

        priority_queue(std::initializer_list<T> ilist, const Compare& comp = Compare())
        : priority_queue(ilist.begin(), ilist.end(), comp) {
        }

        priority_queue(const priority_queue&) = delete;
        priority_queue& operator=(const priority_queue&) = delete;

        void push(const T& value) {
            emplace(value);
        }

        void push(T&& value) {
            emplace(std::move(value));
        }

        // The element constructed from a... and sifted into the heap under
        // the lock; the count of the pushes raised after, and a thread
        // waiting for an element woken
        template<class... A>
        void emplace(A&&... a) {
            {
                Guard guard(_lock);
                _heap.push_back(Entry{T(std::forward<A>(a)...), _seq++});
                std::push_heap(_heap.begin(), _heap.end(), _order);
                _count.store(_heap.size(), std::memory_order_relaxed);
            }
            _pushed.fetch_add(1, std::memory_order_seq_cst);
            if (_waiters.load(std::memory_order_seq_cst)) {
                _pushed.notify_all();
            }
        }

        // The least element, moved out, or nothing when the queue is empty
        optional<T> try_pop() {
            Guard guard(_lock);
            if (_heap.empty()) {
                return nullopt;
            }
            std::pop_heap(_heap.begin(), _heap.end(), _order);
            optional<T> value(std::move(_heap.back().value));
            _heap.pop_back();
            _count.store(_heap.size(), std::memory_order_relaxed);
            return value;
        }

        // The least element, waiting for one when the queue is empty: the
        // count of the pushes is read before the attempt, so a push that
        // came after the attempt found nothing changes the count and the
        // wait returns at once, and one that came before was found
        T pop() {
            for (;;) {
                uint64_t pushed = _pushed.load(std::memory_order_seq_cst);
                if (auto value = try_pop()) {
                    return std::move(*value);
                }
                _wait(pushed);
            }
        }

        // A copy of the least element, or nothing when the queue is empty
        optional<T> try_top() const requires std::is_copy_constructible_v<T> {
            Guard guard(_lock);
            if (_heap.empty()) {
                return nullopt;
            }
            return optional<T>(std::in_place, _heap.front().value);
        }

        // The number of elements as of the last push or pop completed
        bool empty() const noexcept {
            return _count.load(std::memory_order_relaxed) == 0;
        }

        size_type size() const noexcept {
            return _count.load(std::memory_order_relaxed);
        }

        // Every element destroyed
        void clear() {
            Guard guard(_lock);
            _heap.clear();
            _count.store(0, std::memory_order_relaxed);
        }

        value_compare value_comp() const {
            return _order.comp;
        }

    private:
        // The lock: 0 free, 1 taken, 2 taken with a thread parked on the
        // word (Drepper's second mutex). A taker spins with the backoff
        // while the word reads taken, up to SpinRounds attempts, then
        // parks: it stores 2 and waits for the word to change, and an
        // unlock that finds 2 wakes one parked thread.
        struct Lock {
            std::atomic<uint32_t> word = {0};

            void lock() noexcept {
                uint32_t expected = 0;
                if (word.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                    return;
                }
                detail::Backoff<> backoff;
                for (unsigned round = 0; round < SpinRounds; ++round) {
                    backoff();
                    if (word.load(std::memory_order_relaxed) == 0) {
                        expected = 0;
                        if (word.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                            return;
                        }
                    }
                }
                while (word.exchange(2, std::memory_order_acquire) != 0) {
                    word.wait(2, std::memory_order_relaxed);
                }
            }

            void unlock() noexcept {
                if (word.exchange(0, std::memory_order_release) == 2) {
                    word.notify_one();
                }
            }

            static constexpr unsigned SpinRounds = 64;
        };

        struct Guard {
            Lock& lock;
            explicit Guard(Lock& l) noexcept : lock(l) { lock.lock(); }
            ~Guard() { lock.unlock(); }
        };

        // The wait of pop on the count of the pushes: a spin first, then
        // a park, counted so that a push notifies only when someone waits
        // (the seq_cst store and load on each side keep the gate from
        // losing a wakeup, as in bounded_queue.h)
        SGCL_NOINLINE void _wait(uint64_t pushed) noexcept {
            for (unsigned i = 0; i < SpinPauses; ++i) {
                if (_pushed.load(std::memory_order_seq_cst) != pushed) {
                    return;
                }
                detail::os::spin_pause();
            }
            _waiters.fetch_add(1, std::memory_order_seq_cst);
            _pushed.wait(pushed, std::memory_order_seq_cst);
            _waiters.fetch_sub(1, std::memory_order_relaxed);
        }

        static constexpr unsigned SpinPauses = 1024;

        vector<Entry> _heap;
        [[no_unique_address]] Order _order;
        uint64_t _seq = 0;                        // under the lock
        mutable Lock _lock;
        std::atomic<size_t> _count = {0};         // the size, for the readers without the lock
        alignas(config::cache_line_size) std::atomic<uint64_t> _pushed = {0};
        std::atomic<uint32_t> _waiters = {0};
    };
}
