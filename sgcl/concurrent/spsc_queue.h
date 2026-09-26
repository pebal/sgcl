//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/atomic.h"
#include "../core/config.h"
#include "../core/detail/maker.h"
#include "../core/detail/os.h"
#include "../core/tracked_ptr.h"

#include <bit>
#include <type_traits>
#include <utility>

namespace sgcl::concurrent {
    namespace detail { using namespace sgcl::detail; }
    // A bounded wait-free FIFO queue for exactly one producer thread
    // and one consumer thread: a ring of cells, a power of two of them,
    // fixed at construction, with a sequence number per cell, the way
    // Vyukov's bounded queue (bounded_queue) numbers its
    // cells, and without its compare-exchange, since the producer alone
    // moves the tail and the consumer alone the head. A head and a tail
    // count up forever, a cell being the position masked; a cell's
    // sequence says whose the cell is: the position, once the cell is
    // free for a push at it; the position with the Published bit, once
    // the element at it is in. A push looks at the sequence of the cell
    // at the tail, constructs the element there and publishes the
    // cell; a pop looks at the sequence of the cell at the head, moves
    // the element out, destroys it, and frees the cell for the next
    // lap, the position plus the capacity. Each side reads its own
    // index only, which the other never touches: what the two sides
    // share is the cell, its sequence and its element on one line, one
    // line crossing between the cores per element and no more. The
    // ring of Lamport, with each side caching the other's index, shares
    // the indices instead: the consumer running right behind the
    // producer reloads the producer's tail at nearly every pop, the
    // index's line and the cell's both cross, and the producer takes
    // both back for its next push; measured, that ring took twice this
    // one's time per element with a consumer at the producer's heels,
    // and this one is no slower with room to run (spsc_queue.md). The
    // buffer is raw storage that the queue constructs elements in and
    // destroys them in, at push and pop time, as the vector does in its
    // buffer: the collector traces every cell (an unconstructed or
    // destroyed element holds null pointers only) and never destroys
    // one. The queue holds the buffer by a tracked_ptr, so it lives
    // where one may: on a stack or inside a managed object. push()
    // waits while the queue is full, on the cell at the tail, which the
    // pop that frees it notifies; pop() waits while it is empty, on the
    // cell at the head, which the push that publishes it notifies. The
    // producer's word, the consumer's and the ones both read are three
    // cache lines (config::cache_line_size). One producer and one
    // consumer, and no more: a second thread on either side is a data
    // race on that side's index. Several producers or consumers take a
    // bounded_queue.
    template<class T>
    class spsc_queue {
        // A cell: the sequence and the element, which is neither
        // constructed nor destroyed as a cell: the buffer is raw storage
        // (detail::Maker<Cell[]>::make_tracked_data), zeroed when the
        // element may hold tracked pointers, and the queue constructs
        // the element and destroys it in place.
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

        static constexpr size_t Published = size_t(1) << (sizeof(size_t) * 8 - 1);   // on a sequence: the element at the position is in

    public:
        using value_type = T;
        using size_type = size_t;

        // A queue of the capacity rounded up to a power of two, at least
        // one; the cells free, numbered from zero
        explicit spsc_queue(size_type capacity)
        : _mask(std::bit_ceil(capacity ? capacity : 1) - 1)
        , _cells(unique_ptr<Cell>(detail::Maker<Cell[]>::make_tracked_data(_mask + 1))) {
            for (size_type i = 0; i <= _mask; ++i) {
                _cell(i).sequence.store(i, std::memory_order_relaxed);
            }
        }

        spsc_queue(const spsc_queue&) = delete;
        spsc_queue& operator=(const spsc_queue&) = delete;

        // The elements still in the ring destroyed here, wherever the
        // queue dies: on a stack, or in a sweep inside a managed object.
        // No thread is in the queue any more: the elements are the cells
        // from the head to the tail, every one published (the tail
        // moves only once its element is in).
        ~spsc_queue() {
            if constexpr(!std::is_trivially_destructible_v<T>) {
                auto head = _head.load(std::memory_order_relaxed);
                auto tail = _tail.load(std::memory_order_relaxed);
                for (auto pos = head; pos != tail; ++pos) {
                    _cell(pos).value.~T();
                }
            }
        }

        // The element appended, or false when the queue is full
        bool try_push(const T& value) {
            return try_emplace(value);
        }

        bool try_push(T&& value) {
            return try_emplace(std::move(value));
        }

        // The element constructed from the arguments in the cell at the
        // tail, when that cell is free (its sequence the position: the
        // consumer has taken the element of the lap before); the tail
        // moves and the cell is published only once the element is
        // there, so a constructor that throws leaves the queue as it was
        template<class... A>
        bool try_emplace(A&&... a) {
            auto pos = _tail.load(std::memory_order_relaxed);   // the producer's own word
            auto& cell = _cell(pos);
            if (cell.sequence.load(std::memory_order_acquire) != pos) {   // the element of the lap before still in: full
                return false;
            }
            ::new (static_cast<void*>(&cell.value)) T(std::forward<A>(a)...);
            _tail.store(pos + 1, std::memory_order_relaxed);
            _publish(cell, pos | Published);   // the consumer waits on the cell
            return true;
        }

