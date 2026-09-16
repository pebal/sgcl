//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "concurrent_queue.h"
#include "coroutine.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <atomic>
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
    // serves it resumes it, on the serving thread. The
    // buffer is a concurrent_queue, the lists of waiters are concurrent
    // queues too, and every waiter is a managed object: nothing in the
    // channel takes a lock, nothing frees anything, and a waiter that is
    // cancelled (its thread found the element itself) or served is
    // reclaimed by the collector, as the elements are. What a send does:
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

    public:
        using value_type = T;
        using size_type = size_t;

        // A channel of capacity n; 0, the default, is a rendezvous
        explicit channel(size_type capacity = 0)
        : _capacity(capacity) {
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
            long n = _count.load(std::memory_order_acquire);
            return n > 0 ? size_type(n) : 0;
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
                if (auto v = _items.try_pop()) {
                    _count.fetch_sub(1, std::memory_order_acq_rel);
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
            if (_count.load(std::memory_order_acquire) == 0) {
                if (auto w = _take(_receivers)) {
                    w->value.emplace(std::move(v));
                    w->wake();
                    return true;
                }
            }
            if (_capacity) {
                long n = _count.fetch_add(1, std::memory_order_acq_rel) + 1;
                if (n <= long(_capacity)) {
                    _items.emplace(std::move(v));
                    if (auto w = _take(_receivers)) {   // a receiver registered meanwhile: it goes round and finds the buffer
                        w->wake();
                    }
                    return true;
                }
                _count.fetch_sub(1, std::memory_order_acq_rel);
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

        // The first waiting sender's element moved into the buffer (behind
        // whatever is there: the buffer may stand above its capacity for
        // the moment a receiver takes to pop it) and the sender released;
        // false when no sender waits
        bool _refill() {
            if (auto w = _take(_senders)) {
                _count.fetch_add(1, std::memory_order_acq_rel);
                _items.emplace(std::move(*w->value));
                w->value.reset();
                w->wake();
                return true;
            }
            return false;
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
            return _count.load(std::memory_order_acquire) > 0 || !_senders.empty() || _closed.load(std::memory_order_acquire);
        }

        bool _something_to_send_to() const noexcept {
            return !_receivers.empty() || (_capacity && _count.load(std::memory_order_acquire) < long(_capacity)) || _closed.load(std::memory_order_acquire);
        }

        concurrent_queue<T> _items;
        concurrent_queue<WaiterPtr> _receivers;
        concurrent_queue<WaiterPtr> _senders;
        atomic<long> _count = {0};
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
