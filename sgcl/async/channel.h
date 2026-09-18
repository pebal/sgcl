//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/concurrent_queue.h"
#include "../concurrent/detail/backoff.h"
#include "../containers/array.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "coroutine.h"
#include "scheduler.h"

#include <atomic>
#include <bit>
#include <coroutine>
#include <cstdint>
#include <iterator>
#include <utility>

namespace sgcl {
    namespace detail {
        // The state one select shares between its cases: each case is a
        // waiter on its channel, and every claim or cancel of any of them
        // is a compare-exchange here, so that exactly one case is served
        // (`winner`: its index) and the rest are dead entries their
        // channels drop when they reach them. The thread parks here, the
        // coroutine's handle and frame are here.
        struct SelectState {
            enum State : int { Pending, Claimed, Cancelled, Registering, Served };   // Registering: a coroutine's, from the pushes to the look; Served: claimed and woken, for good (channel.h: Waiter)

            atomic<int> state = {Pending};
            atomic<uint32_t> signal = {0};
            std::coroutine_handle<> coro;
            tracked_ptr<FrameWord> frame;
            size_t winner = 0;
        };

        template<class... Cases>
        class Select;
    }

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
    // serves it hands it to the scheduler (scheduler.h), which runs it on
    // a worker; the serving thread returns at once. The buffer is a ring
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
        // thread waits on `signal`; a coroutine is made ready on the
        // scheduler through `coro`, its frame held here (`frame`) for the
        // length of the wait: the frame of a detached task has no other
        // holder.
        // A coroutine's waiter is pushed Registering and published Pending
        // only after the look at the channel that follows the push: a
        // waiter served before that look could have the coroutine resumed,
        // finished and the channel destroyed by its owner while the look
        // still reads it (a thread that waits blocks, and its channel
        // stays). A claim that finds a waiter Registering waits for the
        // publication, a fence and a few loads away, as a pop waits for a
        // slot reserved and not yet written.
        struct Waiter {
            enum State : int { Pending, Claimed, Cancelled, Registering, Served };   // Claimed is transient: the taker may put the waiter back (unclaim); Served is for good

            atomic<int> state = {Pending};
            atomic<uint32_t> signal = {0};
            std::coroutine_handle<> coro;
            tracked_ptr<detail::FrameWord> frame;
            optional<T> value;   // a waiting sender's element; the slot a waiting receiver is served in
            bool closed = false; // woken by close()
            tracked_ptr<detail::SelectState> select;   // the select this waiter is a case of, if any: the state is there
            size_t index = 0;                          // the case's index in it

            bool claim() noexcept {
                if (select) {
                    if (!_claim(select->state, detail::SelectState::Pending, detail::SelectState::Claimed, detail::SelectState::Registering)) {
                        return false;
                    }
                    select->winner = index;
                    return true;
                }
                return _claim(state, Pending, Claimed, Registering);
            }

            // A coroutine's waiter after the look: on the list for good, or
            // dropped (something was there: the coroutine goes round again)
            void publish() noexcept {
                state.store(Pending, std::memory_order_release);
            }

            void abandon() noexcept {
                state.store(Cancelled, std::memory_order_release);
            }

            bool cancel() noexcept {
                if (select) {
                    int e = detail::SelectState::Pending;
                    return select->state.compare_exchange_strong(e, detail::SelectState::Cancelled, std::memory_order_acq_rel, std::memory_order_acquire);
                }
                int e = Pending;
                return state.compare_exchange_strong(e, Cancelled, std::memory_order_acq_rel, std::memory_order_acquire);
            }

            void unclaim() noexcept {   // put back to pending, by the side that claimed and could not serve
                if (select) {
                    select->state.store(detail::SelectState::Pending, std::memory_order_release);
                } else {
                    state.store(Pending, std::memory_order_release);
                }
            }

            // Served, for good, before the wake: a take that meets a waiter
            // Served or Cancelled drops it, one that meets a waiter Claimed
            // keeps it (_take), since the claimer may put it back
            void wake() {
                if (select) {
                    select->state.store(detail::SelectState::Served, std::memory_order_release);
                } else {
                    state.store(Served, std::memory_order_release);
                }
                if (select) {
                    if (select->coro) {
                        detail::enqueue(std::move(select->frame), true);
                    } else {
                        select->signal.store(1, std::memory_order_release);
                        select->signal.notify_one();
                    }
                } else if (coro) {
                    detail::enqueue(std::move(frame), true);
                } else {
                    signal.store(1, std::memory_order_release);
                    signal.notify_one();
                }
            }

