//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Broadcast<T>: a channel every subscriber receives every value from
// (tokio's broadcast, Kotlin's SharedFlow; an event bus). Subscribe()
// gives a Subscription with a cursor of its own into one ring shared by
// all; Send(v) goes to every subscription alive and never waits; a
// subscription that falls more than the capacity behind loses the oldest
// values and Lagged() says how many; Close() ends every subscription. A
// thread waits on Receive(), a task on `co_await s.AsyncReceive()`, a
// Select on s.OnReceive(f).
#pragma once

#include "../../async/broadcast.h"
#include "../Core/Types.h"

#include <utility>

namespace Sgcl {
    template<class T>
    class Broadcast {
    public:
        using ValueType = T;
        using InnerType = sgcl::broadcast<T>;
        using SizeType = size_t;

        // A receiver with a cursor of its own, from the value sent next
        // after its making; counts itself off the values it has not
        // passed when destroyed
        class Subscription {
        public:
            using InnerType = typename sgcl::broadcast<T>::subscription;

            Subscription() noexcept = default;
            Subscription(Subscription&&) noexcept = default;
            Subscription& operator=(Subscription&&) noexcept = default;
            Subscription(const Subscription&) = delete;
            Subscription& operator=(const Subscription&) = delete;

            Subscription(InnerType&& s) noexcept
            : _s(std::move(s)) {
            }

            // The next value, waiting for one: None once the broadcast is
            // closed and drained
            Optional<T> Receive() {
                return _s.receive();
            }

            // The next value if one is there
            Optional<T> TryReceive() {
                return _s.try_receive();
            }

            // `co_await s.AsyncReceive()`: the task holds no thread while it waits
            auto AsyncReceive() noexcept {
                return _s.async_receive();
            }

            // A case of a Select: f gets the value (f(T); or
            // f(Optional<T>), also called, with None, when the broadcast
            // is closed and drained)
            template<class F>
            auto OnReceive(F f) {
                return _s.on_receive(std::move(f));
            }

            // The values lost before the last one received
            SizeType Lagged() const noexcept {
                return _s.lagged();
            }

            bool IsClosed() const noexcept {
                return _s.closed();
            }

            explicit operator bool() const noexcept {
                return (bool)_s;
            }

            InnerType& Inner() noexcept {
                return _s;
            }

        private:
            InnerType _s;
        };

        // A ring of `capacity` values, rounded up to a power of two
        explicit Broadcast(SizeType capacity)
        : _b(capacity) {
        }

        Broadcast(const Broadcast&) = delete;
        Broadcast& operator=(const Broadcast&) = delete;

        Subscription Subscribe() {
            return Subscription(_b.subscribe());
        }

        // The value to every subscription alive, without waiting: false
        // once the broadcast is closed
        bool Send(const T& value) {
            return _b.send(value);
        }

        bool Send(T&& value) {
            return _b.send(std::move(value));
        }

        // No more sends: every subscription receives what was sent, then None
        void Close() {
            _b.close();
        }

        bool IsClosed() const noexcept {
            return _b.closed();
        }

        SizeType Capacity() const noexcept {
            return _b.capacity();
        }

        // The subscriptions alive
        SizeType SubscriberCount() const noexcept {
            return _b.subscribers();
        }

        InnerType& Inner() noexcept {
            return _b;
        }

    private:
        InnerType _b;
    };
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
