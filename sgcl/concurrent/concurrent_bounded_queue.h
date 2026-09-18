//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/detail/maker.h"
#include "../core/detail/os.h"
#include "../core/tracked_ptr.h"
#include "atomic.h"
#include "detail/backoff.h"

#include <bit>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace sgcl {
    // A bounded lock-free FIFO queue shared by any number of producers
    // and consumers: the bounded MPMC queue of Dmitry Vyukov, the ring
    // of Go's buffered channel and of Java's ArrayBlockingQueue without
    // the lock. The elements live in a ring of cells, a power of two of
    // them, fixed at construction; each cell carries a sequence number,
    // and two counters, the enqueue and the dequeue position, count up
    // forever, a cell being the position masked. A cell whose sequence
    // equals the enqueue position is free for the producer that wins
    // the position with a compare-exchange; the producer constructs the
    // element in the cell and publishes it by storing the sequence as
    // the position plus one, which is what the consumer at that
    // position looks for; the consumer that wins the dequeue position
    // moves the element out, destroys it, and stores the sequence as
    // the position plus the number of cells: the enqueue position of
    // the next lap. One compare-exchange per operation, on a word the
    // producers share or on the one the consumers share, and no
    // allocation per element: the cells are one managed buffer, taken
    // once. A cell reserved and not yet published is nothing to a
    // consumer, and a cell taken and not yet released is nothing to a
    // producer: the sequence says so, and a thread that finds its
    // position stale reloads it, so no thread ever waits for another.
    // The buffer is raw storage that the queue constructs elements in
    // and destroys them in, at push and pop time, as the vector does in
    // its buffer: the collector traces every cell (an unconstructed or
    // destroyed element holds null pointers only) and never destroys
    // one. The queue holds the buffer by a tracked_ptr, so it lives
    // where one may: on a stack or inside a managed object. try_push
    // and try_pop are lock-free and linearizable at their exchange on
    // a position; push() waits while the queue is full and pop() while
    // it is empty, each on the sequence of the cell at its position,
    // which the other side's store to that cell notifies. The two
    // positions are kept a cache line apart (config::CacheLineSize).
    // An element's constructor may throw: a producer that won a
    // position has to publish its cell, or the consumers would stop at
    // it for good, so it publishes the cell empty, the top bit of the
    // sequence set (Empty), and the consumer that wins that position
    // releases the cell and goes on to the next; the exception reaches
    // the caller of the push with the queue as it was.
    template<class T>
    class concurrent_bounded_queue {
        // A cell: the sequence and the storage of one element. A union
        // with the element as its only member (the rules, 2: one member,
        // so the element's pointers keep offsets of their own), never
        // constructed or destroyed as a cell: the buffer is raw storage
        // (detail::Maker<Cell[]>::make_tracked_data), zeroed when the
        // element may hold tracked pointers, and the queue constructs the
        // element and destroys it in place.
        struct Cell {
            Cell() noexcept {
            }

            ~Cell() {
            }

            atomic<size_t> sequence;
            union {
                T value;
            };
        };

        static constexpr size_t Empty = size_t(1) << (sizeof(size_t) * 8 - 1);   // on a sequence: published without an element

    public:
        using value_type = T;
        using size_type = size_t;

        // A queue of the capacity rounded up to a power of two, at least
        // two (in a ring of one, a cell published at a position and free
        // at the next would carry one number); the cells free, numbered
        // from zero
        explicit concurrent_bounded_queue(size_type capacity)
        : _mask(std::bit_ceil(capacity < 2 ? size_type(2) : capacity) - 1)
        , _cells(unique_ptr<Cell>(detail::Maker<Cell[]>::make_tracked_data(_mask + 1))) {
            for (size_type i = 0; i <= _mask; ++i) {
                _cell(i).sequence.store(i, std::memory_order_relaxed);
            }
        }

        concurrent_bounded_queue(const concurrent_bounded_queue&) = delete;
        concurrent_bounded_queue& operator=(const concurrent_bounded_queue&) = delete;

        // The elements still in the ring destroyed here, wherever the
        // queue dies: on a stack, or in a sweep inside a managed object.
        // No thread is in the queue any more, so the elements are the
        // published cells from the dequeue position to the enqueue one.
        ~concurrent_bounded_queue() {
            if constexpr(!std::is_trivially_destructible_v<T>) {
                auto head = _dequeue_pos.load(std::memory_order_relaxed);
                auto tail = _enqueue_pos.load(std::memory_order_relaxed);
                for (auto pos = head; pos != tail; ++pos) {
                    auto& cell = _cell(pos);
                    if (cell.sequence.load(std::memory_order_relaxed) == pos + 1) {
                        cell.value.~T();
                    }
                }
            }
        }

        // The element appended, or false when the queue is full at the
        // moment of the exchange
        bool try_push(const T& value) {
            return try_emplace(value);
        }

        bool try_push(T&& value) {
            return try_emplace(std::move(value));
        }

        // The element constructed from the arguments in its cell
        template<class... A>
        bool try_emplace(A&&... a) {
            size_type pos, seq;
            return _try_push(pos, seq, std::forward<A>(a)...);
        }

        // The element appended, waiting for room while the queue is
        // full: on the sequence of the cell at the enqueue position,
        // which the consumer releasing it notifies
        void push(T value) {
            for (;;) {
                size_type pos, seq;
                if (_try_push(pos, seq, std::move(value))) {
                    return;
                }
                _wait(_cell(pos), seq);
            }
        }

        // The first element, or nothing when the queue is empty at the
        // moment of the look
        optional<T> try_pop() {
            size_type pos, seq;
            return _try_pop(pos, seq);
        }

        // The first element, waiting for one while the queue is empty:
        // on the sequence of the cell at the dequeue position, which
        // the producer publishing it notifies
        T pop() {
            for (;;) {
                size_type pos, seq;
                if (auto value = _try_pop(pos, seq)) {
                    return std::move(*value);
                }
                _wait(_cell(pos), seq);   // the sequence as the try saw it, as push waits (the cell may still be the previous lap's consumer's: a wait on the position would return at once, a spin)
            }
        }

        // The number of cells: the capacity asked for, rounded up to a
        // power of two
        size_type capacity() const noexcept {
            return _mask + 1;
        }

        // The number of elements at some moment: the enqueue position
        // less the dequeue one, a cell reserved by a push or a pop in
        // progress counted with the side that reserved it
        size_type size() const noexcept {
            auto head = _dequeue_pos.load(std::memory_order_acquire);
            auto tail = _enqueue_pos.load(std::memory_order_acquire);
            auto n = tail - head;
            return n < capacity() ? n : capacity();
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        bool full() const noexcept {
            return size() == capacity();
        }

    private:
        // The cap of the backoff at a position. Measured (8 producers and
        // 8 consumers, 200 k items each, best of five, ns per item): at
        // 32 pauses 257 with a ring of 64 and 228 with one of 1024, at
        // 128 150 and 114, at 256 121 and 90, at 1024 80 and 60, at 4096
        // (config::BackoffMax) 144 and 70; two and four threads within
        // their noise from 32 to 1024. Sixteen threads at two words are a
        // storm of lost exchanges, and only a pause long enough to let
        // the winner finish turns it into near-serial exchanges, as the
        // stack's backoff does at its head; past that a pause leaves a
        // cell that the other side is already waiting for.
        static constexpr unsigned BackoffMax = 1024;

        Cell& _cell(size_type pos) const noexcept {
            return _cells.get()[pos & _mask];   // the pointer no thread writes after construction: a relaxed load
        }

        // A push at the enqueue position: the cell there free (its
        // sequence the position) and the position won, the element
        // constructed and the cell published; or false, with the
        // position and the sequence seen, when the cell still holds the
        // element of the previous lap (the queue full). A sequence ahead
        // of the position is a position another producer won meanwhile:
        // reloaded.
        template<class... A>
        bool _try_push(size_type& pos, size_type& seq, A&&... a) {
            detail::Backoff<BackoffMax> backoff;
            pos = _enqueue_pos.load(std::memory_order_relaxed);
            for (;;) {
                auto& cell = _cell(pos);
                seq = cell.sequence.load(std::memory_order_acquire);
                auto dif = (intptr_t)(seq & ~Empty) - (intptr_t)pos;
                if (dif == 0) {
                    if (_enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        try {
                            ::new (static_cast<void*>(&cell.value)) T(std::forward<A>(a)...);
                        } catch (...) {
                            _publish(cell, (pos + 1) | Empty);   // the position won: published without an element
                            throw;
                        }
                        _publish(cell, pos + 1);
                        return true;
                    }
                } else if (dif < 0) {
                    return false;
                } else {
                    pos = _enqueue_pos.load(std::memory_order_relaxed);
                }
                backoff();   // a lost exchange or a stale position: another producer is in
            }
        }

        // A pop at the dequeue position: the cell there published (its
        // sequence the position plus one) and the position won, the
        // element moved out and destroyed, the cell released for the
        // next lap; or nothing, with the position and the sequence seen,
        // when the cell is not published yet (the queue empty, or its
        // first element still being written, or the cell not yet
        // released by the consumer of the lap before). A cell published empty is released and
        // the walk goes on to the next position. An element whose move
        // throws is destroyed and its cell released, the exception
        // rethrown: the element is lost, the queue intact.
        optional<T> _try_pop(size_type& pos, size_type& seq) {
            detail::Backoff<BackoffMax> backoff;
            pos = _dequeue_pos.load(std::memory_order_relaxed);
            for (;;) {
                auto& cell = _cell(pos);
                seq = cell.sequence.load(std::memory_order_acquire);
                auto dif = (intptr_t)(seq & ~Empty) - (intptr_t)(pos + 1);
                if (dif == 0) {
                    if (_dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        if (seq & Empty) {
                            _publish(cell, pos + _mask + 1);
                            pos = _dequeue_pos.load(std::memory_order_relaxed);
                            continue;
                        }
                        optional<T> value;
                        try {
                            value.emplace(std::move(cell.value));
                        } catch (...) {
                            cell.value.~T();
                            _publish(cell, pos + _mask + 1);
                            throw;
                        }
                        cell.value.~T();
                        _publish(cell, pos + _mask + 1);
                        return value;
                    }
                } else if (dif < 0) {
                    return nullopt;
                } else {
                    pos = _dequeue_pos.load(std::memory_order_relaxed);
                }
                backoff();   // a lost exchange or a stale position: another consumer is in
            }
        }

        // The cell's sequence stored, and the threads waiting on it
        // woken when there are any: a consumer waits on the cell at its
        // position for a push, a producer for a pop. A notify with
        // nobody waiting is not free (a fetch-add on a table the
        // library shares), so it is gated on the count of waiters, and
        // the gate must lose no wakeup: the store and the load here, the
        // increment and the wait's load in _wait, are seq_cst, so that
        // either the waiter sees the sequence changed or the notifier
        // sees the waiter (a store followed by a load on each side: the
        // pattern only that order makes safe). On arm64 the store is the
        // release store it would be anyway.
        void _publish(Cell& cell, size_t seq) noexcept {
            cell.sequence.store(seq, std::memory_order_seq_cst);
            if (_waiters.load(std::memory_order_seq_cst)) {
                cell.sequence.notify_all();
            }
        }

        // The wait of push and pop on a cell's sequence: a spin of
        // SpinPauses first, since a side that has just caught up with
        // the other is a few nanoseconds from its next element, and a
        // park in the kernel costs both sides a system call; then the
        // wait, counted. Out of line, so that push and pop stay small
        // enough to be inlined: inlined, the store of a sequence and
        // the load of the count after it overlap with the next
        // element's work; as a call, the pair drains at the function's
        // boundary, and a push or pop between two threads costs several
        // times what it should (measured on spsc_queue.h: 2.5 ns against 20).
        SGCL_NOINLINE void _wait(Cell& cell, size_t seen) noexcept {
            for (unsigned i = 0; i < SpinPauses; ++i) {
                if (cell.sequence.load(std::memory_order_acquire) != seen) {
                    return;
                }
                detail::os::spin_pause();
            }
            _waiters.fetch_add(1, std::memory_order_seq_cst);
            cell.sequence.wait(seen, std::memory_order_seq_cst);
            _waiters.fetch_sub(1, std::memory_order_relaxed);
        }

        static constexpr unsigned SpinPauses = 1024;   // about 10 us on arm64 (isb: 9 ns), 40 on x86 (pause: 40 ns): the scheduler's window before a worker sleeps (config::WorkerSpinMicroseconds)

        // The buffer and the mask, read by every thread and written by
        // none, and the count of waiters, written only by a thread
        // about to wait; the enqueue position, the producers' word; the
        // dequeue position, the consumers': the three groups a whole
        // cache line apart. Padding rather than alignas, so that the
        // queue asks no alignment of the object it is a member of, and
        // a full line of it: the object is aligned to 16, not to a
        // line, and a group of three words padded to a line's length
        // would end in the line the next group starts in at one
        // alignment in eight (measured: 3 ns per element or 30, by
        // where the stack put the queue).
        const size_type _mask;
        tracked_ptr<Cell> _cells;
        atomic<size_type> _waiters = {0};
        unsigned char _pad0[config::CacheLineSize] = {};
        atomic<size_type> _enqueue_pos = {0};
        unsigned char _pad1[config::CacheLineSize] = {};
        atomic<size_type> _dequeue_pos = {0};
    };
}
