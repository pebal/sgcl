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
    // iterator's ++). Its frame is managed (coroutine.h: managed_frame),
    // so its locals and parameters are roots while it is suspended; it
    // runs where next() is called, on no scheduler, and leaves the frame's
    // header as it was allocated, zero.
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

        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            iterator() noexcept = default;

            SGCL_INLINE_HOT reference operator*() const noexcept {
                return _g->value();
            }

            SGCL_INLINE_HOT pointer operator->() const noexcept {
                return &_g->value();
            }

            SGCL_INLINE_HOT iterator& operator++() {
                if (!_g->next()) {
                    _g = nullptr;
                }
                return *this;
            }

            SGCL_INLINE_HOT void operator++(int) {
                ++*this;
            }

            SGCL_INLINE_HOT bool operator==(const iterator& o) const noexcept {
                return _g == o._g;
            }

            SGCL_INLINE_HOT bool operator!=(const iterator& o) const noexcept {
                return _g != o._g;
            }

        private:
            SGCL_INLINE_HOT explicit iterator(generator* g) noexcept
            : _g(g) {
            }

            generator* _g = nullptr;

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

        SGCL_INLINE_HOT iterator begin() {
            return next() ? iterator(this) : iterator();
        }

        SGCL_INLINE_HOT iterator end() noexcept {
            return iterator();
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
