//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/detail/nothrow_function.h"
#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/detail/os.h"
#include "../core/vector.h"
#include "../core/detail/backoff.h"

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
        static_assert(detail::nothrow_function_object<Compare, const T&, const T&>, "sgcl::concurrent::priority_queue: Compare must be noexcept");

        // What a push and a pop do to the elements: construct, move
        // construct and move assign
        static constexpr bool NothrowMoves = std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>;

        struct Entry {
            T value;
            uint64_t seq;
        };

        // The heap's order: the greatest of the heap is the least by
        // Compare, and of two equal the one pushed first
        struct Order {
            bool operator()(const Entry& a, const Entry& b) const noexcept {
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

        priority_queue() noexcept(std::is_nothrow_default_constructible_v<Compare> && std::is_nothrow_copy_constructible_v<Compare>)
        : priority_queue(Compare()) {
        }

        explicit priority_queue(const Compare& comp) noexcept(std::is_nothrow_copy_constructible_v<Compare>)
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

        void push(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T> && NothrowMoves) {
            emplace(value);
        }

        void push(T&& value) noexcept(NothrowMoves) {
            emplace(std::move(value));
        }

        // The element constructed from a... and sifted into the heap under
        // the lock; the count of the pushes raised after, and a thread
        // waiting for an element woken. A throw from the constructor
        // leaves the queue as it was
        template<class... A>
        void emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...> && NothrowMoves) {
            {
                Guard guard(_lock);
                _heap.push_back(Entry{T(std::forward<A>(a)...), _seq++});
                if (_heap.size() > 1) {
                    _sift_up();
                }
                _count.store(_heap.size(), std::memory_order_relaxed);
            }
            _pushed.fetch_add(1, std::memory_order_seq_cst);
            if (_waiters.load(std::memory_order_seq_cst)) {
                _pushed.notify_all();
            }
        }

        // The least element, moved out, or nothing when the queue is empty
        optional<T> try_pop() noexcept(NothrowMoves) {
            Guard guard(_lock);
            if (_heap.empty()) {
                return nullopt;
            }
            if (_heap.size() > 1) {
                _pop_heap();
            }
            optional<T> value(std::move(_heap.back().value));
            _heap.pop_back();
            _count.store(_heap.size(), std::memory_order_relaxed);
            return value;
        }

        // The least element, waiting for one when the queue is empty: the
        // count of the pushes is read before the attempt, so a push that
        // came after the attempt found nothing changes the count and the
        // wait returns at once, and one that came before was found
        T pop() noexcept(NothrowMoves) {
            for (;;) {
                uint64_t pushed = _pushed.load(std::memory_order_seq_cst);
                if (auto value = try_pop()) {
                    return std::move(*value);
                }
                _wait(pushed);
            }
        }

        // A copy of the least element, or nothing when the queue is empty
        optional<T> try_top() const noexcept(std::is_nothrow_copy_constructible_v<T>) requires std::is_copy_constructible_v<T> {
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
        void clear() noexcept {
            Guard guard(_lock);
            _heap.clear();
            _count.store(0, std::memory_order_relaxed);
        }

        value_compare value_comp() const noexcept(std::is_nothrow_copy_constructible_v<Compare>) {
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

        // The element at the back of the heap sifted up to its place: the
        // steps of std::push_heap, the element taken out, each parent that
        // comes out after it moved down into the hole, the element put in
        // the hole where the climb stops. The comparator is noexcept; a
        // move of T that throws on the way undoes the climb (_unclimb) and
        // takes the element off, so that no moved-from entry is left in the
        // heap (the contents are then unspecified: push.md). The handlers
        // cost nothing until a throw, and none is compiled for a T whose
        // moves are noexcept. The heap holds two entries at least: the
        // caller tests that, so a heap of one costs no call and no saved
        // registers
        void _sift_up() noexcept(NothrowMoves) {
            size_t last = _heap.size() - 1;
            Entry* heap = _heap.data();
            size_t parent = (last - 1) / 2;
            if (!_order(heap[parent], heap[last])) {
                return;
            }
            Entry entry(std::move(heap[last]));
            size_t hole = last;
            try {
                do {
                    heap[hole] = std::move(heap[parent]);
                    hole = parent;
                    if (hole == 0) {
                        break;
                    }
                    parent = (hole - 1) / 2;
                } while (_order(heap[parent], entry));
            } catch (...) {
                _unclimb(last, hole);
                throw;
            }
            heap[hole] = std::move(entry);
        }

        // The climb from the back to the hole undone, after a throw: the
        // entries on the path, each moved one level down, moved back up from
        // the top, and the back (the element's place, moved-from now) taken
        // off. The path of the back is (last + 1) >> j - 1 for j = 0, 1...
        SGCL_NOINLINE void _unclimb(size_t last, size_t hole) noexcept(std::is_nothrow_move_assignable_v<Entry>) {
            Entry* heap = _heap.data();
            unsigned levels = 0;
            while (((last + 1) >> levels) - 1 != hole) {
                ++levels;
            }
            for (unsigned j = levels; j > 0; --j) {
                heap[((last + 1) >> j) - 1] = std::move(heap[((last + 1) >> (j - 1)) - 1]);
            }
            _heap.pop_back();
        }

        // The front of the heap moved to the back and the rest put in order:
        // the steps of std::pop_heap (Floyd's), the front taken out, the
        // greater child of the hole moved up into it down to a leaf, then
        // the back moved into the hole and sifted up, the front put at the
        // back. Both the descent and the climb run along one path from the
        // root, so a move of T that throws in either is undone the same
        // way (_undescend), and the back, when it was taken out, is put
        // back (the comparator is noexcept). std::pop_heap leaves such a
        // throw with the front lost and a moved-from entry in the heap. The
        // handlers cost nothing until a throw. The heap holds two entries
        // at least: the caller tests that
        void _pop_heap() noexcept(NothrowMoves) {
            size_t last = _heap.size() - 1;
            Entry* heap = _heap.data();
            Entry top(std::move(heap[0]));
            size_t hole = 0;
            bool climbs;
            try {
                do {
                    size_t child = 2 * hole + 1;
                    if (child < last && _order(heap[child], heap[child + 1])) {
                        ++child;
                    }
                    heap[hole] = std::move(heap[child]);
                    hole = child;
                } while (hole <= (last - 1) / 2);
                climbs = hole != last && _order(heap[(hole - 1) / 2], heap[last]);
            } catch (...) {
                _undescend(hole, top);
                throw;
            }
            if (climbs) {
                Entry entry(std::move(heap[last]));
                try {
                    do {
                        size_t parent = (hole - 1) / 2;
                        heap[hole] = std::move(heap[parent]);
                        hole = parent;
                    } while (hole > 0 && _order(heap[(hole - 1) / 2], entry));
                } catch (...) {
                    heap[last] = std::move(entry);
                    _undescend(hole, top);
                    throw;
                }
                heap[hole] = std::move(entry);
            } else if (hole != last) {
                heap[hole] = std::move(heap[last]);
            }
            heap[last] = std::move(top);
        }

        // The descent from the root to the hole undone, after a throw: the
        // entries on the path, each moved one level up, moved back down from
        // the hole, and the front put back at the root. A climb that came
        // after the descent moved the lower part of the path back already,
        // so the same steps from where it stopped undo both
        SGCL_NOINLINE void _undescend(size_t hole, Entry& top) noexcept(std::is_nothrow_move_assignable_v<Entry>) {
            Entry* heap = _heap.data();
            while (hole > 0) {
                size_t parent = (hole - 1) / 2;
                heap[hole] = std::move(heap[parent]);
                hole = parent;
            }
            heap[0] = std::move(top);
        }

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
