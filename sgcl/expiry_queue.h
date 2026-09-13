//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "vector.h"
#include "weak_ptr.h"

#include <functional>

namespace sgcl {
    // What to do with an object once nothing else reaches it, decided by
    // an observer rather than by the object's destructor: watch(object, f)
    // hands out a weak_ptr to the object and keeps f next to the weak
    // pointer's cell. When a cycle finds the object unreachable it does not
    // destroy it: it keeps it alive for the queue (detail/weak_cell.h:
    // Expired), and drain() calls f with the object, alive one last time,
    // as a tracked_ptr; f may use it, or keep it, which is the object's
    // return to life. Then the entry is dropped and the object dies with
    // the next cycle that finds it unreachable, its destructor as ever.
    // f runs on the thread that calls drain(), at that moment, with the
    // heap in a consistent state: no collector thread, none of the rules
    // of destructors. What f captures follows rule 1 (a std::function
    // keeps its closure on the unmanaged heap: no tracked_ptr in it); the
    // object comes as the argument. The queue drains by itself every so
    // many watch() calls, as many as it has entries (a pass costs less
    // than the calls that paid for it); a thread that watches little and
    // wants its cleanups on time calls drain() in its loop, and an object
    // found unreachable waits for that call. Lives where a tracked_ptr
    // may; shared between threads with the program's own synchronization.
    template<class T>
    class expiry_queue {
    public:
        using value_type = tracked_ptr<T>;
        using function_type = std::function<void(tracked_ptr<T>)>;
        using size_type = size_t;

        expiry_queue() = default;

        // A weak_ptr to the object, with f kept for the day the object is
        // found unreachable; a null object gets no entry
        template<class F>
        weak_ptr<T> watch(const tracked_ptr<T>& object, F&& on_expire) {
            if (!object) {
                return {};
            }
            weak_ptr<T> w(weak_ptr<T>::_make_cell(object.get(), detail::WeakCell::Watched));
            _entries.push_back(Entry{w._cell, function_type(std::forward<F>(on_expire))});
            if (++_watched > _threshold) {
                drain();
            }
            return w;
        }

        // Calls f with the object of every entry whose object a cycle has
        // found unreachable, and drops the entry. Returns how many.
        size_type drain() {
            size_type count = 0;
            for (size_type i = 0; i < _entries.size();) {
                auto cell = _entries[i].cell.get();
                if (cell->flags.load(std::memory_order_acquire) & detail::WeakCell::Expired) {
                    tracked_ptr<T> object = weak_ptr<T>(_entries[i].cell).lock();
                    auto on_expire = std::move(_entries[i].on_expire);
                    _release(cell);
                    _entries[i] = std::move(_entries.back());
                    _entries.pop_back();
                    ++count;
                    on_expire(std::move(object));
                } else {
                    ++i;
                }
            }
            _watched = 0;
            _threshold = std::max<size_type>(16, _entries.size());
            return count;
        }

        // The entries not drained yet, expired or not
        size_type size() const noexcept {
            return _entries.size();
        }

        bool empty() const noexcept {
            return _entries.empty();
        }

        // Drops every entry without calling its function: the objects are
        // no longer kept
        void clear() noexcept {
            for (auto& entry : _entries) {
                _release(entry.cell.get());
            }
            _entries.clear();
            _watched = 0;
            _threshold = 16;
        }

        ~expiry_queue() {
            clear();
        }

        expiry_queue(expiry_queue&&) noexcept = default;
        expiry_queue& operator=(expiry_queue&&) noexcept = default;
        expiry_queue(const expiry_queue&) = delete;
        expiry_queue& operator=(const expiry_queue&) = delete;

    private:
        struct Entry {
            tracked_ptr<detail::WeakCell> cell;
            function_type on_expire;
        };

        // The cell is an ordinary weak cell from now on: its target is no
        // longer kept for the queue
        static void _release(detail::WeakCell* cell) noexcept {
            cell->flags.fetch_or(detail::WeakCell::Drained, std::memory_order_acq_rel);
        }

        vector<Entry> _entries;
        size_type _watched = 0;
        size_type _threshold = 16;
    };
}
