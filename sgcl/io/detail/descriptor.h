//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../async/channel.h"
#include "../../async/reactor.h"
#include "../../async/timer.h"
#include "../../core/vector.h"
#include "../../core/aliases.h"
#include "../../core/tracked_ptr.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unistd.h>

namespace sgcl::io::detail {
    // How a wait for readiness ended: the descriptor is ready (or the wait
    // was cut short for a reason that is not an error, and the caller
    // looks again), it was closed, its deadline passed, or the reactor
    // stopped under it (scheduler::stop)
    enum class WaitResult {
        ready,
        closed,
        timed_out,
        cancelled
    };

    // A descriptor shared by the tasks and threads that use one file or
    // one socket (io::file, net's sockets):
    // Go's fdMutex and pollDesc in one, written for this reactor.
    //
    // The race it closes: a task reads the number, another closes the
    // descriptor, the kernel gives the number to the next socket opened,
    // and the first task's recv lands on a stranger's connection. So every
    // operation holds the descriptor for as long as it runs (acquire,
    // release: a count in one atomic word), close() only sets the closing
    // bit in that word and wakes the waits in progress, and the ::close
    // itself is made by whoever lets go of the word last, the close or the
    // last operation. After the bit is set no operation starts; the number
    // is the descriptor's own until the ::close.
    //
    // The wait of an operation is a one-shot registration with the
    // reactor (readable, writable): a channel signalled when the
    // descriptor is ready. The descriptor keeps the channels of the waits
    // in progress, and wakes a wait by closing its channel, so that the
    // wait ends with nothing: on a close, when the deadline of its
    // direction passes (a timer armed for the wait), when a deadline is
    // changed while it waits. A wait woken with nothing tells the causes
    // apart afterwards: closing is a close, a deadline in the past a
    // timeout, a generation moved (every change of a deadline moves it) a
    // look again, and none of these the reactor stopping. The
    // registration with the kernel stays until the descriptor is ready,
    // its wait then already over, or until the ::close, before which
    // cancel_waits drops it: the reactor's cancel frees the entry the
    // kernel hands back, so it is called only right before the descriptor
    // is closed, never on one that stays open.
    //
    // A wait registers its channel, then looks at the closing bit and the
    // generation; a close or a change sets its flag, then closes the
    // channels registered; both under one lock: either the waker finds
    // the channel, or the wait sees the flag.
    //
    // The descriptor lives inside the managed object of its file or
    // socket; its destructor, on the collector's thread after the sweep
    // that finds the owner dead, closes a descriptor nobody closed. One
    // that is not owned (the standard streams) is never given back to the
    // kernel: a close only ends the operations on it.
    class Descriptor {
    public:
        static constexpr int Read = 0;
        static constexpr int Write = 1;

        explicit Descriptor(int fd, bool owns = true) noexcept
        : _fd(fd)
        , _owns(owns) {
        }

        Descriptor(const Descriptor&) = delete;
        Descriptor& operator=(const Descriptor&) = delete;

        ~Descriptor() {
            if (!(_state.load(std::memory_order_acquire) & Closed)) {
                sgcl::async::cancel_waits(_fd);
                if (_owns) {
                    ::close(_fd);
                }
            }
        }

        int fd() const noexcept {
            return _fd;
        }

        // An operation begins: false once close() has been called
        bool acquire() noexcept {
            uint64_t s = _state.load(std::memory_order_relaxed);
            do {
                if (s & Closing) {
                    return false;
                }
            } while (!_state.compare_exchange_weak(s, s + 1, std::memory_order_seq_cst, std::memory_order_relaxed));
            return true;
        }

        // The operation ends; the last one after a close closes
        void release() noexcept {
            (void)_release();
        }

        // Whether close() has been called
        bool closing() const noexcept {
            return (_state.load(std::memory_order_seq_cst) & Closing) != 0;
        }

        // The closing bit set and the waits in progress woken; the ::close
        // now when no operation runs, else by the last to end. Holds the
        // word itself meanwhile, so that the number cannot be closed and
        // given to another descriptor under it. The error of the ::close
        // when it was made here (0 when it went well, or was left to an
        // operation, or was made before)
        int close() noexcept {
            uint64_t s = _state.load(std::memory_order_relaxed);
            do {
                if (s & Closing) {
                    return 0;
                }
            } while (!_state.compare_exchange_weak(s, (s | Closing) + 1, std::memory_order_seq_cst, std::memory_order_relaxed));
            _wake_waits();
            return _release();
        }

        // The deadline of a direction, time_point{} for none; a change
        // wakes the waits in progress, which look at it again
        void set_deadline(int dir, time_point t) {
            _deadline[dir].store(_rep(t), std::memory_order_seq_cst);
            _generation.fetch_add(1, std::memory_order_seq_cst);
            _wake_waits();
        }

        time_point deadline(int dir) const noexcept {
            return _point(_deadline[dir].load(std::memory_order_seq_cst));
        }

        // Whether the direction's deadline is set and has passed
        bool expired(int dir) const noexcept {
            auto d = _deadline[dir].load(std::memory_order_seq_cst);
            return d != 0 && sgcl::clock::now() >= _point(d);
        }