        // The element appended, waiting for room while the queue is
        // full: on the cell at the tail, which the pop that frees it
        // notifies
        void push(const T& value) {
            while (!try_push(value)) {
                auto pos = _tail.load(std::memory_order_relaxed);
                _wait(_cell(pos), pos);
            }
        }

        void push(T&& value) {
            while (!try_push(std::move(value))) {   // taken only by the attempt that finds room
                auto pos = _tail.load(std::memory_order_relaxed);
                _wait(_cell(pos), pos);
            }
        }

        // The first element, or nothing when the queue is empty: the
        // cell at the head not published
        optional<T> try_pop() {
            auto pos = _head.load(std::memory_order_relaxed);   // the consumer's own word
            auto& cell = _cell(pos);
            if (cell.sequence.load(std::memory_order_acquire) != (pos | Published)) {
                return nullopt;
            }
            optional<T> value(std::in_place, std::move(cell.value));
            cell.value.~T();
            _head.store(pos + 1, std::memory_order_relaxed);
            _publish(cell, pos + _mask + 1);   // free for the next lap: the producer waits on the cell
            return value;
        }

        // The first element, waiting for one while the queue is empty:
        // on the cell at the head, which the push that publishes it
        // notifies
        T pop() {
            for (;;) {
                if (auto value = try_pop()) {
                    return std::move(*value);
                }
                auto pos = _head.load(std::memory_order_relaxed);
                _wait(_cell(pos), pos | Published);
            }
        }

        // The number of cells: the capacity asked for, rounded up to a
        // power of two
        size_type capacity() const noexcept {
            return _mask + 1;
        }

        // The number of elements: the tail less the head, exact on the
        // producer's thread and the consumer's, of some moment elsewhere
        size_type size() const noexcept {
            auto head = _head.load(std::memory_order_acquire);
            auto tail = _tail.load(std::memory_order_acquire);
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
        Cell& _cell(size_type pos) const noexcept {
            return _cells.get()[pos & _mask];   // the pointer no thread writes after construction: a relaxed load
        }

        // The cell's sequence stored, and the other side woken when it
        // waits on the cell. A notify with nobody waiting is not free (a
        // fetch-add on a table the library shares), so it is gated on
        // the count of waiters, and the gate must lose no wakeup: the
        // store and the load here, the increment and the wait's load in
        // _wait, are seq_cst, so that either the waiter sees the
        // sequence changed or the notifier sees the waiter (a store
        // followed by a load on each side: the pattern only that order
        // makes safe). On arm64 the store is the release store it would
        // be anyway. One waiter at most on a cell: a side waits on the
        // cell at its own position, and the queue is not full and empty
        // at once.
        void _publish(Cell& cell, size_type seq) noexcept {
            cell.sequence.store(seq, std::memory_order_seq_cst);
            if (_waiters.load(std::memory_order_seq_cst)) {
                cell.sequence.notify_one();
            }
        }

        // The wait of push and pop for a cell's sequence to become
        // `wanted`: a spin of SpinPauses first, since a side that has
        // just caught up with the other is a few nanoseconds from its
        // next element, and a park in the kernel costs both sides a
        // system call; then the wait, counted. Out of line, so that push
        // and pop stay small enough to be inlined: inlined, the store of
        // a sequence and the load of the count after it overlap with
        // the next element's work; as a call, the pair drains at the
        // function's boundary and an element costs several times more
        // (measured, on the ring before this one: 2.5 ns against 20).
        SGCL_NOINLINE void _wait(Cell& cell, size_type wanted) noexcept {
            for (unsigned i = 0; i < SpinPauses; ++i) {
                if (cell.sequence.load(std::memory_order_acquire) == wanted) {
                    return;
                }
                detail::os::spin_pause();
            }
            _waiters.fetch_add(1, std::memory_order_seq_cst);
            for (auto seen = cell.sequence.load(std::memory_order_seq_cst); seen != wanted; seen = cell.sequence.load(std::memory_order_seq_cst)) {
                cell.sequence.wait(seen, std::memory_order_seq_cst);
            }
            _waiters.fetch_sub(1, std::memory_order_relaxed);
        }

        static constexpr unsigned SpinPauses = 1024;   // about 10 us on arm64 (isb: 9 ns), 40 on x86 (pause: 40 ns): the scheduler's window before a worker sleeps (config::worker_spin_microseconds)

        // The buffer and the mask, read by both sides and written by
        // neither, and the count of waiters, written only by a side
        // about to wait; the head, the consumer's line; the tail, the
        // producer's: the three groups a whole cache line apart. Padding
        // rather than alignas, so that the queue asks no alignment of
        // the object it is a member of, and a full line of it: the
        // object is aligned to 16, not to a line, and a group padded to
        // a line's length would end in the line the next group starts
        // in at one alignment in eight (measured on the ring before
        // this one: 3 ns per element or 30, by where the stack put the
        // queue).
        const size_type _mask;
        tracked_ptr<Cell> _cells;
        atomic<size_type> _waiters = {0};
        unsigned char _pad0[config::cache_line_size] = {};
        atomic<size_type> _head = {0};
        unsigned char _pad1[config::cache_line_size] = {};
        atomic<size_type> _tail = {0};
    };
}
