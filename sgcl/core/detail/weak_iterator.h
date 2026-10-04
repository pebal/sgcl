//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
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
    //
    // An iterator of an equal_range walks one object's run: the entries
    // of the object stand together in the chain, and the walk ends, at
    // the bound, at the first entry that is not the object's. So it ends
    // with the run whatever became of the entry the bound stands on: a
    // dead one dropped by a sweep, or a growth of the table that put
    // another entry behind the run. Only a weak_multimap has runs
    // (Runs): the iterators of the other weak containers carry no flag
    // and test none.
    struct WeakNoRun {};

    template<class Inner, class Key, class Reference, bool Runs = false>
    class WeakIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type = ptrdiff_t;
        using value_type = Reference;
        using reference = Reference;
        struct pointer {
            Reference ref;
            SGCL_INLINE_HOT const Reference* operator->() const noexcept { return &ref; }
        };

        WeakIterator() = default;

        SGCL_INLINE_HOT WeakIterator(Inner at, Inner end) noexcept
        : _at(at)
        , _end(end) {
            _settle();
        }

        // An iterator of a run, from its first entry up to its bound
        SGCL_INLINE_HOT WeakIterator(Inner at, Inner end, bool run) noexcept requires Runs
        : _at(at)
        , _end(end)
        , _run(run) {
            _settle();
        }

        // The iterator at `at`, the next node after an erasure through
        // `from`: with from's bound, and for a run, from's object
        SGCL_INLINE_HOT WeakIterator(Inner at, const WeakIterator& from) noexcept
        : _at(at)
        , _end(from._end)
        , _object(from._object)
        , _run(from._run) {
            _next();
        }

        // An iterator to a const_iterator, as std's containers convert:
        // the same place, the same object held
        template<class I2, class R2>
        requires (!std::is_same_v<I2, Inner> && std::is_convertible_v<I2, Inner>)
        SGCL_INLINE_HOT WeakIterator(const WeakIterator<I2, Key, R2, Runs>& o) noexcept
        : _at(o._at)
        , _end(o._end)
        , _object(o._object)
        , _run(o._run) {
        }

        SGCL_INLINE_HOT reference operator*() const noexcept {
            if constexpr(std::is_same_v<Reference, tracked_ptr<Key>>) {
                return _object;
            } else {
                return Reference::of(_object, *_at);
            }
        }

        SGCL_INLINE_HOT pointer operator->() const noexcept {
            return {**this};
        }

        SGCL_INLINE_HOT WeakIterator& operator++() noexcept {
            ++_at;
            _next();
            return *this;
        }

        SGCL_INLINE_HOT WeakIterator operator++(int) noexcept {
            auto it = *this;
            ++*this;
            return it;
        }

        SGCL_INLINE_HOT bool operator==(const WeakIterator& o) const noexcept {
            return _at == o._at;
        }

        SGCL_INLINE_HOT Inner inner() const noexcept {
            return _at;
        }

        // The bound: what an erase through the iterator keeps, so that a
        // walk of an equal_range stops where the range does
        SGCL_INLINE_HOT Inner bound() const noexcept {
            return _end;
        }

    private:
        template<class, class, class, bool> friend class WeakIterator;

        // Settled on what follows a step: the next live entry, or within
        // a run the run's next entry. In a weak_multimap the entry after
        // one of the object held is first asked whether it is the same
        // object's (live then, and held already: no lock); when it is
        // not, a run is over and the iterator is its bound
        SGCL_INLINE_HOT void _next() noexcept {
            if constexpr (Runs) {
                if (_at != _end && _at != Inner() && WeakIdentity::of(key_of(*_at)) == static_cast<const void*>(_object.get())) {
                    return;
                }
                if (_run) {
                    _at = _end;
                    _object = nullptr;
                    return;
                }
            }
            _settle();
        }

        // the first live entry from here on, held; or the bound, or the
        // chain's end (a bound erased from under the walk is never reached)
        void _settle() noexcept {
            for (; _at != _end && _at != Inner(); ++_at) {
                _object = key_of(*_at).lock();
                if (_object) {
                    return;
                }
            }
            _object = nullptr;
        }

        template<class V>
        SGCL_INLINE_HOT static auto& key_of(V& value) noexcept {
            if constexpr(requires { value.first; }) {
                return value.first;
            } else {
                return value;
            }
        }

        Inner _at;
        Inner _end;
        tracked_ptr<Key> _object;
        // an equal_range's: ends with the object's run (a weak_multimap's)
        [[no_unique_address]] std::conditional_t<Runs, bool, WeakNoRun> _run = {};
    };
}
