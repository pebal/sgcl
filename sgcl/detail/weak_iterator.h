//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "weak_table.h"

#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // The iterator of a weak container: over the table's nodes, passing
    // over the entries whose objects are gone, up to its own bound (the
    // table's end, or the end of an equal_range). Where it stands it
    // holds the object as a strong pointer, so the entry cannot die under
    // it; the reference it gives out carries that pointer (the pointer
    // itself for a set). It is a tracked object then, and lives where
    // the container's pointers may.
    template<class Inner, class Key, class Reference>
    class WeakIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = Reference;
        using reference = Reference;
        struct pointer {
            Reference ref;
            const Reference* operator->() const noexcept { return &ref; }
        };

        WeakIterator() = default;

        WeakIterator(Inner at, Inner end) noexcept
        : _at(at)
        , _end(end) {
            _settle();
        }

        reference operator*() const noexcept {
            if constexpr(std::is_same_v<Reference, tracked_ptr<Key>>) {
                return _object;
            } else {
                return Reference::of(_object, *_at);
            }
        }

        pointer operator->() const noexcept {
            return {**this};
        }

        WeakIterator& operator++() noexcept {
            ++_at;
            _settle();
            return *this;
        }

        WeakIterator operator++(int) noexcept {
            auto it = *this;
            ++*this;
            return it;
        }

        bool operator==(const WeakIterator& o) const noexcept {
            return _at == o._at;
        }

        Inner inner() const noexcept {
            return _at;
        }

    private:
        // the first live entry from here on, held; or the bound
        void _settle() noexcept {
            for (; _at != _end; ++_at) {
                _object = key_of(*_at).lock();
                if (_object) {
                    return;
                }
            }
            _object = nullptr;
        }

        template<class V>
        static auto& key_of(V& value) noexcept {
            if constexpr(requires { value.first; }) {
                return value.first;
            } else {
                return value;
            }
        }

        Inner _at;
        Inner _end;
        tracked_ptr<Key> _object;
    };
}