            void park() noexcept {
                signal.wait(0, std::memory_order_acquire);
            }

            // Claimed by a taker that has not served or put it back yet
            bool in_hand() const noexcept {
                return (select ? select->state.load(std::memory_order_acquire) : state.load(std::memory_order_acquire)) == Claimed;
            }

        private:
            static bool _claim(atomic<int>& s, int pending, int claimed, int registering) noexcept {
                detail::Backoff<RingBackoffMax> backoff;
                for (;;) {
                    int e = pending;
                    if (s.compare_exchange_strong(e, claimed, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }
                    if (e != registering) {
                        return false;
                    }
                    backoff();   // the owner is between the push and the publication
                }
            }
        };

        using WaiterPtr = tracked_ptr<Waiter>;

        // A list of waiters with a count of its entries beside it, for
        // the looks a buffered channel's fast path makes: a send that
        // pushed looks for a receiver to serve, a receive that popped for
        // a sender to move in, and the lists are empty nearly always; a
        // look at the queue's head is two loads through hazard pointers
        // and their temporaries (10 ns each, measured), a look at the
        // count one. The count is added to after the push and taken from
        // after a pop, whatever the entry's state: it counts entries, not
        // pending waiters, and never more than there are, so a count seen
        // is an entry linked. The fence protocol (_fence below) holds
        // with the count as the waiter's mark: the waiter pushes, counts,
        // fences and looks at the ring; the other side changes the ring,
        // fences and looks at the count; one of the two sees the other,
        // and a waiter whose count the other side missed sees the ring
        // and goes round. Only the buffered fast paths look at the count
        // (_try_send, _refill): a rendezvous serves every element through
        // the lists, and there the round a missed count costs (a
        // cancelled entry, a waiter made again) is the common case, not
        // the rare one (measured: sixteen threads on a rendezvous 615 to
        // 1130 ns per item with the takes gated, 3700 with the count
        // added before the push so that a taker never misses it, since a
        // taker that sees a count and pops nothing serves nobody).
        // A rendezvous keeps no count: nothing of it looks at the count,
        // and the two exchanges per entry on one line cost sixteen
        // threads on a rendezvous half as much again (615 to 910 ns per
        // item, measured).
        struct Waiters {
            explicit Waiters(bool counted) noexcept
            : counted(counted) {
            }

            void push(const WaiterPtr& w) {
                list.push(w);
                if (counted) {
                    count.fetch_add(1, std::memory_order_seq_cst);
                }
            }

            optional<WaiterPtr> try_pop() {
                auto w = list.try_pop();
                if (w && counted) {
                    count.fetch_sub(1, std::memory_order_relaxed);
                }
                return w;
            }

            bool empty() const noexcept {   // the count for a counted list, the queue's head for a rendezvous (channel::empty, the looks before a park)
                return counted ? count.load(std::memory_order_acquire) <= 0 : list.empty();
            }

            concurrent_queue<WaiterPtr> list;
            atomic<long> count = {0};
            const bool counted;
        };

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
        , _receivers(capacity != 0)
        , _senders(capacity != 0)
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
                std::atomic_thread_fence(std::memory_order_seq_cst);   // the push before the look (_fence)
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
        // managed frame; the coroutine is made ready by the thread that
        // serves it and runs on a worker of the scheduler
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

