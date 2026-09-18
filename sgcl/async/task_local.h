//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/aliases.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "coroutine.h"

#include <coroutine>
#include <utility>

namespace sgcl {
    namespace detail {
        // The values of a task's task_locals: a chain of nodes, one per
        // set, the newest first, whose head the frame's header holds
        // (coroutine.h: FrameHeader). A node is never changed once made:
        // a set puts a new node in front of the task's chain, and a task
        // started by this one takes the head as it is at the start, so
        // the parent's later sets are its own, a child's sets are the
        // child's (a node of its own in front of the shared tail), and
        // siblings share nothing but the tail they inherited. The key is
        // the task_local's address, one per declared variable.
        struct TaskLocals {
            tracked_ptr<TaskLocals> next;
            const void* key = nullptr;
        };

        template<class T>
        struct TaskLocalValue : TaskLocals {
            explicit TaskLocalValue(T v)
            : value(std::move(v)) {
            }

            T value;
        };

        // The node of a key in the chain of the frame this thread runs,
        // null outside a task or when the task never set it
        inline const TaskLocals* find_task_local(const void* key) noexcept {
            auto frame = current_frame;
            for (auto n = frame ? frame_header(frame).locals.get() : nullptr; n; n = n->next.get()) {
                if (n->key == key) {
                    return n;
                }
            }
            return nullptr;
        }
    }

    // A value visible to a task and to the tasks it starts, read without
    // passing it through every signature: a request id, a deadline, a
    // stop token, a logger, the current user (Go's context.WithValue,
    // Kotlin's CoroutineContext, tokio's task_local!). Declared once, at
    // namespace scope (`sgcl::task_local<int> request_id;`): the variable
    // is the key, the values live in managed nodes the tasks' frames
    // point to (detail::TaskLocals), so the variable holds nothing and
    // lives anywhere. `co_await request_id.set(7)` sets the value for the
    // task from that line on, and for every task it starts from then on
    // (spawn, go, co_await of a task not yet started, an executor's
    // spawn); request_id.get() reads it, from the coroutine's body or
    // from any function it calls (the worker knows which frame it runs:
    // coroutine.h: current_frame), nullopt when unset or outside a task;
    // request_id.with(7, t) is a task that runs t with the value set. A
    // child's set is the child's: the parent and the siblings keep what
    // they had (copy on write, by a node in front of the shared chain).
    // The value is copied out by get(): a tracked_ptr, a string, a token,
    // an int, whatever is cheap to copy and safe to share between the
    // tasks that inherit it; the nodes are never written after they are
    // made, so the tasks that share one read it freely.
    template<class T>
    class task_local {
    public:
        task_local() noexcept = default;
        task_local(const task_local&) = delete;
        task_local& operator=(const task_local&) = delete;

        // `co_await x.set(v)`: the value for this task from here on, and
        // for the tasks it starts from here on. An awaitable that never
        // suspends: the co_await is what names the coroutine whose value
        // it is (a function the task calls reads, the task sets)
        class setter {
        public:
            bool await_ready() const noexcept {
                return false;
            }

            template<class P>
            bool await_suspend(std::coroutine_handle<P> h) {
                auto& header = detail::frame_header(detail::frame_of(h).get());
                tracked_ptr<detail::TaskLocalValue<T>> node = make_tracked<detail::TaskLocalValue<T>>(std::move(_value));
                node->key = _key;
                node->next = header.locals;
                header.locals = node;
                return false;   // set: the coroutine goes on at once
            }

            void await_resume() const noexcept {
            }

        private:
            friend class task_local;

            setter(const void* key, T value)
            : _key(key)
            , _value(std::move(value)) {
            }

            const void* _key;
            T _value;
        };

        setter set(T value) {
            return setter(this, std::move(value));
        }

        // The value of the task this thread runs: nullopt when the task
        // never set it and inherited none, and outside a task
        optional<T> get() const {
            if (auto n = detail::find_task_local(this)) {
                return static_cast<const detail::TaskLocalValue<T>*>(n)->value;
            }
            return nullopt;
        }

        // The value, or the one given when there is none
        T get_or(T fallback) const {
            if (auto n = detail::find_task_local(this)) {
                return static_cast<const detail::TaskLocalValue<T>*>(n)->value;
            }
            return fallback;
        }

        bool is_set() const noexcept {
            return detail::find_task_local(this) != nullptr;
        }

        // A task that runs t with the value set: `co_await x.with(v, f())`,
        // `spawn(x.with(v, f()))`. The value is this task's, t inherits it
        // at its start, whatever set it or its children do is theirs
        template<class U>
        task<U> with(T value, task<U> t) {
            co_await set(std::move(value));
            co_return co_await t;
        }
    };
}
