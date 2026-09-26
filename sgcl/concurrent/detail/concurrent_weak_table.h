//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/atomic.h"
#include "../../core/detail/weak_table.h"
#include "../../core/tracked_ptr.h"
#include "../../core/weak_ptr.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sgcl::concurrent::detail {
    using namespace sgcl::detail;
    // The key of the concurrent weak containers: the weak pointer to the
    // object and the hash the entry was placed with. The sequential weak
    // containers key their tables by the weak pointer alone and hash it
    // by the address the cell holds (weak_table.h); the split-ordered
    // list (split_list.h) keeps a node's split key from the hash at the
    // insertion and hashes the key again to erase by iterator, and the
    // address, so the hash, is gone once the object dies: a dead entry
    // erased by that hash would be sought in the wrong bucket and stay
    // linked until a walk happened to pass it. So the key carries its
    // hash, and the entry is erased from where it was put. The hash has
    // its top bit set (weak_hash), so the word is never taken for a heap
    // address by a conservative scan. lock() is what the iterator of the
    // weak containers asks of a key (weak_iterator.h).
    template<class Key>
    struct WeakKey {
        weak_ptr<Key> weak;
        size_t hash;

        tracked_ptr<Key> lock() const noexcept {
            return weak.lock();
        }
    };

    // The hash and the equality of the concurrent weak containers' keys:
    // by the object's identity, as WeakHash and WeakEqual (weak_table.h),
    // the hash of a key the one it carries; transparent to the strong
    // pointers and the raw pointers the lookups bring. A key whose object
    // is gone equals nothing, itself included, so a dead entry is never
    // found and never blocks the entry of the object that takes the slot
    // next.
    template<class Key>
    struct ConcurrentWeakHash {
        using is_transparent = void;

        size_t operator()(const WeakKey<Key>& k) const noexcept {
            return k.hash;
        }

        size_t operator()(const tracked_ptr<Key>& p) const noexcept {
            return weak_hash(p.get());
        }

        size_t operator()(const Key* p) const noexcept {
            return weak_hash(p);
        }
    };

    template<class Key>
    struct ConcurrentWeakEqual {
        using is_transparent = void;

        static const void* of(const WeakKey<Key>& k) noexcept {
            return WeakIdentity::of(k.weak);
        }

        static const void* of(const tracked_ptr<Key>& p) noexcept {
            return p.get();
        }

        static const void* of(const Key* p) noexcept {
            return p;
        }

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            auto x = of(a);
            return x && x == of(b);
        }
    };

    // What the concurrent weak containers share: a lock-free hash table
    // keyed by WeakKeys (the Table: a map or a
    // set of them), the entries of dead objects
    // swept out every so many insertions, and the counters that decide
    // when. The sweep is a walk of the table that erases, by iterator,
    // every entry whose cell is cleared: the collector clears a cell
    // before the object's slot can be handed out again, so an entry seen
    // dead is dead for good and nobody can find it alive meanwhile, and
    // erasing it races with nothing but another erase of the same node,
    // which the split list settles (one thread marks it). The inserting
    // thread whose insertion brings the count since the last sweep to
    // the threshold (as many as the table has entries, 16 at least, so
    // that a pass costs less than the insertions that paid for it) runs
    // the sweep; one sweep runs at a time, a thread finding one under
    // way goes on without waiting (sweep() returns 0 to it), so an
    // insertion never blocks on a sweep and stays lock-free.
    template<class Key, class Table>
    class ConcurrentWeakTable {
    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using weak_type = weak_ptr<Key>;
        using table_type = Table;
        using size_type = size_t;

        ConcurrentWeakTable() = default;
        ConcurrentWeakTable(const ConcurrentWeakTable&) = delete;
        ConcurrentWeakTable& operator=(const ConcurrentWeakTable&) = delete;

        // The entries, the dead ones not yet swept included: the sum of
        // the table's stripes, a snapshot under concurrent modification;
        // empty() is exact after a sweep() with the threads quiet
        size_type size() const noexcept {
            return _table.size();
        }

        bool empty() const noexcept {
            return _table.empty();
        }

        // Drops the entries whose objects are gone; returns how many, or
        // 0 at once when another thread's sweep is under way
        size_type sweep() {
            bool expected = false;
            if (!_sweeping.compare_exchange_strong(expected, true, std::memory_order_acquire, std::memory_order_relaxed)) {
                return 0;
            }
            struct Done {   // the flag given back however the walk ends: an erase may throw (the marker's allocation), and a flag left set would end every later sweep at once
                atomic<bool>& flag;
                ~Done() { flag.store(false, std::memory_order_release); }
            } done{_sweeping};
            _inserted.store(0, std::memory_order_relaxed);
            size_type count = 0;
            for (auto it = _table.begin(); it != _table.end();) {
                if (!WeakIdentity::of(_key_of(*it).weak)) {
                    it = _table.erase(it);
                    ++count;
                } else {
                    ++it;
                }
            }
            _threshold.store(std::max<size_type>(16, _table.size()), std::memory_order_relaxed);
            _inserted.store(0, std::memory_order_relaxed);
            return count;
        }

        // Every entry there is at the time of the walk, dead or alive
        void clear() {
            _table.clear();
            _inserted.store(0, std::memory_order_relaxed);
            _threshold.store(16, std::memory_order_relaxed);
        }

        // The entries of the object: none for a null pointer, or an object
        // that is gone; wait-free
        size_type count(const key_pointer& object) const noexcept {
            return object ? _table.count(object) : 0;
        }

        bool contains(const key_pointer& object) const noexcept {
            return count(object) != 0;
        }

        // The entry of the object, dropped: how many (0 or 1); lock-free
        size_type erase(const key_pointer& object) {
            return object ? _table.erase(object) : 0;
        }

    protected:
        template<class V>
        static auto& _key_of(V& value) noexcept {
            if constexpr(requires { value.first; }) {
                return value.first;
            } else {
                return value;
            }
        }

        static WeakKey<Key> _key(const key_pointer& object) {
            return {weak_type(object), weak_hash(object.get())};
        }

        // One more insertion: the sweep when the count reaches the
        // threshold, by this thread unless another is at it
        void _inserted_one() {
            if (_inserted.fetch_add(1, std::memory_order_relaxed) + 1 >= _threshold.load(std::memory_order_relaxed)) {
                sweep();
            }
        }

        // The table with its insertion of a key that may be there opened
        // to this class: one search, the node built only when the key is
        // absent (split_list.h: _insert_absent), which the weak
        // containers and intern do with the object's key, or with the
        // object itself made only then
        struct Opened : Table {
            using Table::Table;
            using Table::_insert_absent;
            using Table::_make_node;
        };

        Opened _table;
        atomic<size_type> _inserted = {0};
        atomic<size_type> _threshold = {16};
        atomic<bool> _sweeping = {false};
    };
}