            // The waiter is Registering until the look at the channel that
            // follows the push is done (Waiter above): nobody serves it
            // before, so the coroutine, the awaiter and the channel are
            // all still here for the look. Once it is published another
            // thread may claim it and hand the coroutine to the scheduler
            // before this returns: nothing of the frame (this awaiter) is
            // touched past the publication
            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                for (;;) {
                    WaiterPtr me = make_tracked<Waiter>();
                    me->state.store(Waiter::Registering, std::memory_order_relaxed);
                    me->coro = h;
                    me->frame = detail::frame_of(h);
                    _me = me;
                    _ch._receivers.push(me);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                    if (!_ch._something_to_receive()) {
                        me->publish();
                        return true;   // suspended: served or woken later
                    }
                    me->abandon();
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
                return _ch._try_receive();   // woken by the close: what was sent before it, if anything is left
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

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {   // as for async_receive_op: Registering until the look, nothing of the frame touched past the publication
                for (;;) {
                    WaiterPtr me = make_tracked<Waiter>();
                    me->state.store(Waiter::Registering, std::memory_order_relaxed);
                    me->coro = h;
                    me->frame = detail::frame_of(h);
                    me->value.emplace(std::move(_value));
                    _me = me;
                    _ch._senders.push(me);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                    if (!_ch._something_to_send_to()) {
                        me->publish();
                        return true;
                    }
                    me->abandon();
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
            if (_closed.exchange(true, std::memory_order_seq_cst)) {   // seq_cst: the pair with the waiters' fence-then-load of the flag (_requeue, the registrations)
                return;
            }
            _close_list(_receivers);
            _close_list(_senders);
        }

        // Every waiter of a list woken by the close, once: a waiter in a
        // taker's hand (claimed, not yet served or put back) is left to it,
        // and its requeue, which looks at the flag, closes it (_requeue)
        void _close_list(Waiters& waiters) {
            Waiter* seen = nullptr;
            while (auto w = waiters.try_pop()) {
                if ((*w)->claim()) {
                    (*w)->closed = true;
                    (*w)->wake();
                } else if ((*w)->in_hand()) {
                    if (w->get() == seen) {
                        waiters.push(*w);
                        return;
                    }
                    if (!seen) {
                        seen = w->get();
                    }
                    waiters.push(*w);
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

        // The cases of a select (select.h): `ch.on_receive(f)` is served
        // when an element comes, and f gets it (f(T); or f(optional<T>),
        // which is also called, with nothing, when the channel is closed);
        // `ch.on_send(v, f)` when v is delivered, and f is called after
        // (a closed channel serves either case at once, without f)
        template<class F>
        class receive_case {
        public:
            using channel_type = channel;
            static constexpr bool is_send = false;

            receive_case(channel& ch, F f)
            : _ch(&ch)
            , _f(std::move(f)) {
            }

            channel& ch() const noexcept {
                return *_ch;
            }

            // The case served: its element, or nothing (closed)
            void run(optional<T>&& v) {
                if constexpr (std::is_invocable_v<F&, optional<T>>) {
                    _f(std::move(v));
                } else if (v) {
                    _f(std::move(*v));
                }
            }

            WaiterPtr waiter;

        private:
            channel* _ch;
            F _f;
        };

        template<class F>
        class send_case {
        public:
            using channel_type = channel;
            static constexpr bool is_send = true;

            send_case(channel& ch, T value, F f)
            : _ch(&ch)
            , _value(std::move(value))
            , _f(std::move(f)) {
            }

            channel& ch() const noexcept {
                return *_ch;
            }

            T& value() noexcept {
                return _value;
            }

            void run(bool delivered) {
                if (delivered) {
                    _f();
                }
            }

            WaiterPtr waiter;

        private:
            channel* _ch;
            T _value;
            F _f;
        };

        template<class F>
        receive_case<F> on_receive(F f) {
            return receive_case<F>(*this, std::move(f));
        }

        template<class F = void (*)()>
        send_case<F> on_send(const T& value, F f = [] {}) {
            return send_case<F>(*this, T(value), std::move(f));
        }

        template<class F = void (*)()>
        send_case<F> on_send(T&& value, F f = [] {}) {
            return send_case<F>(*this, T(std::move(value)), std::move(f));
        }

    private:
        template<class...> friend class detail::Select;

        // The fast path of a receive: the buffer, with the first waiting
        // sender's element moved in behind what was taken, so that the
        // order holds; when the buffer is empty, a waiting sender's element
        // is moved in the same way and taken from there, never directly
        // (a receiver that found the buffer empty may find it filled by
        // the time it reaches the senders, and an element taken directly
        // would jump the ones sent before it)
        // A sender whose element was moved in is released after this
        // receiver's last read of the channel, not before: released, it
        // may return from its send and let the channel go (a rendezvous
        // on the sender's stack), and a read after that is of nothing
        optional<T> _try_receive() {
            for (;;) {
                if (auto v = _pop()) {
                    if (_capacity) {   // a buffered channel: the first waiting sender's element moved in behind what was taken, so that the buffer stays full and the order holds. A rendezvous moves nothing in here: a waiting sender is released by the receive that takes its element (the second branch), never for an element of its own that nobody has taken
                        std::atomic_thread_fence(std::memory_order_seq_cst);   // the pop before the look at the senders (_fence)
                        if (auto w = _refill()) {
                            w->wake();
                        }
                    }
                    return v;
                }
                auto w = _refill();
                if (!w) {
                    return nullopt;
                }
                auto v = _pop();   // the element moved in, or one another receiver left behind; either way the sender is released now
                w->wake();
                if (v) {
                    return v;
                }
            }
        }

        // The fast paths of a send: a waiting receiver served directly,
        // when the buffer holds nothing and nothing is on its way into it
        // (an element sent before is in the buffer or about to be, and
        // must come out first), or the buffer when it has room
        bool _try_send(T& v) {
            if (_ring_empty() && (!_capacity || !_receivers.empty())) {
                if (auto w = _take(_receivers)) {
                    w->value.emplace(std::move(v));
                    w->wake();
                    return true;
                }
            }
            if (_capacity && _push(v, _capacity)) {
                std::atomic_thread_fence(std::memory_order_seq_cst);   // the push before the look at the list (_fence)
                // a receiver registered meanwhile: the ring's first element
                // is its (the order holds), taken here on its behalf; when
                // another receiver was quicker, its wait simply goes on. A
                // waiting receiver is woken with an element or with the
                // close, never to look for itself: a coroutine could not
                // wait again from where it resumes.
                // (a receiver claimed here is invisible to the other senders'
                // takes for a moment: one of them may publish, find no
                // receiver and go on; so after the receiver is put back the
                // ring is looked at again, and a receiver served if it can be)
                // (and every waiting receiver that an element is there for is
                // served, not the first alone: a slot reserved and not yet
                // written held the ones behind it back, and the sender that
                // finishes it is the one that finds them)
                auto w = _receivers.empty() ? WaiterPtr() : _take(_receivers);
                while (w) {
                    if (auto x = _try_receive()) {
                        w->value.emplace(std::move(*x));
                        w->wake();
                    } else {
                        _requeue(_receivers, w);
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                    }
                    w = _can_pop() && !_receivers.empty() ? _take(_receivers) : WaiterPtr();
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
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (_something_to_send_to() && me->cancel()) {
                    v = std::move(*me->value);
                    continue;
                }
                me->park();
                return !(me->closed && me->value);   // false only if the channel closed with the element still in hand
            }
        }

        // The first waiting sender's element moved into the ring (behind
        // whatever is there): the sender, for the caller to release once
        // it is done with the channel (_try_receive); null when no sender
        // waits or the ring has no room. The room is checked before the
        // sender is claimed; a sender that raced into the room since (a
        // push between the check and the claim) leaves the claimed one to
        // be put back (_requeue).
        WaiterPtr _refill() {
            auto limit = _capacity ? _capacity : _ring.size();
            if (!_can_push(limit) || (_capacity && _senders.empty())) {   // the count: a buffered channel's fast path (Waiters)
                return WaiterPtr();
            }
            // (the same for a sender claimed here and put back: a receiver
            // that popped meanwhile found no sender to refill from)
            // In a buffered channel every waiting sender the ring has room
            // for is moved in, the first given back and the others released
            // here: a slot reserved and not yet written held them back, and
            // the receiver that finishes it is the one that finds them. A
            // rendezvous moves one in per receive, so that a sender is
            // released by the receive that takes its element.
            WaiterPtr first;
            auto w = _take(_senders);
            while (w) {
                if (_push(*w->value, limit)) {
                    w->value.reset();
                    if (!first) {
                        first = w;
                    } else {
                        w->wake();
                    }
                    if (!_capacity) {
                        break;
                    }
                } else {
                    _requeue(_senders, w);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }
                w = _can_push(limit) ? _take(_senders) : WaiterPtr();
            }
            return first;
        }

        // A claimed waiter put back at the end of its list, pending again,
        // its thread still parked or its coroutine still suspended; told
        // of a close that may have passed the list meanwhile
        void _requeue(Waiters& waiters, const WaiterPtr& w) {
            w->unclaim();
            waiters.push(w);
            std::atomic_thread_fence(std::memory_order_seq_cst);   // the push before the look at the close (close: its exchange before its walk of the list): one of the two sees the other (_fence)
            if (_closed.load(std::memory_order_seq_cst) && w->claim()) {
                w->closed = true;
                w->wake();
            }
        }

        // The ring (Slot above): a push into a free slot up to `limit`
        // elements in the ring, false when full; a pop of the head's
        // element, nothing when the ring is empty or its head not yet
        // written
        bool _push(T& v, size_t limit) {
            detail::Backoff<RingBackoffMax> backoff;
            auto pos = _tail.load(std::memory_order_relaxed);
            for (;;) {
                auto room = (intptr_t)(pos - _head.load(std::memory_order_acquire));   // signed: the head loaded after the tail may have passed it (a push and a pop between the two loads), which is a stale tail, not a full ring
                if (room < 0) {
                    pos = _tail.load(std::memory_order_relaxed);
                    continue;
                }
                if (room >= (intptr_t)limit) {
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

        // The fences (marked _fence above): a waiter registers and then
        // looks at the other side, the other side changes and then looks
        // at the list; a store followed by a load may be reordered on
        // arm64, and both looks could miss, the waiter parked with its
        // element sitting there. A seq_cst fence between the store and
        // the load on each side makes one of the two see the other.
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
            return (intptr_t)(tail - _head.load(std::memory_order_acquire)) < (intptr_t)limit && _slot(tail).seq.load(std::memory_order_acquire) == tail;
        }

        bool _can_pop() const noexcept {
            auto head = _head.load(std::memory_order_acquire);
            return _slot(head).seq.load(std::memory_order_acquire) == head + 1;
        }

        // The first pending waiter of a list, claimed; cancelled and served
        // ones are dropped on the way. One claimed by another taker (a
        // case of a select claimed through another channel's list, or a
        // waiter of this one) is not dropped: the claimer may put it back
        // (unclaim, _requeue), and a waiter dropped meanwhile would never
        // be served again; it goes to the end of the list, and a take that
        // comes round to it again gives up for now (the claimer's requeue
        // or wake settles it, and a taker that then registers looks at
        // the list again: _something_to_receive, _something_to_send_to)
        static WaiterPtr _take(Waiters& waiters) {
            Waiter* seen = nullptr;
            while (auto w = waiters.try_pop()) {
                if ((*w)->claim()) {
                    return *w;
                }
                if ((*w)->in_hand()) {
                    if (w->get() == seen) {
                        waiters.push(*w);
                        return WaiterPtr();
                    }
                    if (!seen) {
                        seen = w->get();
                    }
                    waiters.push(*w);
                }
            }
            return WaiterPtr();
        }

        // What a registered receiver checks before parking: an element,
        // a sender or the close it might have missed
        bool _something_to_receive() const noexcept {
            return _can_pop() || !_senders.empty() || _closed.load(std::memory_order_acquire);
        }

        // A waiting receiver counts only with the ring empty: with an
        // element in it a sender cannot serve the receiver (the element
        // must come out first), and a stale entry of a receiver that
        // cancelled would keep a sender going round (registering,
        // cancelling, failing) while the receivers that could drain the
        // ring wait for a worker the sender holds
        bool _something_to_send_to() const noexcept {
            return (_ring_empty() && !_receivers.empty()) || (_capacity && _can_push(_capacity)) || _closed.load(std::memory_order_acquire);
        }

        static constexpr size_t MinSlots = 8;   // the ring of a rendezvous, or of a small capacity
        static constexpr unsigned RingBackoffMax = 32;   // the cap of the backoff at the ring's head and tail: a long pause here leaves a slot others wait for (measured: 8, 32 and 128 alike at sixteen threads, config::BackoffMax an order worse on a loaded machine)

        // The head and the tail a cache line apart (config::CacheLineSize):
        // the receivers' line and the senders' line
        array<Slot> _ring;
        atomic<size_t> _head = {0};
        unsigned char _pad[config::CacheLineSize - sizeof(atomic<size_t>)] = {};
        atomic<size_t> _tail = {0};
        Waiters _receivers;
        Waiters _senders;
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

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
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

        // The cases of a select: f() on a signal and on the close alike
        // (both end a wait for a signal); the send case as for any channel
        template<class F>
        auto on_receive(F f) {
            return _ch.on_receive([f = std::move(f)](optional<Signal>) mutable { f(); });
        }

        template<class F = void (*)()>
        auto on_send(F f = [] {}) {
            return _ch.on_send(Signal{}, std::move(f));
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
