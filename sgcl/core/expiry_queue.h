//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "function.h"
#include "vector.h"
#include "weak_ptr.h"

namespace sgcl {
    // What to do with an object once nothing else reaches it, decided by
    // an observer rather than by the object's destructor: watch(object, f)
    // makes a weak cell for the object and keeps f next to it. When a cycle finds the object unreachable it does not
    // destroy it: it keeps it alive for the queue (detail/weak_cell.h:
    // Expired), and drain() calls f with the object, alive one last time,
    // as a tracked_ptr; f may use it, or keep it, which is the object's
    // return to life. Then the entry is dropped and the object dies with
    // the next cycle that finds it unreachable, its destructor as ever.
    // f runs on the thread that calls drain(), at that moment, with the
    // heap in a consistent state: no collector thread, none of the rules
    // of destructors. f is a function (function.h): its closure may
    // capture tracked pointers, which the collector follows; a closure
    // holding a strong pointer to the watched object itself keeps the
    // object alive and the entry never expires (the object comes as the
    // argument instead). The queue drains by itself every so
    // many watch() calls, as many as it has entries (a pass costs less
    // than the calls that paid for it); a thread that watches little and
    // wants its cleanups on time calls drain() in its loop, and an object
    // found unreachable waits for that call. watch() returns an entry
    // handle: cancel() withdraws the entry (the object no longer kept,
    // f never called: a resource released by hand), weak() is a weak_ptr
    // to the object sharing the entry's cell. The queue holds its entries
    // and the handle its cell by tracked_ptrs, what watch() takes and
    // what f receives, so they live where one may: on a stack or in a
    // managed object. Shared between threads with the program's own
    // synchronization; cancel() alone is
    // an atomic flag and may come from any thread.
    template<class T>
    class expiry_queue {
    public:
        using value_type = tracked_ptr<T>;
        using weak_type = weak_ptr<T>;
        using function_type = function<void(value_type)>;
        using size_type = size_t;

        // The handle of one entry: the entry's cell, which watch() made.
        // Empty for a null object. Copies share the entry.
        class entry {
        public:
            entry() noexcept = default;

            // Withdraws the entry: the object is no longer kept for the
            // queue and its function will not be called; the entry leaves
            // the queue with the next drain(). True when the entry was
            // still pending (not drained, not cancelled before).
            bool cancel() noexcept {
                auto cell = _cell.get();
                if (!cell) {
                    return false;
                }
                auto old = cell->flags.fetch_or(detail::WeakCell::Drained, std::memory_order_acq_rel);
                return !(old & detail::WeakCell::Drained);
            }

            // The collector has found the object unreachable: its function
            // waits for drain() (or was called, or the entry was cancelled)
            bool expired() const noexcept {
                auto cell = _cell.get();
                return cell && (cell->flags.load(std::memory_order_acquire) & detail::WeakCell::Expired);
            }

            // A weak pointer to the object, sharing the entry's cell: an
            // ordinary weak_ptr (lock(), expired()), holding nothing
            weak_type weak() const noexcept {
                return weak_type(_cell);
            }

            explicit operator bool() const noexcept {
                return _cell.get() != nullptr;
            }

        private:
            explicit entry(const tracked_ptr<detail::WeakCell>& cell) noexcept
            : _cell(cell) {
            }

            tracked_ptr<detail::WeakCell> _cell;

            friend class expiry_queue;
        };

        expiry_queue() = default;

        // An entry for the object, with f kept for the day the object is
        // found unreachable; a null object gets no entry (an empty handle).
        // The function is made before the cell: it may throw, and a
        // Watched cell with no entry behind it would keep its target one
        // cycle longer than the cell itself lives.
        template<class F>
        entry watch(const value_type& object, F&& on_expire) {
            if (!object) {
                return {};
            }
            function_type f(std::forward<F>(on_expire));
            weak_type w(weak_type::_make_cell(object.get(), detail::WeakCell::Watched));
            _entries.push_back(Entry{w._cell, std::move(f)});
            if (++_watched > _threshold) {
                drain();
            }
            return entry(w._cell);
        }

        // Calls f with the object of every entry whose object a cycle has
        // found unreachable, and drops the entry; drops the cancelled
        // entries without a call. Returns how many functions were called.
        // The Drained flag is what decides: a cancel() from another thread
        // between the load of the flags and the release of the cell has
        // set it first and returned true, so its entry is dropped without
        // the call, as one cancelled before the load is.
        size_type drain() {
            size_type count = 0;
            for (size_type i = 0; i < _entries.size();) {
                auto cell = _entries[i].cell.get();
                auto flags = cell->flags.load(std::memory_order_acquire);
                if (flags & detail::WeakCell::Drained) {   // cancelled: no call
                    _entries[i] = std::move(_entries.back());
                    _entries.pop_back();
                } else if (flags & detail::WeakCell::Expired) {
                    value_type object = weak_type(_entries[i].cell).lock();
                    auto on_expire = std::move(_entries[i].on_expire);
                    bool pending = !(_release(cell) & detail::WeakCell::Drained);
                    _entries[i] = std::move(_entries.back());
                    _entries.pop_back();
                    if (pending) {
                        ++count;
                        on_expire(std::move(object));
                    }
                } else {
                    ++i;
                }
            }
            _watched = 0;
            _threshold = std::max<size_type>(16, _entries.size());
            return count;
        }

        // The entries not drained yet, expired or not (a cancelled one
        // counts until the next drain)
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

        // A move takes the entries over with the count of the automatic
        // drain; the source is left empty, as after clear()
        expiry_queue(expiry_queue&& other) noexcept
        : _entries(std::move(other._entries))
        , _watched(other._watched)
        , _threshold(other._threshold) {
            other._watched = 0;
            other._threshold = 16;
        }

        // The entries this queue held are dropped as clear() drops them,
        // without a call: released, their objects are no longer kept
        // (defaulted, the assignment let the old cells go still Watched,
        // and every handle or weak pointer of theirs kept its object for
        // as long as it lived)
        expiry_queue& operator=(expiry_queue&& other) noexcept {
            if (this != &other) {
                clear();
                _entries = std::move(other._entries);
                _watched = other._watched;
                _threshold = other._threshold;
                other._watched = 0;
                other._threshold = 16;
            }
            return *this;
        }

        expiry_queue(const expiry_queue&) = delete;
        expiry_queue& operator=(const expiry_queue&) = delete;

    private:
        struct Entry {
            tracked_ptr<detail::WeakCell> cell;
            function_type on_expire;
        };

        // The cell is an ordinary weak cell from now on: its target is no
        // longer kept for the queue. Returns the flags as they were: the
        // Drained among them says a cancel() got there first.
        static unsigned _release(detail::WeakCell* cell) noexcept {
            return cell->flags.fetch_or(detail::WeakCell::Drained, std::memory_order_acq_rel);
        }

        vector<Entry> _entries;
        size_type _watched = 0;
        size_type _threshold = 16;
    };
}
