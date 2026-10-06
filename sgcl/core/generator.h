//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "coroutine.h"

#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <optional>
#include <utility>

namespace sgcl {
    // A coroutine that co_yields values, consumed with a range-for or
    // next()/value(); an exception it throws comes out of next() (or the
    // iterator's first look at an element). Its frame is managed
    // (coroutine.h: managed_frame), so its locals and parameters are roots
    // while it is suspended; it runs where next() is called, on no
    // scheduler, and leaves the frame's header as it was allocated, zero.
    template<class T>
    class generator {
    public:
        struct promise_type : managed_frame {
            optional<T> value;
            std::exception_ptr error;

            SGCL_INLINE_HOT generator get_return_object() noexcept {
                return generator(std::coroutine_handle<promise_type>::from_promise(*this));
            }

            SGCL_INLINE_HOT std::suspend_always initial_suspend() noexcept {
                return {};
            }

            SGCL_INLINE_HOT std::suspend_always final_suspend() noexcept {
                return {};
            }

            SGCL_INLINE_HOT std::suspend_always yield_value(T v) noexcept(std::is_nothrow_move_constructible_v<T>) {
                value.emplace(std::move(v));
                return {};
            }

            SGCL_INLINE_HOT void return_void() noexcept {
            }

            SGCL_INLINE_HOT void unhandled_exception() noexcept {
                error = std::current_exception();
            }
        };

        // Lazy, as async::channel's: the coroutine runs to its next co_yield
        // at the first look at an element (*it, or it == end), and ++ only
        // marks it used, so an iterator left after its ++ (views::take) has
        // run the coroutine no further and the next begin() goes on from the
        // element after the last one looked at; the end is
        // std::default_sentinel
        class iterator {
        public:
            using iterator_concept = std::input_iterator_tag;
            using iterator_category = std::input_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            iterator() noexcept = default;

            SGCL_INLINE_HOT reference operator*() const {
                _fill();
                return _g->value();
            }

            SGCL_INLINE_HOT pointer operator->() const {
                _fill();
                return &_g->value();
            }

            SGCL_INLINE_HOT iterator& operator++() noexcept {
                _held = false;
                return *this;
            }

            SGCL_INLINE_HOT void operator++(int) noexcept {
                ++*this;
            }

            // At the end once the coroutine has returned: the look runs it
            // to its next co_yield when the iterator holds no element
            SGCL_INLINE_HOT friend bool operator==(const iterator& it, std::default_sentinel_t) {
                it._fill();
                return !it._g;
            }

        private:
            SGCL_INLINE_HOT explicit iterator(generator* g) noexcept
            : _g(g) {
            }

            SGCL_INLINE_HOT void _fill() const {
                if (!_held && _g) {
                    if (_g->next()) {
                        _held = true;
                    } else {
                        _g = nullptr;
                    }
                }
            }

            mutable generator* _g = nullptr;
            mutable bool _held = false;

            friend class generator;
        };

        generator() noexcept = default;

        // Runs to the next co_yield: true, or to the end: false
        SGCL_INLINE_HOT bool next() {
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

        SGCL_INLINE_HOT T& value() const noexcept {
            return *_frame.promise().value;
        }

        SGCL_INLINE_HOT iterator begin() noexcept {
            return iterator(this);
        }

        SGCL_INLINE_HOT std::default_sentinel_t end() const noexcept {
            return std::default_sentinel;
        }

        SGCL_INLINE_HOT void destroy() noexcept {
            _frame.destroy();
        }

    private:
        SGCL_INLINE_HOT explicit generator(std::coroutine_handle<promise_type> h) noexcept
        : _frame(h) {
        }

        frame_ptr<promise_type> _frame;
    };
}
