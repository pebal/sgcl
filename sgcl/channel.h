//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "array.h"
#include "concurrent_queue.h"
#include "coroutine.h"
#include "detail/backoff.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <atomic>
#include <bit>
#include <coroutine>
#include <cstdint>
#include <iterator>
#include <utility>

namespace sgcl {
    // A channel of Go: a queue with the synchronization of both ends. A
    // channel of capacity n buffers n elements; one of capacity 0 buffers
    // none, and a send waits until a receive takes the element, so that
    // the pair is a meeting of the two threads (a rendezvous) and not a
    // delivery to a buffer. A receive on an empty channel waits, a send
    // on a full one waits: a producer ahead of its consumer simply stops
    // (back-pressure). close() ends the stream: what was sent is still
    // received, then every receive returns nothing at once and every send
    // returns false; a range-for over the channel runs until then.
    //
    // The waiting is done either by a thread, on an atomic of its own, or
    // by a coroutine: `co_await ch.async_receive()` and `co_await
    // ch.async_send(v)` suspend the coroutine (a task<T>, or any coroutine
    // whose promise derives from managed_frame, so that the tracked
    // pointers of its frame stay roots while it waits) with its handle on
    // the channel's list of waiters, and the send or the receive that
    // serves it resumes it, on the serving thread. The buffer is a ring
    // (below), a managed array of slots made once, the lists of waiters
    // are concurrent queues, and every waiter is a managed object:
    // nothing in the channel takes a lock, nothing frees anything, and a
    // waiter that is cancelled (its thread found the element itself) or
    // served is reclaimed by the collector. What a send does:
    // hand the element to a waiting receiver (when the buffer is empty),
    // or put it in the buffer if there is room, or wait with it until a
    // receiver moves it into the buffer; what a receive does: take from
    // the buffer, moving the first waiting sender's element in behind
    // (into an empty buffer too, and take it from there: the order of
    // the sends holds), or wait. Elements are delivered in the order they were sent by
    // each sender; between senders sending at the same moment the order
    // is theirs.
    //
    // The channel holds its queues by tracked_ptrs, so it lives where
    // one may: on a stack or inside a managed object.
    template<class T>
    class channel {
        // A waiting thread or coroutine: claimed with a compare-exchange
        // by the side that serves it (a sender gives it the element, a
        // receiver takes the element it holds), or cancelled by its own
        // side when the wait turned out unnecessary; whichever wins. A
        // thread waits on `signal`, a coroutine is resumed through `coro`;
        // the coroutine's frame is held by its task (frame_ptr) for the
        // length of the wait, as for any suspension.
        struct Waiter {
            enum State : int { Pending, Claimed, Cancelled };

            atomic<int> state = {Pending};
            atomic<uint32_t> signal = {0};
            std::coroutine_handle<> coro;
            optional<T> value;   // a waiting sender's element; the slot a waiting receiver is served in
            bool closed = false; // woken by close()

