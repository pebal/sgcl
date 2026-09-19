//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// The composition of tasks: WhenAll, every result back as a tuple or a
// List; WhenAny, the index of the first to finish.
#pragma once

#include "../../async/when.h"
#include "../Containers/List.h"
#include "Coroutine.h"

#include <tuple>
#include <utility>

namespace Sgcl {
    // The composition of tasks: `co_await WhenAll(a, b)` waits for every
    // task and gives their results as a tuple (a List for a List of
    // tasks of one type; nothing for tasks of nothing), what any threw
    // rethrown; `co_await WhenAny(a, b)` gives the index of the first to
    // finish and lets go of the rest. The tasks are taken over: spawned
    // ones, waited for here, not started here.
    template<class... T>
    Task<std::tuple<T...>> WhenAll(Task<T>... ts) {
        return sgcl::when_all(std::move(ts.Inner())...);   // the inner task as a Task: no frame of its own (Timeout.h)
    }

    template<class... Void>
    requires (std::is_void_v<Void> && ...)
    Task<> WhenAll(Task<Void>... ts) {
        return sgcl::when_all(std::move(ts.Inner())...);
    }

    template<class T>
    Task<List<T>> WhenAll(List<Task<T>> ts) {
        sgcl::vector<sgcl::task<T>> inner;
        inner.reserve(ts.Count());
        for (auto& t : ts) {
            inner.push_back(std::move(t.Inner()));
        }
        co_return List<T>(co_await sgcl::when_all(std::move(inner)));
    }

    inline Task<> WhenAll(List<Task<>> ts) {
        sgcl::vector<sgcl::task<>> inner;
        inner.reserve(ts.Count());
        for (auto& t : ts) {
            inner.push_back(std::move(t.Inner()));
        }
        return sgcl::when_all(std::move(inner));
    }

    template<class... T>
    Task<size_t> WhenAny(Task<T>... ts) {
        return sgcl::when_any(std::move(ts.Inner())...);
    }

    template<class T>
    Task<size_t> WhenAny(List<Task<T>> ts) {
        sgcl::vector<sgcl::task<T>> inner;
        inner.reserve(ts.Count());
        for (auto& t : ts) {
            inner.push_back(std::move(t.Inner()));
        }
        return sgcl::when_any(std::move(inner));
    }
}

using namespace Sgcl;   // the interface without a prefix: List, Ptr, Make...
