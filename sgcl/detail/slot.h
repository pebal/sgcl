//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "page.h"

#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // Storage for one element inside a managed node. The containers
    // construct and destroy the element themselves, at insert and erase
    // time, as the standard containers do; the node itself is reclaimed
    // later by the collector. Whether the element is still alive is kept
    // in the slot's state byte on the page, not in the node: an element the
    // container destroyed leaves the node in state Destroyed, which the
    // sweep frees without running the node's destructor (the way a
    // unique_ptr's deleter does). The node's destructor therefore runs only
    // for a node whose element is alive, with one exception it checks for:
    // a node whose element's constructor threw is Destroyed as well.
    // A union with the element as its only member keeps the element's
    // pointers at fixed offsets, which the pointer maps require; the page
    // is zero when it is issued to the node's type and a destroyed element
    // leaves its pointers null (maker.h: _init), so an unconstructed
    // element holds null pointers only.
    template<class T, bool Trivial = std::is_trivially_destructible_v<T>>
    struct Slot {
        union {
            T value;
        };

        Slot() noexcept {
        }

        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;

        ~Slot() {
            if (Page::state_of(this) != State::Destroyed) {
                value.~T();
            }
        }

        template<class... A>
        T& construct(A&&... a) {
            assert(Page::state_of(this) != State::Destroyed);
            try {
                ::new (static_cast<void*>(&value)) T(std::forward<A>(a)...);
            } catch (...) {
                Page::set_state<State::Destroyed>(this);   // nothing to destroy at the sweep
                throw;
            }
            return value;
        }

        void destroy() noexcept {
            assert(Page::state_of(this) != State::Destroyed);
            value.~T();
            Page::set_state<State::Destroyed>(this);
        }
    };

    // A trivially destructible element: nothing runs at erase or at the
    // sweep, so the slot keeps no state and is trivially destructible
    // itself, which leaves the node without a destroy function.
    template<class T>
    struct Slot<T, true> {
        union {
            T value;
        };

        Slot() noexcept {
        }

        Slot(const Slot&) = delete;
        Slot& operator=(const Slot&) = delete;

        template<class... A>
        T& construct(A&&... a) {
            ::new (static_cast<void*>(&value)) T(std::forward<A>(a)...);
            return value;
        }

        void destroy() noexcept {
        }
    };
}
