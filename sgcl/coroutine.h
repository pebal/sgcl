//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/collector.h"
#include "detail/frame_word.h"
#include "detail/maker.h"
#include "tracked_ptr.h"
#include "unique_ptr.h"

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace sgcl {
    // The frame of a coroutine is allocated with operator new and is no
    // place for a tracked_ptr (README, "The rules"): a promise type that
    // derives from managed_frame gets its frames from the managed heap
    // instead, as buffers of words traced conservatively (detail/frame_word.h),
    // so the tracked_ptr parameters, locals and promise members of the
    // coroutine are roots for as long as the frame is held. A frame is
    // held through a frame_ptr (below), made from the coroutine handle in
    // get_return_object; it leaves operator new in the state of an object
    // a unique_ptr owns (a root), which the frame_ptr takes over. operator
    // delete does nothing for a frame taken over: destroying the coroutine
    // (frame_ptr::destroy, the handle's destroy) runs the destructors of
    // its locals and promise, and the memory is the collector's once
    // nothing holds it; a frame nothing took over (an exception before
    // get_return_object) is freed at once.
    struct managed_frame {
        static void* operator new(size_t size) {
            auto words = (size + sizeof(detail::FrameWord) - 1) / sizeof(detail::FrameWord);
            return detail::Maker<detail::FrameWord[]>::make_tracked_data(words).release();
        }

        static void operator delete(void* p, size_t) noexcept {
            if (detail::Page::is_unique(p)) {
                detail::Collector::delete_unique(p);
            }
        }
    };

    // The owner of a coroutine whose promise derives from managed_frame:
    // a tracked_ptr to the frame and the coroutine handle. Move-only;
    // destroys the coroutine when destroyed, which runs the destructors of
    // its locals and promise. Lives where a tracked_ptr may: in a managed
    // object, on a stack, or in another managed frame.
    template<class Promise>
    class frame_ptr {
    public:
        using promise_type = Promise;
        using handle_type = std::coroutine_handle<Promise>;

        frame_ptr() noexcept = default;

        explicit frame_ptr(handle_type h)
        : _frame(_take(h.address()))
        , _handle(h) {
        }

        frame_ptr(frame_ptr&& o) noexcept
        : _frame(o._frame)
        , _handle(std::exchange(o._handle, {})) {
            o._frame = nullptr;
        }

        frame_ptr& operator=(frame_ptr&& o) noexcept {
            if (this != &o) {
                destroy();
                _frame = o._frame;
                o._frame = nullptr;
                _handle = std::exchange(o._handle, {});
            }
            return *this;
        }

        frame_ptr(const frame_ptr&) = delete;
        frame_ptr& operator=(const frame_ptr&) = delete;

        ~frame_ptr() {
            destroy();
        }

        explicit operator bool() const noexcept {
            return (bool)_handle;
        }

        handle_type handle() const noexcept {
            return _handle;
        }

        Promise& promise() const {
            return _handle.promise();
        }

        void resume() {
            _handle.resume();
        }

        bool done() const noexcept {
            return !_handle || _handle.done();
        }

        // Runs the destructors of the coroutine's locals and promise and
        // lets go of the frame
        void destroy() noexcept {
            if (_handle) {
                _handle.destroy();
                _handle = {};
            }
            _frame = nullptr;
        }

    private:
        // The frame, from the state operator new left it in (owned by a
        // unique_ptr) to a tracked one: the same path a container's buffer
        // takes (vector.h: _allocate)
        static tracked_ptr<detail::FrameWord> _take(void* address) {
            return unique_ptr<detail::FrameWord>(detail::UniquePtr<detail::FrameWord>((detail::FrameWord*)address));
        }

        tracked_ptr<detail::FrameWord> _frame;
        handle_type _handle;
    };

    // A lazy coroutine that produces a value: resume() runs it to its next
    // co_await std::suspend_always or to its end; result() is the value it
    // co_returned, or rethrows what it threw. A building block with the
    // managed frame; a scheduler brings its own promise types, derived
    // from managed_frame the same way.
    template<class T = void>
    class task {
    public:
        struct promise_type : managed_frame {
            std::optional<T> value;
            std::exception_ptr error;

            task get_return_object() {
                return task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            std::suspend_always final_suspend() noexcept {
                return {};
            }

            void return_value(T v) {
                value.emplace(std::move(v));
            }

            void unhandled_exception() noexcept {
                error = std::current_exception();
            }
        };

        task() noexcept = default;

        void resume() {
            _frame.resume();
        }

        bool done() const noexcept {
            return _frame.done();
        }

        T& result() {
            auto& p = _frame.promise();
            if (p.error) {
                std::rethrow_exception(p.error);
            }
            return *p.value;
        }

        void destroy() noexcept {
            _frame.destroy();
        }

    private:
        explicit task(std::coroutine_handle<promise_type> h)
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };

    template<>
    class task<void> {
    public:
        struct promise_type : managed_frame {
            std::exception_ptr error;

            task get_return_object() {
                return task(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            std::suspend_always final_suspend() noexcept {
                return {};
            }

            void return_void() noexcept {
            }

            void unhandled_exception() noexcept {
                error = std::current_exception();
            }
        };

        task() noexcept = default;

        void resume() {
            _frame.resume();
        }

        bool done() const noexcept {
            return _frame.done();
        }

        // Rethrows what the coroutine threw, if anything
        void result() {
            if (auto& p = _frame.promise(); p.error) {
                std::rethrow_exception(p.error);
            }
        }

        void destroy() noexcept {
            _frame.destroy();
        }

    private:
        explicit task(std::coroutine_handle<promise_type> h)
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };

    // A coroutine that co_yields values, consumed with a range-for or
    // next()/value(); an exception it throws comes out of next() (or the
    // iterator's ++).
    template<class T>
    class generator {
    public:
        struct promise_type : managed_frame {
            std::optional<T> value;
            std::exception_ptr error;

            generator get_return_object() {
                return generator(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            std::suspend_always final_suspend() noexcept {
                return {};
            }

            std::suspend_always yield_value(T v) {
                value.emplace(std::move(v));
                return {};
            }

            void return_void() noexcept {
            }

            void unhandled_exception() noexcept {
                error = std::current_exception();
            }
        };

        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = const T*;
            using reference = const T&;

            iterator() noexcept = default;

            reference operator*() const noexcept {
                return _g->value();
            }

            pointer operator->() const noexcept {
                return &_g->value();
            }

            iterator& operator++() {
                if (!_g->next()) {
                    _g = nullptr;
                }
                return *this;
            }

            void operator++(int) {
                ++*this;
            }

            bool operator==(const iterator& o) const noexcept {
                return _g == o._g;
            }

            bool operator!=(const iterator& o) const noexcept {
                return _g != o._g;
            }

        private:
            explicit iterator(generator* g) noexcept
            : _g(g) {
            }

            generator* _g = nullptr;

            friend class generator;
        };

        generator() noexcept = default;

        // Runs to the next co_yield: true, or to the end: false
        bool next() {
            if (_frame.done()) {
                return false;
            }
            _frame.resume();
            auto& p = _frame.promise();
            if (p.error) {
                std::rethrow_exception(p.error);
            }
            return !_frame.done();
        }

        const T& value() const noexcept {
            return *_frame.promise().value;
        }

        iterator begin() {
            return next() ? iterator(this) : iterator();
        }

        iterator end() noexcept {
            return iterator();
        }

        void destroy() noexcept {
            _frame.destroy();
        }

    private:
        explicit generator(std::coroutine_handle<promise_type> h)
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };
}
