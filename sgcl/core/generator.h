//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
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
