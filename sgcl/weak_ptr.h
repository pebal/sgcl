//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/maker.h"
#include "detail/weak_cell.h"
#include "tracked_ptr.h"
#include "unique_ptr.h"

namespace sgcl {
    // A pointer that does not keep its object alive: lock() is the object as
    // a tracked_ptr while it is reachable through strong pointers, and null
    // once a cycle has found it unreachable. One word, a tracked_ptr to a
    // cell on the managed heap (detail/weak_cell.h) that holds the target
    // as a word the collector clears instead of tracing; copies share the
    // cell, a weak_ptr made from a tracked_ptr gets a cell of its own. It
    // lives where a tracked_ptr may: in a managed object or on a stack.
    // Not for an object a unique_ptr owns: a tracked_ptr cannot address one
    // either, and its owner's delete would leave the cell dangling.
    template<class T>
    class weak_ptr {
    public:
        using element_type = typename tracked_ptr<T>::element_type;

        constexpr weak_ptr() noexcept = default;

        constexpr weak_ptr(std::nullptr_t) noexcept {
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        weak_ptr(const tracked_ptr<U>& p)
        : _cell(_make_cell(static_cast<element_type*>(p.get()))) {
        }

        weak_ptr(const weak_ptr&) noexcept = default;
        weak_ptr(weak_ptr&&) noexcept = default;

        template<class U, std::enable_if_t<std::is_convertible_v<typename weak_ptr<U>::element_type*, element_type*>, int> = 0>
        weak_ptr(const weak_ptr<U>& w) noexcept
        : _cell(w._cell) {
        }

        weak_ptr& operator=(const weak_ptr&) noexcept = default;
        weak_ptr& operator=(weak_ptr&&) noexcept = default;

        template<class U, std::enable_if_t<std::is_convertible_v<typename weak_ptr<U>::element_type*, element_type*>, int> = 0>
        weak_ptr& operator=(const weak_ptr<U>& w) noexcept {
            _cell = w._cell;
            return *this;
        }

        template<class U, std::enable_if_t<std::is_convertible_v<typename tracked_ptr<U>::element_type*, element_type*>, int> = 0>
        weak_ptr& operator=(const tracked_ptr<U>& p) {
            _cell = _make_cell(static_cast<element_type*>(p.get()));
            return *this;
        }

        weak_ptr& operator=(std::nullptr_t) noexcept {
            _cell = nullptr;
            return *this;
        }

        // The object, held: the protocol of atomic<tracked_ptr>::load. The
        // hazard pointer is published (seq_cst) before the cell is read
        // again, and the collector clears a cell before it reads the
        // hazards (collector.h: _clear_weak_cells), so a lock that races
        // with the clearing either sees the null or is seen: its target is
        // marked and survives the cycle.
        tracked_ptr<T> lock() const noexcept {
            auto cell = _cell.get();
            if (!cell) {
                return {};
            }
            auto& thread = detail::current_thread();
            auto l = (element_type*)cell->target.load(std::memory_order_seq_cst);
            element_type* t;
            do {
                t = l;
                thread.set_hazard_pointer(l);
                l = (element_type*)cell->target.load(std::memory_order_seq_cst);
            } while(l != t);
            tracked_ptr<T> p(l, detail::OnRegisteredThread{});
            thread.clear_hazard_pointer();
            return p;
        }

        // The cell has been cleared, or there is none: lock() would be null
        // (the other way round is not guaranteed: an object found
        // unreachable stays in the cell until the cycle clears it).
        bool expired() const noexcept {
            auto cell = _cell.get();
            return !cell || !cell->target.load(std::memory_order_acquire);
        }

        void reset() noexcept {
            _cell = nullptr;
        }

        void swap(weak_ptr& w) noexcept {
            _cell.swap(w._cell);
        }

    private:
        // The cell, constructed before its slot is published (maker.h)
        static tracked_ptr<detail::WeakCell> _make_cell(element_type* p, unsigned flags = 0) {
            if (!p) {
                return {};
            }
            assert(!detail::Page::is_unique(p) && "a weak_ptr cannot address an object a unique_ptr owns");
            return unique_ptr<detail::WeakCell>(detail::Maker<detail::WeakCell>::make_tracked_before_publish((void*)p, flags));
        }

        // A weak_ptr over a cell made with flags (expiry_queue.h)
        explicit weak_ptr(tracked_ptr<detail::WeakCell> cell) noexcept
        : _cell(std::move(cell)) {
        }

        tracked_ptr<detail::WeakCell> _cell;

        template<class> friend class weak_ptr;
        template<class> friend class expiry_queue;
    };

    template<class T>
    void swap(weak_ptr<T>& l, weak_ptr<T>& r) noexcept {
        l.swap(r);
    }

    template<class T>
    weak_ptr(const tracked_ptr<T>&) -> weak_ptr<T>;
}