            bool claim() noexcept {
                int e = Pending;
                return state.compare_exchange_strong(e, Claimed, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            bool cancel() noexcept {
                int e = Pending;
                return state.compare_exchange_strong(e, Cancelled, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            void wake() {
                if (coro) {
                    coro.resume();
                } else {
                    signal.store(1, std::memory_order_release);
                    signal.notify_one();
                }
            }

            void park() noexcept {
                signal.wait(0, std::memory_order_acquire);
            }
        };

        using WaiterPtr = tracked_ptr<Waiter>;

        // The buffer: the bounded queue of Vyukov, a ring of slots with a
        // sequence number each, and a head and a tail that count up
        // forever. A slot whose sequence equals the tail is free for the
        // sender that wins the tail; the sender writes the element and
        // publishes the slot by setting its sequence to tail + 1, which is
        // what the receiver at that position waits to see; a receiver that
        // wins the head takes the element and sets the sequence to head +
        // the number of slots, the tail of the next lap. One compare-
        // exchange per operation, no allocation per element, and a slot
        // reserved but not yet written is nothing to a receiver (the
        // sequence says so), which is why a send that reserved a slot
        // wakes a waiting receiver after publishing it. The ring has a
        // power of two of slots, at least the capacity, and the capacity
        // is enforced apart (a channel of 3 has a ring of 4); a rendezvous
        // has a ring too, of a few slots, through which a waiting sender's
        // element passes to the receiver that serves it (_refill), so that
        // the waiting senders are served in their order.
        struct Slot {
            atomic<size_t> seq = {0};
            optional<T> value;
        };

    public:
        using value_type = T;
        using size_type = size_t;

        // A channel of capacity n; 0, the default, is a rendezvous
        explicit channel(size_type capacity = 0)
        : _ring(std::bit_ceil(capacity < MinSlots ? MinSlots : capacity))
        , _capacity(capacity) {
            for (size_t i = 0; i < _ring.size(); ++i) {
                _ring[i].seq.store(i, std::memory_order_relaxed);
            }
        }

        channel(const channel&) = delete;
        channel& operator=(const channel&) = delete;

        // Sends the element: delivered, and true; or false when the
        // channel is closed, before or while the send waits
        bool send(const T& value) {
            return _send(T(value));
        }

        bool send(T&& value) {
            return _send(T(std::move(value)));
        }

        // Sends without waiting: false when the channel is closed or full
        // (for a rendezvous: when no receiver waits)
        bool try_send(const T& value) {
            if (_closed.load(std::memory_order_acquire)) {
                return false;
            }
            T v(value);
            return _try_send(v);
        }

        bool try_send(T&& value) {
            if (_closed.load(std::memory_order_acquire)) {
                return false;
            }
            T v(std::move(value));
            return _try_send(v);
        }

        // Receives the next element, waiting for one; nothing once the
        // channel is closed and drained
        optional<T> receive() {
            for (;;) {
                if (auto v = _try_receive()) {
                    return v;
                }
                if (_closed.load(std::memory_order_acquire)) {
                    return _try_receive();   // what was sent before the close, and may have come since the look above
                }
                WaiterPtr me = make_tracked<Waiter>();
                _receivers.push(me);
                if (_something_to_receive() && me->cancel()) {
                    continue;
                }
                me->park();
                if (me->value) {
                    return optional<T>(std::in_place, std::move(*me->value));
                }
            }
        }

        // Receives without waiting: nothing when the channel is empty
        optional<T> try_receive() {
            return _try_receive();
        }

        // The awaitables: `co_await ch.async_receive()` is receive(), and
        // `co_await ch.async_send(v)` is send(v), for a coroutine with a
        // managed frame; the coroutine is resumed by the thread that serves
        // it, and runs on that thread until its next suspension
        class async_receive_op {
        public:
            bool await_ready() {
                _value = _ch._try_receive();
                if (!_value && _ch._closed.load(std::memory_order_acquire)) {
                    _value = _ch._try_receive();
                    return true;
                }
                return (bool)_value;
            }

            bool await_suspend(std::coroutine_handle<> h) {
                for (;;) {
                    _me = make_tracked<Waiter>();
                    _me->coro = h;
                    _ch._receivers.push(_me);
                    if (!_ch._something_to_receive() || !_me->cancel()) {
                        return true;   // suspended: served or woken later
                    }
                    _value = _ch._try_receive();
                    if (!_value && _ch._closed.load(std::memory_order_acquire)) {
                        _value = _ch._try_receive();
                        _me = nullptr;
                        return false;
                    }
                    if (_value) {
                        _me = nullptr;
                        return false;
                    }
                }
            }

            optional<T> await_resume() {
                if (_value) {
                    return optional<T>(std::in_place, std::move(*_value));
                }
                if (_me && _me->value) {
                    return optional<T>(std::in_place, std::move(*_me->value));
                }
                return nullopt;
            }

        private:
            friend class channel;

            explicit async_receive_op(channel& ch) noexcept
            : _ch(ch) {
            }

            channel& _ch;
            optional<T> _value;
            WaiterPtr _me;
        };

        class async_send_op {
        public:
            bool await_ready() {
                if (_ch._closed.load(std::memory_order_acquire)) {
                    _result = false;
                    return true;
                }
                if (_ch._try_send(_value)) {
                    _result = true;
                    return true;
                }
                return false;
            }

            bool await_suspend(std::coroutine_handle<> h) {
                for (;;) {
                    _me = make_tracked<Waiter>();
                    _me->coro = h;
                    _me->value.emplace(std::move(_value));
                    _ch._senders.push(_me);
                    if (!_ch._something_to_send_to() || !_me->cancel()) {
                        return true;
                    }
                    _value = std::move(*_me->value);
                    _me = nullptr;
                    if (_ch._closed.load(std::memory_order_acquire)) {
                        _result = false;
                        return false;
                    }
                    if (_ch._try_send(_value)) {
                        _result = true;
                        return false;
                    }
                }
            }

            bool await_resume() noexcept {
                if (_me) {
                    return !(_me->closed && _me->value);   // the element left with a receiver or the buffer, unless the channel closed on it
                }
                return _result;
            }

        private:
            friend class channel;

            async_send_op(channel& ch, T value) noexcept
            : _ch(ch)
            , _value(std::move(value)) {
            }

            channel& _ch;
            T _value;
            WaiterPtr _me;
            bool _result = false;
        };

        async_receive_op async_receive() noexcept {
            return async_receive_op(*this);
        }

        async_send_op async_send(const T& value) {
            return async_send_op(*this, T(value));
        }

        async_send_op async_send(T&& value) {
            return async_send_op(*this, T(std::move(value)));
        }

        // Closes the channel: every waiting receiver gets nothing, every
        // waiting sender false, every later send false; what was sent is
        // received first
        void close() {
            if (_closed.exchange(true, std::memory_order_acq_rel)) {
                return;
            }
            while (auto w = _receivers.try_pop()) {
                if ((*w)->claim()) {
                    (*w)->closed = true;
                    (*w)->wake();
                }
            }
            while (auto w = _senders.try_pop()) {
                if ((*w)->claim()) {
                    (*w)->closed = true;
                    (*w)->wake();
                }
            }
        }

        bool closed() const noexcept {
            return _closed.load(std::memory_order_acquire);
        }

        size_type capacity() const noexcept {
            return _capacity;
        }

        // The elements in the buffer (not the ones held by waiting senders)
        size_type size() const noexcept {
            auto head = _head.load(std::memory_order_acquire);
            auto tail = _tail.load(std::memory_order_acquire);
            return tail > head ? tail - head : 0;
        }

        bool empty() const noexcept {
            return size() == 0 && _senders.empty();
        }

        // A range-for over the channel: receive() until nothing comes
        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = T;
            using difference_type = ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            iterator() noexcept = default;

            reference operator*() noexcept {
                return *_value;
            }

            pointer operator->() noexcept {
                return &*_value;
            }

            iterator& operator++() {
                _value = _ch->receive();
                if (!_value) {
                    _ch = nullptr;
                }
                return *this;
            }

            void operator++(int) {
                ++*this;
            }

            bool operator==(const iterator& o) const noexcept {
                return _ch == o._ch;
            }

        private:
            friend class channel;

            explicit iterator(channel* ch)
            : _ch(ch) {
                ++*this;
            }

            channel* _ch = nullptr;
            optional<T> _value;
        };

        iterator begin() {
            return iterator(this);
        }

        iterator end() noexcept {
            return iterator();
        }

    private:
        // The fast path of a receive: the buffer, with the first waiting
        // sender's element moved in behind what was taken, so that the
        // order holds; when the buffer is empty, a waiting sender's element
        // is moved in the same way and taken from there, never directly
        // (a receiver that found the buffer empty may find it filled by
        // the time it reaches the senders, and an element taken directly
        // would jump the ones sent before it)
        optional<T> _try_receive() {
            for (;;) {
                if (auto v = _pop()) {
                    _refill();
                    return v;
                }
                if (!_refill()) {
                    return nullopt;
                }
            }
        }

        // The fast paths of a send: a waiting receiver served directly,
        // when the buffer holds nothing and nothing is on its way into it
        // (an element sent before is in the buffer or about to be, and
        // must come out first), or the buffer when it has room
        bool _try_send(T& v) {
            if (_ring_empty()) {
                if (auto w = _take(_receivers)) {
                    w->value.emplace(std::move(v));
                    w->wake();
                    return true;
                }
            }
            if (_capacity && _push(v, _capacity)) {
                if (auto w = _take(_receivers)) {   // a receiver registered meanwhile: it goes round and finds the buffer
                    w->wake();
                }
                return true;
            }
            return false;
        }

        bool _send(T v) {
            for (;;) {
                if (_closed.load(std::memory_order_acquire)) {
                    return false;
                }
                if (_try_send(v)) {
                    return true;
                }
                WaiterPtr me = make_tracked<Waiter>();
                me->value.emplace(std::move(v));
                _senders.push(me);
                if (_something_to_send_to() && me->cancel()) {
                    v = std::move(*me->value);
                    continue;
                }
                me->park();
                return !(me->closed && me->value);   // false only if the channel closed with the element still in hand
            }
        }

        // The first waiting sender's element moved into the ring (behind
        // whatever is there) and the sender released; false when no
        // sender waits or the ring has no room. The room is checked before
        // the sender is claimed; a sender that raced into the room since
        // (a push between the check and the claim) leaves the claimed one
        // to be put back at the end of the list, pending again, its
        // thread still parked, and told of a close that may have passed
        // the list meanwhile.
        bool _refill() {
            auto limit = _capacity ? _capacity : _ring.size();
            if (!_can_push(limit)) {
                return false;
            }
            if (auto w = _take(_senders)) {
                if (_push(*w->value, limit)) {
                    w->value.reset();
                    w->wake();
                    return true;
                }
                w->state.store(Waiter::Pending, std::memory_order_release);
                _senders.push(w);
                if (_closed.load(std::memory_order_acquire) && w->claim()) {
                    w->closed = true;
                    w->wake();
                }
            }
            return false;
        }

        // The ring (Slot above): a push into a free slot up to `limit`
        // elements in the ring, false when full; a pop of the head's
        // element, nothing when the ring is empty or its head not yet
        // written
        bool _push(T& v, size_t limit) {
            detail::Backoff<RingBackoffMax> backoff;
            auto pos = _tail.load(std::memory_order_relaxed);
            for (;;) {
                if (pos - _head.load(std::memory_order_acquire) >= limit) {
                    return false;
                }
                auto& slot = _slot(pos);
                auto seq = slot.seq.load(std::memory_order_acquire);
                auto dif = (intptr_t)seq - (intptr_t)pos;
                if (dif == 0) {
                    if (_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        slot.value.emplace(std::move(v));
                        slot.seq.store(pos + 1, std::memory_order_release);
                        return true;
                    }
                } else if (dif < 0) {
                    return false;
                } else {
                    pos = _tail.load(std::memory_order_relaxed);
                }
                backoff();   // a lost exchange or a stale tail: another sender is in
            }
        }

        optional<T> _pop() {
            detail::Backoff<RingBackoffMax> backoff;
            auto pos = _head.load(std::memory_order_relaxed);
            for (;;) {
                auto& slot = _slot(pos);
                auto seq = slot.seq.load(std::memory_order_acquire);
                auto dif = (intptr_t)seq - (intptr_t)(pos + 1);
                if (dif == 0) {
                    if (_head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        optional<T> v(std::in_place, std::move(*slot.value));
                        slot.value.reset();
                        slot.seq.store(pos + _ring.size(), std::memory_order_release);
                        return v;
                    }
                } else if (dif < 0) {
                    return nullopt;
                } else {
                    pos = _head.load(std::memory_order_relaxed);
                }
                backoff();
            }
        }

        Slot& _slot(size_t pos) noexcept {
            return _ring[pos & (_ring.size() - 1)];
        }

        const Slot& _slot(size_t pos) const noexcept {
            return _ring[pos & (_ring.size() - 1)];
        }

        // Nothing in the ring and nothing on its way into it
        bool _ring_empty() const noexcept {
            return _tail.load(std::memory_order_acquire) == _head.load(std::memory_order_acquire);
        }

        // What a push or a pop would find now: the slot at the tail free
        // (and the ring under its limit), the slot at the head written.
        // What a waiter checks after registering, and exactly what the
        // operation checks: a slot reserved by another thread and not yet
        // finished counts as neither, so the waiter parks instead of
        // going round (allocating a waiter each time) until that thread
        // is done; the thread that finishes wakes it, a sender through
        // _take(_receivers) after publishing, a receiver through _refill
        bool _can_push(size_t limit) const noexcept {
            auto tail = _tail.load(std::memory_order_acquire);
            return tail - _head.load(std::memory_order_acquire) < limit && _slot(tail).seq.load(std::memory_order_acquire) == tail;
        }

        bool _can_pop() const noexcept {
            auto head = _head.load(std::memory_order_acquire);
            return _slot(head).seq.load(std::memory_order_acquire) == head + 1;
        }

        // The first pending waiter of a list, claimed; cancelled ones are
        // dropped on the way
        static WaiterPtr _take(concurrent_queue<WaiterPtr>& waiters) {
            while (auto w = waiters.try_pop()) {
                if ((*w)->claim()) {
                    return *w;
                }
            }
            return WaiterPtr();
        }

        // What a registered receiver checks before parking: an element,
        // a sender or the close it might have missed
        bool _something_to_receive() const noexcept {
            return _can_pop() || !_senders.empty() || _closed.load(std::memory_order_acquire);
        }

        bool _something_to_send_to() const noexcept {
            return !_receivers.empty() || (_capacity && _can_push(_capacity)) || _closed.load(std::memory_order_acquire);
        }

        static constexpr size_t MinSlots = 8;   // the ring of a rendezvous, or of a small capacity
        static constexpr unsigned RingBackoffMax = 32;   // the cap of the backoff at the ring's head and tail: a long pause here leaves a slot others wait for (measured: 8, 32 and 128 alike at sixteen threads, config::BackoffMax an order worse on a loaded machine)

        // The head and the tail a cache line apart (config::CacheLineSize):
        // the receivers' line and the senders' line
        array<Slot> _ring;
        atomic<size_t> _head = {0};
        unsigned char _pad[config::CacheLineSize - sizeof(atomic<size_t>)] = {};
        atomic<size_t> _tail = {0};
        concurrent_queue<WaiterPtr> _receivers;
        concurrent_queue<WaiterPtr> _senders;
        atomic<bool> _closed = {false};
        const size_type _capacity;
    };

    // A channel of signals: send() carries nothing, receive() is whether
    // one came (false once closed)
    template<>
    class channel<void> {
        struct Signal {};

    public:
        using value_type = void;
        using size_type = size_t;

        explicit channel(size_type capacity = 0)
        : _ch(capacity) {
        }

        bool send() {
            return _ch.send(Signal{});
        }

        bool try_send() {
            return _ch.try_send(Signal{});
        }

        bool receive() {
            return _ch.receive().has_value();
        }

        bool try_receive() {
            return _ch.try_receive().has_value();
        }

        auto async_send() {
            return _ch.async_send(Signal{});
        }

        // co_await gives whether a signal came
        class async_receive_op {
        public:
            bool await_ready() {
                return _op.await_ready();
            }

            bool await_suspend(std::coroutine_handle<> h) {
                return _op.await_suspend(h);
            }

            bool await_resume() {
                return _op.await_resume().has_value();
            }

        private:
            friend class channel;

            explicit async_receive_op(channel<Signal>& ch) noexcept
            : _op(ch.async_receive()) {
            }

            typename channel<Signal>::async_receive_op _op;
        };

        async_receive_op async_receive() noexcept {
            return async_receive_op(_ch);
        }

        void close() {
            _ch.close();
        }

        bool closed() const noexcept {
            return _ch.closed();
        }

        size_type capacity() const noexcept {
            return _ch.capacity();
        }

        size_type size() const noexcept {
            return _ch.size();
        }

        bool empty() const noexcept {
            return _ch.empty();
        }

    private:
        channel<Signal> _ch;
    };
}
