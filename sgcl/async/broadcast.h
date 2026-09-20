//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../concurrent/atomic.h"
#include "../concurrent/detail/backoff.h"
#include "../containers/dynamic_array.h"
#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "channel.h"
#include "coroutine.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace sgcl {
    // A broadcast channel: every subscriber receives every value sent
    // (tokio's broadcast, Kotlin's SharedFlow; an event bus). A channel
    // hands each element to one receiver; here `b.send(v)` goes to every
    // `subscription` alive, each reading at its own pace from one ring
    // shared by all: the sender's position and a cursor per subscription.
    // A subscription made later starts at the position of its making and
    // sees what is sent from then on. The ring holds `capacity` values
    // (rounded up to a power of two): a subscription that falls further
    // behind loses the oldest, its next receive is the oldest still
    // there, and `lagged()` says how many were lost before it (tokio's
    // Lagged(n), as a count beside the value). A value stays in the ring
    // until every subscription alive at its send has passed it (a
    // `tracked_ptr` value is held exactly that long), or until it is
    // lapped; a subscription dropped stops receiving and counts itself
    // off the values it has not passed. `close()` ends every
    // subscription: what was sent is still received, then nothing. A
    // subscription is received from the three ways of the module: a
    // thread blocks (`s.receive()`), a task `co_await s.async_receive()`s,
    // a select takes `s.on_receive(f)` as a case; a send never waits.
    //
    // How it is made: one word holds the positions reserved so far and
    // the number of subscriptions, together, so that a send takes the
    // next position and the count of the subscriptions that will read it
    // in one atomic add, and a subscription takes its cursor and counts
    // itself in with another: the count on a value is exact, and the
    // value's node lets the value go when it reaches zero. The slots of
    // the ring hold the values' nodes by atomic tracked pointers: a
    // reader loads the node, copies the value out of it and counts
    // itself off, and a sender overwriting the slot meanwhile frees
    // nothing (the node is the collector's when nothing holds it), which
    // is what lets readers read a slot no lock protects. Senders publish
    // in order: a second word counts the positions committed, advanced
    // by whichever sender finds the next slot stored (a sender that
    // stored ahead of a slower one leaves its commit to that one), so
    // that a reader waits for a position by its commit alone. The wait
    // is a channel of signals closed when a position is committed (a
    // round: made by the first reader to wait for the position, closed
    // and let go of by the sender that commits it), one round for all
    // the readers waiting, which all wait for the same position, the one
    // committed next. The three ways of the wait are the round channel's
    // three, and the value is taken after the wake. A send costs the
    // node's allocation and a few atomic operations; it does not touch
    // the subscriptions.
    template<class T>
    class broadcast {
        static_assert(std::is_copy_constructible_v<T>, "a broadcast value is copied to every subscription");

        // A value's node: the value, its position, the subscriptions
        // still to pass it (the value let go of at zero). Made, with the
        // value in it, before the position is reserved: nothing throws
        // between the reservation and the store.
        struct Node {
            explicit Node(T&& v)
            : value(std::in_place, std::move(v)) {
            }

            optional<T> value;
            size_t pos = 0;
            atomic<size_t> rem = {0};
        };

        struct Slot {
            atomic<tracked_ptr<Node>> node;
        };

        // The channel the readers waiting for a position wait on
        struct Round {
            explicit Round(size_t position) noexcept
            : position(position) {
            }

            channel<void> ch;
            const size_t position;
        };

        // A subscription's wait, kept with the subscription for its life
        // (tokio's shape: the waker in the receiver): the position it
        // waits for, its task's frame or its thread's park word, and a
        // link in the list of them the senders walk. A wait registers
        // the position; the sender that commits past it claims the
        // registration with a compare-exchange and wakes the one
        // subscriber, once; nothing is allocated and no list shared by
        // every waiter is pushed on per value. Unsubscribed: dead, and
        // unlinked by the next walk that passes it.
        struct Sub {
            static constexpr size_t None = SIZE_MAX;

            atomic<size_t> waiting = {None};
            tracked_ptr<detail::FrameWord> frame;   // a task's, set before the registration, taken by the sender that wakes it
            atomic<uint32_t> park = {0};            // a thread's: 1 = woken
            atomic<bool> alive = {true};
            atomic<tracked_ptr<Sub>> next;
        };

        // The ring and its words, shared by the broadcast and its subscriptions
        struct State {
            explicit State(size_t capacity)
            : ring(capacity) {
                ready.close();
            }

            Slot& slot(size_t pos) noexcept {
                return ring[pos & (ring.size() - 1)];
            }

            size_t reserved() const noexcept {
                return word.load(std::memory_order_seq_cst) & PositionMask;
            }

            dynamic_array<Slot> ring;
            atomic<uint64_t> word = {0};       // the positions reserved (the low bits) and the subscriptions alive (the high ones)
            atomic<size_t> committed = {0};    // the positions published, in order
            atomic<tracked_ptr<Round>> round;  // the select cases waiting for the next position, if any
            atomic<tracked_ptr<Sub>> subs;     // the subscriptions, for the wake of a receive
            channel<void> ready;               // closed at birth: the channel of a select case served at once
            atomic<bool> closed = {false};
        };

        static constexpr unsigned SpinBeforeWait = 1024;   // about 10 us on arm64 (isb: 9 ns), 40 on x86: the queues' window (spsc_queue.h)
        static constexpr unsigned PositionBits = 52;
        static constexpr uint64_t PositionMask = (uint64_t(1) << PositionBits) - 1;
        static constexpr uint64_t Subscriber = uint64_t(1) << PositionBits;
        static constexpr size_t MaxSubscriptions = (size_t(1) << (64 - PositionBits)) - 1;

    public:
        using value_type = T;
        using size_type = size_t;

        // A ring of `capacity` values, rounded up to a power of two (at least 1)
        explicit broadcast(size_type capacity)
        : _s(make_tracked<State>(std::bit_ceil(capacity ? capacity : 1))) {
        }

        broadcast(const broadcast&) = delete;
        broadcast& operator=(const broadcast&) = delete;

        // A subscription: a receiver with a cursor of its own into the
        // ring, starting at the next value sent
        class subscription {
        public:
            subscription() noexcept = default;

            subscription(subscription&& o) noexcept
            : _s(std::move(o._s))
            , _sub(std::move(o._sub))
            , _cursor(o._cursor)
            , _pending(o._pending)
            , _lagged(o._lagged) {
                o._s = nullptr;
                o._sub = nullptr;
            }

            subscription& operator=(subscription&& o) noexcept {
                if (this != &o) {
                    _unsubscribe();
                    _s = std::move(o._s);
                    _sub = std::move(o._sub);
                    _cursor = o._cursor;
                    _pending = o._pending;
                    _lagged = o._lagged;
                    o._s = nullptr;
                    o._sub = nullptr;
                }
                return *this;
            }

            subscription(const subscription&) = delete;
            subscription& operator=(const subscription&) = delete;

            // Counts itself off the values it has not passed
            ~subscription() {
                _unsubscribe();
            }

            // The next value, waiting for one: nothing once the broadcast
            // is closed and drained
            optional<T> receive() {
                for (;;) {
                    if (auto v = _take()) {
                        return v;
                    }
                    if (_s->closed.load(std::memory_order_seq_cst)) {
                        return _drain();
                    }
                    for (unsigned i = 0; i < SpinBeforeWait; ++i) {   // a sender at work is nanoseconds from the next commit: looked for before the park, which costs both sides a system call
                        if (_cursor < _s->committed.load(std::memory_order_acquire) || _s->closed.load(std::memory_order_acquire)) {
                            break;
                        }
                        detail::os::spin_pause();
                    }
                    if (_cursor < _s->committed.load(std::memory_order_seq_cst)) {
                        continue;
                    }
                    _sub->park.store(0, std::memory_order_relaxed);
                    if (_register()) {
                        continue;   // a value or the close came: taken back, round again
                    }
                    _sub->park.wait(0, std::memory_order_acquire);   // the sender that commits the position wakes this thread, once
                }
            }

            // The next value if one is there, waiting for nothing
            optional<T> try_receive() {
                return _take();
            }

            // `co_await s.async_receive()`: the next value, the task
            // suspended until one is sent; the wait is the round channel's
            class async_receive_op {
            public:
                bool await_ready() {
                    _value = _sub->_take();
                    if (_value) {
                        return true;
                    }
                    if (_sub->_s->closed.load(std::memory_order_seq_cst)) {
                        _value = _sub->_drain();
                        _done = true;
                        return true;
                    }
                    return false;
                }

                // The frame registered with the position; a value or the
                // close that came meanwhile takes the registration back
                // and the task goes on at once, unless a sender claimed
                // it first: then its wake is on its way and the task
                // suspends for it. Nothing of the frame is touched past
                // the registration the sender may have claimed.
                template<class P>
                bool await_suspend(std::coroutine_handle<P> h) {
                    _sub->_sub->frame = detail::frame_of(h);
                    return !_sub->_register();
                }

                optional<T> await_resume() {
                    if (_done) {
                        return std::move(_value);
                    }
                    if (_value) {
                        return optional<T>(std::in_place, std::move(*_value));
                    }
                    return _sub->_after_wake();
                }

            private:
                friend class subscription;

                explicit async_receive_op(subscription& s) noexcept
                : _sub(&s) {
                }

                subscription* _sub;
                optional<T> _value;
                bool _done = false;
            };

            async_receive_op async_receive() noexcept {
                return async_receive_op(*this);
            }

            // A case of a select (select.h): served when a value comes,
            // and f gets it (f(T); or f(optional<T>), which is also
            // called, with nothing, when the broadcast is closed and
            // drained). Built over the round channel's receive case, or
            // over a channel closed at birth when the value is there
            // already (the value is taken when the case is served, not
            // when it is made: another case may win)
            template<class F>
            class receive_case
            : public decltype(std::declval<channel<void>&>().on_receive(std::declval<F>())) {
                using Base = decltype(std::declval<channel<void>&>().on_receive(std::declval<F>()));

            public:
                receive_case(subscription& s, tracked_ptr<Round> round, F f)
                : Base(_channel(s, round).on_receive(std::move(f)))
                , _keep(std::move(round)) {
                }

            private:
                static channel<void>& _channel(subscription& s, const tracked_ptr<Round>& round) noexcept {
                    return round ? round->ch : s._s->ready;
                }

                tracked_ptr<Round> _keep;   // the round lives while the case does
            };

            template<class F>
            auto on_receive(F f) {
                tracked_ptr<Round> round = _ready_or_round();
                auto body = [this, f = std::move(f)]() mutable {
                    optional<T> v = _after_wake();
                    if constexpr (std::is_invocable_v<F&, optional<T>>) {
                        f(std::move(v));
                    } else if (v) {
                        f(std::move(*v));
                    }
                };
                return receive_case<decltype(body)>(*this, std::move(round), std::move(body));
            }

            // The values lost before the last one received: the ring
            // lapped this subscription by that many
            size_t lagged() const noexcept {
                return _lagged;
            }

            bool closed() const noexcept {
                return _s->closed.load(std::memory_order_acquire);
            }

            explicit operator bool() const noexcept {
                return (bool)_s;
            }

        private:
            friend class broadcast;

            subscription(tracked_ptr<State> s, tracked_ptr<Sub> sub, size_t cursor) noexcept
            : _s(std::move(s))
            , _sub(std::move(sub))
            , _cursor(cursor) {
            }

            // The value at the cursor, if it is committed: copied out of
            // its node and counted off; a cursor the ring has lapped moves
            // to the oldest position still in the ring and counts the
            // values it lost. The commit word is looked at before the
            // slot on purpose: a reader that looked at the slot first
            // spared itself the commit word's line on a hit, but polled
            // the slot's line while the sender was about to write it,
            // and with four readers at the senders' heels the sends
            // slowed by a fifth (measured, benchmarks.md: 1300 to 1550 ns
            // per value); the commit word is the one line the readers
            // share with the senders while they wait.
            optional<T> _take() {
                for (;;) {
                    if (_cursor >= _s->committed.load(std::memory_order_seq_cst)) {
                        return nullopt;
                    }
                    tracked_ptr<Node> n = _s->slot(_cursor).node.load(std::memory_order_acquire);
                    if (n->pos == _cursor) {
                        optional<T> v(std::in_place, *n->value);
                        ++_cursor;
                        _lagged = std::exchange(_pending, 0);
                        if (n->rem.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                            n->value.reset();
                        }
                        return v;
                    }
                    size_t oldest = _s->reserved() - _s->ring.size();   // lapped: the slot holds a later lap
                    _pending += oldest - _cursor;
                    _cursor = oldest;
                }
            }

            // The wait registered: the cursor as the position waited for,
            // then the look at the commit and the close (the sender: its
            // commit, then the look at the registrations; a fence on each
            // side, one of the two sees the other). True when something is
            // there and the registration was taken back: nothing to wait
            // for. False when the wait goes on: nothing there, or a sender
            // claimed the registration between the look and the take-back,
            // whose wake is on its way.
            // Nothing of the subscription is read past the registration:
            // it lives in the task's frame, and a sender that claims the
            // registration resumes the task, which may go round, move the
            // cursor and register again on another worker while this
            // call is still on its way out (the cursor read after the
            // store once took the new registration back, and the task was
            // resumed twice: a hang in one run of three).
            bool _register() {
                Sub* sub = _sub.get();
                State* s = _s.get();
                size_t cursor = _cursor;
                sub->waiting.store(cursor, std::memory_order_seq_cst);
                std::atomic_thread_fence(std::memory_order_seq_cst);
                if (cursor < s->committed.load(std::memory_order_seq_cst) || s->closed.load(std::memory_order_seq_cst)) {
                    size_t e = cursor;
                    if (sub->waiting.compare_exchange_strong(e, Sub::None, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                        sub->frame = nullptr;
                        return true;
                    }
                }
                return false;
            }

            // Whether a value is there or the broadcast is closed (a
            // select case served at once), else the round to wait on
            tracked_ptr<Round> _ready_or_round() {
                detail::Backoff<> backoff;
                for (;;) {
                    auto committed = _s->committed.load(std::memory_order_seq_cst);
                    if (_cursor < committed || _s->closed.load(std::memory_order_seq_cst)) {
                        return nullptr;
                    }
                    if (_cursor > committed) {
                        backoff();
                        continue;
                    }
                    tracked_ptr<Round> round = _round_for(_cursor);
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                    if (_cursor < _s->committed.load(std::memory_order_seq_cst) || _s->closed.load(std::memory_order_seq_cst)) {
                        return nullptr;
                    }
                    return round;
                }
            }

            // The round of the position: the one there, or a new one in
            // place of none or of a stale one (a position committed
            // already, the cursor is past it, whose committer has not
            // reached it yet: closed here, for the readers on it, since
            // the committer that finds the new one leaves it alone)
            tracked_ptr<Round> _round_for(size_t position) {
                tracked_ptr<Round> round = _s->round.load(std::memory_order_seq_cst);
                tracked_ptr<Round> fresh;   // made once: a lost exchange keeps it for the next try
                for (;;) {
                    if (round && round->position >= position) {
                        return round;
                    }
                    if (!fresh) {
                        fresh = make_tracked<Round>(position);
                    }
                    if (_s->round.compare_exchange_strong(round, fresh, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                        if (round) {
                            round->ch.close();
                        }
                        return fresh;
                    }
                }
            }

            // After a wake: the round closed by the commit of the
            // position the cursor was at (the value is there, or the ring
            // lapped the cursor, whose jump may land ahead of the commits:
            // a spin of a few instructions), or by the close
            optional<T> _after_wake() {
                detail::Backoff<> backoff;
                for (;;) {
                    if (auto v = _take()) {
                        return v;
                    }
                    if (_s->closed.load(std::memory_order_seq_cst)) {
                        return _drain();
                    }
                    backoff();
                }
            }

            // Closed: what was reserved before is published (a sender
            // still storing finishes: a few instructions), then what is there
            optional<T> _drain() {
                detail::Backoff<> backoff;
                while (_s->committed.load(std::memory_order_seq_cst) != _s->reserved()) {
                    backoff();
                }
                return _take();
            }

            // Counted out, and off every value between the cursor and the
            // position reserved last (the ones that counted it in); a
            // value lapped needs nothing
            void _unsubscribe() noexcept {
                if (!_s) {
                    return;
                }
                auto w = _s->word.fetch_sub(Subscriber, std::memory_order_seq_cst);
                size_t end = w & PositionMask;
                size_t from = std::max(_cursor, end > _s->ring.size() ? end - _s->ring.size() : 0);   // the positions the ring lapped need nothing (their nodes went with their slots): the walk is the ring at most, not the lag
                for (size_t pos = from; pos < end; ++pos) {
                    auto& slot = _s->slot(pos);
                    detail::Backoff<> backoff;
                    for (;;) {
                        tracked_ptr<Node> n = slot.node.load(std::memory_order_acquire);
                        if (n && n->pos >= pos) {
                            if (n->pos == pos && n->rem.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                                n->value.reset();
                            }
                            break;
                        }
                        backoff();   // the sender of the position is between its reservation and its store
                    }
                }
                _sub->alive.store(false, std::memory_order_release);   // unlinked by the next walk of the senders
                _sub = nullptr;
                _s = nullptr;
            }

            tracked_ptr<State> _s;
            tracked_ptr<Sub> _sub;
            size_t _cursor = 0;
            size_t _pending = 0;   // the values lost since the last one received
            size_t _lagged = 0;    // the values lost before the last one received
        };

        // A subscription starting at the next value sent
        subscription subscribe() {
            detail::Backoff<> backoff;   // after a lost exchange, as at every contended word of the module
            auto w = _s->word.load(std::memory_order_seq_cst);
            for (;;) {
                if ((w >> PositionBits) >= MaxSubscriptions) {
                    throw std::length_error("sgcl::broadcast::subscribe");
                }
                if (_s->word.compare_exchange_weak(w, w + Subscriber, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                    break;
                }
                backoff();
            }
            tracked_ptr<Sub> sub = make_tracked<Sub>();
            tracked_ptr<Sub> head = _s->subs.load(std::memory_order_acquire);
            for (;;) {
                sub->next.store(head, std::memory_order_relaxed);
                if (_s->subs.compare_exchange_weak(head, sub, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    break;
                }
                backoff();
            }
            return subscription(_s, std::move(sub), w & PositionMask);
        }

        // The value to every subscription alive, without waiting: true;
        // false when the broadcast is closed. A value nobody subscribes
        // to is dropped.
        bool send(const T& value) {
            return _send(T(value));
        }

        bool send(T&& value) {
            return _send(T(std::move(value)));
        }

        // No more sends: every subscription receives what was sent, then nothing
        void close() {
            if (_s->closed.exchange(true, std::memory_order_seq_cst)) {
                return;
            }
            _wake(SIZE_MAX);
        }

        bool closed() const noexcept {
            return _s->closed.load(std::memory_order_acquire);
        }

        size_type capacity() const noexcept {
            return _s->ring.size();
        }

        // The subscriptions alive
        size_type subscribers() const noexcept {
            return _s->word.load(std::memory_order_acquire) >> PositionBits;
        }

    private:
        bool _send(T v) {
            if (_s->closed.load(std::memory_order_seq_cst)) {
                return false;
            }
            tracked_ptr<Node> node = make_tracked<Node>(std::move(v));
            auto w = _s->word.fetch_add(1, std::memory_order_seq_cst);
            size_t pos = w & PositionMask;
            size_t subscribers = w >> PositionBits;
            node->pos = pos;
            node->rem.store(subscribers, std::memory_order_relaxed);
            if (!subscribers) {
                node->value.reset();   // nobody to read it: not held
            }
            auto& slot = _s->slot(pos);
            if (pos >= _s->ring.size()) {
                // The slot is taken for this lap only once the position of
                // the lap before is committed (the commits are in order, so
                // the slot then holds that position's node): a slot
                // overwritten while its position was stored and not yet
                // committed stopped the commits for good, the committer
                // finding a later lap where it looked for the position. So
                // the senders run at most a ring ahead of the commit point,
                // and a sender between its reservation and its store holds
                // up the ones a lap behind it for those few instructions.
                auto before = pos - _s->ring.size();
                detail::Backoff<> backoff;
                while (_s->committed.load(std::memory_order_seq_cst) <= before) {
                    backoff();
                }
            }
            slot.node.store(node, std::memory_order_seq_cst);
            _commit();
            return true;
        }

        // The positions committed advanced over every slot stored, by
        // whichever sender gets there; then the readers waiting for a
        // position now committed woken
        void _commit() {
            auto c = _s->committed.load(std::memory_order_seq_cst);
            auto from = c;
            for (;;) {
                tracked_ptr<Node> n = _s->slot(c).node.load(std::memory_order_seq_cst);
                if (!n || n->pos != c) {
                    break;   // not stored yet: its sender commits it, and what follows
                }
                if (_s->committed.compare_exchange_weak(c, c + 1, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                    ++c;
                }
            }
            if (c != from) {
                _wake(c);
            }
        }

        // The round closed if its position is below the commit (the
        // readers on it wait for a position now committed); a round for
        // a later position stays. SIZE_MAX: every round (the close).
        void _wake(size_t committed) {
            std::atomic_thread_fence(std::memory_order_seq_cst);   // the commit before the look at the registrations (_register: the registration before the look at the commit)
            tracked_ptr<Sub> pred;
            tracked_ptr<Sub> sub = _s->subs.load(std::memory_order_acquire);
            while (sub) {
                tracked_ptr<Sub> next = sub->next.load(std::memory_order_acquire);
                if (!sub->alive.load(std::memory_order_acquire)) {   // unsubscribed: unlinked here (a lost exchange leaves it to the next walk)
                    tracked_ptr<Sub> e = sub;
                    if (pred) {
                        pred->next.compare_exchange_strong(e, next, std::memory_order_acq_rel, std::memory_order_acquire);
                    } else {
                        _s->subs.compare_exchange_strong(e, next, std::memory_order_acq_rel, std::memory_order_acquire);
                    }
                    sub = std::move(next);
                    continue;
                }
                auto w = sub->waiting.load(std::memory_order_seq_cst);
                if (w < committed && sub->waiting.compare_exchange_strong(w, Sub::None, std::memory_order_seq_cst, std::memory_order_seq_cst)) {   // claimed: this wake is the one
                    if (tracked_ptr<detail::FrameWord> frame = sub->frame) {
                        sub->frame = nullptr;   // before the enqueue, not after: the task resumed may register again, with its frame, while this walk is still here (a frame nulled after the enqueue once wiped that registration's, and the next claim woke nobody: a hang in one run of four)
                        detail::enqueue(std::move(frame), false);
                    } else {
                        sub->park.store(1, std::memory_order_release);
                        sub->park.notify_one();
                    }
                }
                pred = std::move(sub);
                sub = std::move(next);
            }
            for (;;) {
                tracked_ptr<Round> round = _s->round.load(std::memory_order_seq_cst);
                if (!round || round->position >= committed) {
                    return;
                }
                if (_s->round.compare_exchange_strong(round, nullptr, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
                    round->ch.close();
                    return;
                }
            }
        }

        tracked_ptr<State> _s;
    };
}
