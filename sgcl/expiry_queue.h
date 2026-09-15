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
    // makes a weak cell for the object and keeps f next to it. When a cycle finds the object unreachable it does not
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
    // found unreachable waits for that call. watch() returns an entry
    // handle: cancel() withdraws the entry (the object no longer kept,
    // f never called: a resource released by hand), weak() is a weak_ptr
    // to the object sharing the entry's cell. Ptr is the kind of the
    // queue's pointers: what watch() takes and what f receives, the word
    // by which the queue holds its entries and the handle its cell, so
    // where they live: a tracked_ptr, on a stack or in a managed object;
    // a gc::tracked_ptr (gc::expiry_queue), anywhere. Shared between
    // threads with the program's own synchronization; cancel() alone is
    // an atomic flag and may come from any thread.
    template<class T, template<class> class Ptr>
    class expiry_queue {
    public:
        using value_type = Ptr<T>;
        using weak_type = weak_ptr<T, Ptr>;
        using function_type = std::function<void(value_type)>;
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
            explicit entry(Ptr<detail::WeakCell> cell) noexcept
            : _cell(std::move(cell)) {
            }

            Ptr<detail::WeakCell> _cell;

            friend class expiry_queue;
        };

        expiry_queue() = default;

        // An entry for the object, with f kept for the day the object is
        // found unreachable; a null object gets no entry (an empty handle)
        template<class F>
        entry watch(const value_type& object, F&& on_expire) {
            if (!object) {
                return {};
            }
            weak_type w(weak_type::_make_cell(object.get(), detail::WeakCell::Watched));
            _entries.push_back(Entry{w._cell, function_type(std::forward<F>(on_expire))});
            if (++_watched > _threshold) {
                drain();
            }
            return entry(w._cell);
        }

        // Calls f with the object of every entry whose object a cycle has
        // found unreachable, and drops the entry; drops the cancelled
        // entries without a call. Returns how many functions were called.
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

        vector<Entry, Ptr> _entries;
        size_type _watched = 0;
        size_type _threshold = 16;
    };
}