        // One wait for the direction's readiness, in two halves around the
        // wait on the channel, so that a thread (channel->receive()) and a
        // task (co_await channel->receive()) share them:
        //
        //     auto w = d.begin_wait(dir);
        //     auto r = w.done ? w.result : d.end_wait(w, w.channel->receive().wait());
        //
        // begin_wait registers with the reactor, and with the descriptor,
        // and arms the deadline's timer; it answers at once (done) when the
        // descriptor is closing, the deadline has passed, or the generation
        // moved meanwhile.
        struct Wait {
            tracked_ptr<async::channel<void>> channel;
            tracked_ptr<sgcl::async::detail::Timer> timer;
            uint64_t generation = 0;
            int dir = Read;
            bool done = false;
            WaitResult result = WaitResult::ready;
        };

        Wait begin_wait(int dir) {
            Wait w;
            w.dir = dir;
            w.generation = _generation.load(std::memory_order_seq_cst);
            auto dl = _deadline[dir].load(std::memory_order_seq_cst);
            if (dl != 0 && sgcl::clock::now() >= _point(dl)) {
                return _early(w, WaitResult::timed_out);
            }
            w.channel = dir == Read ? sgcl::async::readable(_fd) : sgcl::async::writable(_fd);
            {
                std::lock_guard lock(_m);
                _waits.push_back(w.channel);
            }
            if (closing()) {
                _forget(w.channel.get());
                return _early(w, WaitResult::closed);
            }
            if (_generation.load(std::memory_order_seq_cst) != w.generation) {
                _forget(w.channel.get());
                return _early(w, WaitResult::ready);   // a deadline changed meanwhile: look again
            }
            if (dl != 0) {
                w.timer = sgcl::async::detail::add_timer(_point(dl), tracked_ptr<void>(this), &Descriptor::_expire);
            }
            return w;
        }

        WaitResult end_wait(Wait& w, bool signalled) {
            _forget(w.channel.get());
            if (w.timer) {
                w.timer->cancelled.store(true, std::memory_order_release);
                sgcl::async::detail::timer_cancelled();
            }
            if (signalled) {
                return WaitResult::ready;
            }
            if (closing()) {
                return WaitResult::closed;
            }
            if (expired(w.dir)) {
                return WaitResult::timed_out;
            }
            if (_generation.load(std::memory_order_seq_cst) != w.generation) {
                return WaitResult::ready;
            }
            return WaitResult::cancelled;   // the reactor stopped (scheduler::stop)
        }

        // The whole wait on this thread
        WaitResult wait(int dir) {
            auto w = begin_wait(dir);
            return w.done ? w.result : end_wait(w, w.channel->receive().wait());
        }

    private:
        static constexpr uint64_t Closing = uint64_t(1) << 63;
        static constexpr uint64_t Closed = uint64_t(1) << 62;
        static constexpr uint64_t CountMask = Closed - 1;

        int _release() noexcept {
            uint64_t s = _state.fetch_sub(1, std::memory_order_seq_cst) - 1;
            if ((s & Closing) && (s & CountMask) == 0) {
                return _close_now();
            }
            return 0;
        }

        // Once: the registrations dropped from the reactor and the number
        // given back to the kernel, in that order (reactor.h: cancel)
        int _close_now() noexcept {
            if (_state.fetch_or(Closed, std::memory_order_acq_rel) & Closed) {
                return 0;
            }
            sgcl::async::cancel_waits(_fd);
            return !_owns || ::close(_fd) == 0 ? 0 : errno;
        }

        Wait _early(Wait& w, WaitResult r) noexcept {
            w.done = true;
            w.result = r;
            return w;
        }

        void _forget(const async::channel<void>* ch) {   // by identity
            std::lock_guard lock(_m);
            auto it = std::find_if(_waits.begin(), _waits.end(), [ch](const auto& w) { return w.get() == ch; });
            if (it != _waits.end()) {
                *it = _waits.back();
                _waits.pop_back();
            }
        }

        // The waits in progress ended with nothing: their channels closed
        // (the reactor's entry signals a closed channel later, which does
        // nothing)
        void _wake_waits() noexcept {
            std::lock_guard lock(_m);
            for (auto& ch : _waits) {
                ch->close();
            }
        }

        // The timer of a wait's deadline, on the timer's thread
        static void _expire(void* self) {
            auto d = static_cast<Descriptor*>(self);
            d->_generation.fetch_add(1, std::memory_order_seq_cst);
            d->_wake_waits();
        }

        static int64_t _rep(time_point t) noexcept {
            return std::chrono::nanoseconds(t.time_since_epoch()).count();
        }

        static time_point _point(int64_t rep) noexcept {
            return time_point() + std::chrono::nanoseconds(rep);
        }

        std::atomic<uint64_t> _state = {0};         // closing, closed, and the operations in progress
        std::atomic<uint64_t> _generation = {0};    // moved by a deadline changed or passed
        std::atomic<int64_t> _deadline[2] = {{0}, {0}};
        std::mutex _m;                              // the waits' channels
        vector<tracked_ptr<async::channel<void>>> _waits;
        const int _fd;
        const bool _owns;
    };

    // An operation's hold on the descriptor, for a scope: false (and no
    // hold) when the descriptor is closing
    class Operation {
    public:
        explicit Operation(Descriptor& d) noexcept
        : _d(&d)
        , _held(d.acquire()) {
        }

        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        ~Operation() {
            if (_held) {
                _d->release();
            }
        }

        explicit operator bool() const noexcept {
            return _held;
        }

    private:
        Descriptor* _d;
        bool _held;
    };
}
