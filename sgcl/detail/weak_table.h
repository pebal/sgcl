//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../unordered_map.h"
#include "../unordered_multimap.h"
#include "../unordered_set.h"
#include "../weak_ptr.h"

#include <cstdint>

namespace sgcl::detail {
    // The object behind a weak pointer, read without locking it: the
    // cell's word, which the collector clears once the object is
    // unreachable and before the object's slot can be handed out again
    // (collector.h: the weak phase runs before the sweep, the free
    // bitmaps are rebuilt after it). So the word, while it holds an
    // address, names a live object, and the address of a live object
    // names it alone: the weak containers compare and hash by it.
    struct WeakIdentity {
        template<class T>
        static const void* of(const weak_ptr<T>& w) noexcept {
            auto cell = w._cell.get();
            return cell ? cell->target.load(std::memory_order_acquire) : nullptr;
        }
    };

    // The hash of an object's address, with the top bit set so that the
    // word, cached in the node or spilled on a stack, is never taken for
    // a heap address by a conservative scan
    inline size_t weak_hash(const void* p) noexcept {
        auto x = (uint64_t)(uintptr_t)p >> 4;
        x *= 0x9E3779B97F4A7C15ull;
        x ^= x >> 29;
        return (size_t)(x | (uint64_t(1) << 63));
    }

    // The hash and the equality of the weak containers' keys, transparent
    // to the strong pointers and the raw pointers the lookups bring: a
    // key whose object is gone equals nothing, itself included, so a
    // dead entry is never found and never blocks a live one.
    // The hash of a key changes when its object dies (the address is
    // gone), so the table must keep the hash it placed the node with:
    // libc++ and MSVC always do, libstdc++ only for a hash that may
    // throw, which is why the operator on a key is not noexcept.
    template<class Key>
    struct WeakHash {
        using is_transparent = void;
        size_t operator()(const weak_ptr<Key>& w) const {
            return weak_hash(WeakIdentity::of(w));
        }
        size_t operator()(const tracked_ptr<Key>& p) const noexcept {
            return weak_hash(p.get());
        }
        size_t operator()(const Key* p) const noexcept {
            return weak_hash(p);
        }
    };

    template<class Key>
    struct WeakEqual {
        using is_transparent = void;
        static const void* of(const weak_ptr<Key>& w) noexcept { return WeakIdentity::of(w); }
        static const void* of(const tracked_ptr<Key>& p) noexcept { return p.get(); }
        static const void* of(const Key* p) noexcept { return p; }
        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            auto x = of(a);
            return x && x == of(b);
        }
    };

    // What the weak containers share: a hash table keyed by weak pointers
    // (the Table: unordered_map, unordered_multimap or unordered_set of
    // them), the entries of dead objects swept out every so many
    // insertions, and an iterator that passes the dead ones over.
    template<class Key, class Table>
    class WeakTable {
    public:
        using key_type = Key;
        using key_pointer = tracked_ptr<Key>;
        using weak_type = weak_ptr<Key>;
        using table_type = Table;
        using size_type = size_t;

        WeakTable() = default;
        WeakTable(WeakTable&&) noexcept = default;
        WeakTable& operator=(WeakTable&&) noexcept = default;
        WeakTable(const WeakTable&) = delete;
        WeakTable& operator=(const WeakTable&) = delete;

        // The entries, the dead ones not yet swept included; empty() is
        // exact after a sweep()
        size_type size() const noexcept {
            return _table.size();
        }

        bool empty() const noexcept {
            return _table.empty();
        }

        // Drops the entries whose objects are gone; returns how many
        size_type sweep() {
            size_type count = 0;
            for (auto it = _table.begin(); it != _table.end();) {
                if (!WeakIdentity::of(_key_of(*it))) {
                    it = _table.erase(it);
                    ++count;
                } else {
                    ++it;
                }
            }
            _inserted = 0;
            _threshold = std::max<size_type>(16, _table.size());
            return count;
        }

        void clear() noexcept {
            _table.clear();
            _inserted = 0;
            _threshold = 16;
        }

        // The entries of the object: none for a null pointer, or an object
        // that is gone
        size_type count(const key_pointer& object) const {
            return object ? _table.count(object) : 0;
        }

        bool contains(const key_pointer& object) const {
            return count(object) != 0;
        }

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

        // One more insertion: a sweep every so many, as many as the table
        // has entries, so that a pass costs less than the insertions that
        // paid for it
        void _inserted_one() {
            if (++_inserted > _threshold) {
                sweep();
            }
        }

        Table _table;
        size_type _inserted = 0;
        size_type _threshold = 16;
    };
}
