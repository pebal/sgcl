//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// Channel<T>: Go's channel. A capacity of n buffers
// n elements, 0 is a rendezvous; Receive on an empty channel and Send
// on a full one wait, Close ends the stream. Threads wait on the call;
// tasks `co_await ch.AsyncSend(v)` and `co_await ch.AsyncReceive()`.
// Channel<void> carries signals.
#pragma once

#include "../../async/channel.h"
#include "../../async/select.h"
#include "../Core/Types.h"

namespace Sgcl {
    template<class T>
    class Channel {
    public:
        using ValueType = T;
        using InnerType = sgcl::channel<T>;
        using SizeType = size_t;

        explicit Channel(SizeType capacity = 0)
        : _ch(capacity) {
        }

        Channel(const Channel&) = delete;
        Channel& operator=(const Channel&) = delete;

        // The element in, waiting for room or a receiver: false once the
        // channel is closed
        bool Send(const T& value) {
            return _ch.send(value);
        }

        bool Send(T&& value) {
            return _ch.send(std::move(value));
        }

        bool TrySend(const T& value) {
            return _ch.try_send(value);
        }

        bool TrySend(T&& value) {
            return _ch.try_send(std::move(value));
        }

        // The next element, waiting for one: nothing once the channel is
        // closed and drained
        Optional<T> Receive() {
            return _ch.receive();
        }

        Optional<T> TryReceive() {
            return _ch.try_receive();
        }

        // For a task: `co_await ch.AsyncSend(v)` (bool), `co_await
        // ch.AsyncReceive()` (optional<T>); the task holds no thread while
        // it waits
        auto AsyncSend(const T& value) {
            return _ch.async_send(value);
        }

        auto AsyncSend(T&& value) {
            return _ch.async_send(std::move(value));
        }

        auto AsyncReceive() noexcept {
            return _ch.async_receive();
        }

        // The cases of a Select: `ch.OnReceive(f)` is served when an
        // element comes and f gets it (f(T); or f(Optional<T>), also
        // called, with None, when the channel is closed); `ch.OnSend(v, f)`
        // when v is delivered, f after (a closed channel serves either at
        // once, without f)
        template<class F>
        auto OnReceive(F f) {
            return _ch.on_receive(std::move(f));
        }

        template<class F = void (*)()>
        auto OnSend(const T& value, F f = [] {}) {
            return _ch.on_send(value, std::move(f));
        }

        template<class F = void (*)()>
        auto OnSend(T&& value, F f = [] {}) {
            return _ch.on_send(std::move(value), std::move(f));
        }

        // No more sends: the receivers get what is buffered, then nothing
        void Close() {
            _ch.close();
        }

        bool IsClosed() const noexcept {
            return _ch.closed();
        }

        SizeType Capacity() const noexcept {
            return _ch.capacity();
        }

        SizeType Count() const noexcept {
            return _ch.size();
        }

        bool IsEmpty() const noexcept {
            return _ch.empty();
        }

        InnerType& Inner() noexcept {
            return _ch;
        }

        const InnerType& Inner() const noexcept {
            return _ch;
        }

    private:
        InnerType _ch;
    };

    template<>
    class Channel<void> {
    public:
        using ValueType = void;
        using InnerType = sgcl::channel<void>;
        using SizeType = size_t;

        explicit Channel(SizeType capacity = 0)
        : _ch(capacity) {
        }

        Channel(const Channel&) = delete;
        Channel& operator=(const Channel&) = delete;

        bool Send() {
            return _ch.send();
        }

        bool TrySend() {
            return _ch.try_send();
        }

        // A signal, waiting for one: false once the channel is closed
        bool Receive() {
            return _ch.receive();
        }

        bool TryReceive() {
            return _ch.try_receive();
        }

        auto AsyncSend() {
            return _ch.async_send();
        }

        auto AsyncReceive() noexcept {
            return _ch.async_receive();
        }

        // The cases of a Select: f() on a signal and on the close alike
        template<class F>
        auto OnReceive(F f) {
            return _ch.on_receive(std::move(f));
        }

        template<class F = void (*)()>
        auto OnSend(F f = [] {}) {
            return _ch.on_send(std::move(f));
        }

        void Close() {
            _ch.close();
        }

        bool IsClosed() const noexcept {
            return _ch.closed();
        }

        SizeType Capacity() const noexcept {
            return _ch.capacity();
        }

        SizeType Count() const noexcept {
            return _ch.size();
        }

        bool IsEmpty() const noexcept {
            return _ch.empty();
        }

        InnerType& Inner() noexcept {
            return _ch;
        }

        const InnerType& Inner() const noexcept {
            return _ch;
        }

    private:
        InnerType _ch;
    };

    // `for (auto v : ch)`: every element until the channel is closed and
    // drained, each a Receive
    template<class T>
    auto begin(Channel<T>& ch) {
        return ch.Inner().begin();
    }

    template<class T>
    auto end(Channel<T>& ch) noexcept {
        return ch.Inner().end();
    }

    // Select: a wait on several channels at once, one case per channel
    // (OnReceive, OnSend) and at most one Otherwise, taken when no other
    // can be served at once; the case served runs its body and its index
    // is returned. `Select(...)` blocks the thread; `co_await
    // AsyncSelect(...)` suspends the task. Among cases ready at once one
    // is chosen at random.
    template<class F>
    auto Otherwise(F f) {
        return sgcl::otherwise(std::move(f));
    }

    template<class... Cases>
    size_t Select(Cases... cases) {
        return sgcl::select(std::move(cases)...);
    }

    template<class... Cases>
    auto AsyncSelect(Cases... cases) {
        return sgcl::async_select(std::move(cases)...);
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...

