//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "coroutine.h"

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace sgcl {
    // A generator that may wait: a coroutine that co_yields values and
    // co_awaits between them (a channel, a sleep, a task), consumed from a
    // task with `while (auto v = co_await g.next())`. The consumer and the
    // generator hand control to each other directly: next() resumes the
    // generator on the consumer's worker, a co_yield resumes the consumer
    // where the generator is, and while the generator waits for something
    // the consumer waits with it, no thread held by either. Both frames
    // are on the managed heap: the generator's held by the async_generator
    // object, the consumer's by the generator's promise while it waits.
    template<class T>
    class async_generator {
    public:
        struct promise_type : managed_frame {
            std::optional<T> value;
            std::exception_ptr error;
            std::coroutine_handle<> consumer;               // the coroutine waiting in next()
            tracked_ptr<detail::FrameWord> consumer_frame;   // its frame, held for the length of the wait

            async_generator get_return_object() {
                return async_generator(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            std::suspend_always initial_suspend() noexcept {
                return {};
            }

            // At a yield and at the end: back to the consumer, directly
            struct to_consumer {
                bool await_ready() noexcept {
                    return false;
                }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto& p = h.promise();
                    auto c = p.consumer;
                    p.consumer = {};
                    // The consumer's frame stays held here through its run:
                    // when the generator was resumed by the scheduler (after a
                    // wait of its own) the worker's stack holds the generator's
                    // buffer alone, and the consumer, handed the thread by the
                    // transfer, would run with its frame held by nothing (a
                    // handle is a pointer into the buffer, which keeps nothing:
                    // README, rule 4); the next next() replaces the word. The
                    // consumer runs as the current frame: its own locals
                    if (c) {
                        detail::current_frame = p.consumer_frame.get();
                    }
                    return c ? c : std::noop_coroutine();
                }

                void await_resume() noexcept {
                }
            };

            to_consumer yield_value(T v) {
                value.emplace(std::move(v));
                return {};
            }

            to_consumer final_suspend() noexcept {
                return {};
            }

            void return_void() noexcept {
            }

            void unhandled_exception() noexcept {
                error = std::current_exception();
            }
        };

        async_generator() noexcept = default;
        async_generator(async_generator&&) noexcept = default;
        async_generator& operator=(async_generator&&) noexcept = default;

        // `co_await g.next()`: the next value, or nothing at the end;
        // rethrows what the generator threw
        class next_op {
        public:
            bool await_ready() const noexcept {
                return !_g._frame || _g._frame.done();
            }

            // The generator's frame takes the consumer's header (its
            // executor, its task-locals) at every next(): a wait of the
            // generator's own is enqueued and resumed by that frame, so
            // it goes where the consumer runs and the functions under it
            // (and the consumer, resumed by the yield on that thread) see
            // the consumer's locals; copied every time, since the consumer
            // may have moved or set a local since the last next(). The
            // generator is suspended and held by the consumer alone here,
            // so its header is the consumer's to write
            template<class P>
            std::coroutine_handle<> await_suspend(std::coroutine_handle<P> h) noexcept {
                auto& p = _g._frame.promise();
                p.value.reset();
                p.consumer = h;
                p.consumer_frame = detail::frame_of(h);
                auto& theirs = detail::frame_header(p.consumer_frame.get());
                auto& mine = detail::frame_header(p.self.get());
                mine.executor = theirs.executor;
                mine.locals = theirs.locals;
                return _g._frame.handle();   // the generator runs, here, until its next yield or wait
            }

            std::optional<T> await_resume() {
                if (!_g._frame) {
                    return std::nullopt;
                }
                auto& p = _g._frame.promise();
                if (p.error) {
                    std::rethrow_exception(std::exchange(p.error, nullptr));
                }
                if (_g._frame.done()) {
                    return std::nullopt;
                }
                return std::move(p.value);
            }

        private:
            friend class async_generator;

            explicit next_op(async_generator& g) noexcept
            : _g(g) {
            }

            async_generator& _g;
        };

        next_op next() noexcept {
            return next_op(*this);
        }

        bool done() const noexcept {
            return !_frame || _frame.done();
        }

    private:
        explicit async_generator(std::coroutine_handle<promise_type> h)
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };
}
