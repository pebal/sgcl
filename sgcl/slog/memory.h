//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "handler.h"
#include "record.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "../core/vector.h"

#include <mutex>

namespace sgcl::slog {
    namespace detail {
        struct MemoryState {
            std::mutex lock;
            vector<record> records;
        };
    }

    // A handler that keeps the records, for tests: slog::logger(mem), then
    // mem.records(). Each record kept is a clone() (it owns its texts and
    // its attributes). A handle of one word, made empty by its
    // constructor; the copies share the records; any thread may log
    // through it.
    class memory {
    public:
        SGCL_INLINE_HOT memory() noexcept
        : _s(make_tracked<detail::MemoryState>()) {
        }

        // The records so far, in the order they came
        vector<record> records() const noexcept {
            std::lock_guard<std::mutex> g(_s->lock);
            vector<record> out;
            out.reserve(_s->records.size());
            for (const record& r : _s->records) {
                out.push_back(r);
            }
            return out;
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            std::lock_guard<std::mutex> g(_s->lock);
            return _s->records.size();
        }

        SGCL_INLINE_HOT void clear() const noexcept {
            std::lock_guard<std::mutex> g(_s->lock);
            _s->records.clear();
        }

        SGCL_INLINE_HOT void handle(const record& r) const {
            record copy = r.clone();
            std::lock_guard<std::mutex> g(_s->lock);
            _s->records.push_back(copy);
        }

        SGCL_INLINE_HOT bool enabled(slog::level) const noexcept {
            return true;
        }

    private:
        tracked_ptr<detail::MemoryState> _s;
    };

    namespace detail {
        template<>
        inline constexpr bool IsHandlerHandle<memory> = true;
    }
}
