//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../containers/detail/transparent.h"
#include "../core/aliases.h"
#include "../core/config.h"
#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "atomic.h"
#include "concurrent_map.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

namespace sgcl {
    // A key-value cache shared by any number of threads, bounded by a
    // capacity (a number of entries) and, if asked, by a time to live,
    // evicting the entries least recently used: what Guava's Cache and
    // Caffeine are in Java (the standard libraries of Go and Java have
    // none, everybody writes one), and the concurrent counterpart of the
    // LRU cache that ordered_map gives in two lines to one thread. Built
    // over concurrent_map, so that a lookup is the map's
    // wait-free search, and no lock and no shared write is on the path
    // of a hit: an exact LRU list is a global write on every access, the
    // one thing a cache read by many threads cannot afford (Caffeine's
    // lesson), so the order is approximated. Every entry keeps a stamp
    // of its last access, the value of a clock the cache's insertions
    // tick (a counter ticked by put, read by get: a hit stores the
    // current tick in its entry with one relaxed store, and only when it
    // differs from the one there, so a hot entry's line stays shared),
    // and when an insertion takes the size past the capacity the thread
    // that inserted evicts by sampling, as Redis's approximated LRU does:
    // a walk of `sample` entries on from a rotating position, the oldest
    // stamp among them erased, until the size is at the capacity. The
    // ticks are exactly as fine as the evictions need: two entries used
    // between the same two insertions are equally recent, an entry put
    // is as recent as the uses just before it and older than any use
    // after, and an entry untouched over the last n insertions is n
    // ticks old. With a sample of eight, an entry evicted is older than
    // seven others at least; what survives is what was used since most
    // of the cache was inserted, which is what LRU is for, and the
    // ordered_map is the one to reach for when the order has to be exact.
    //
    // A get is wait-free; a put is lock-free (the map's try_emplace, a
    // store into the entry on a key already there) plus the eviction it
    // owes, which is the walk and the erasures. The value lives in the
    // map's node, copied in by put and never modified there; a put on a
    // key already there puts the new value in a box of its own that the
    // entry points to (no moment of absence, and a get in flight reads
    // the old value whole), one load more on the gets of that entry. A
    // value is copied out (the entry may be evicted by another thread the
    // moment after), so the natural payload is a tracked_ptr, or a
    // string, one word each. An entry with a time to live is made absent
    // by the get that finds it stale, which erases it. get_or_compute
    // computes the value when the key is absent and inserts it; two
    // threads computing the same key at once both compute, one insertion
    // wins, and both return the value that won. hits() and misses() are
    // the counts of the gets, striped over cache lines as the map's count
    // is, and the stripes hold the cursors of the evictions, so that
    // threads evicting at once walk different stretches of the list. The
    // cache holds its map and its counters by tracked_ptrs, so it lives
    // where one may: on a stack or, for a cache the threads share, inside
    // a managed object under a root_ptr.
    template<class Key, class T, class Hash = std::hash<Key>, class KeyEqual = std::equal_to<Key>>
    class concurrent_cache {
        static_assert(std::is_copy_constructible_v<T>, "concurrent_cache copies the value in on put and out on get");

    public:
        using key_type = Key;
        using mapped_type = T;
        using hasher = Hash;
        using key_equal = KeyEqual;
        using size_type = size_t;
        using clock = std::chrono::steady_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

        static constexpr unsigned DefaultSample = 8;

    private:
        // A value put over one already there, and the moment it was put:
        // a managed object of its own, never modified, that the entry
        // points to from then on
        struct Box {
            template<class... A>
            explicit Box(time_point m, A&&... a)
            : value(std::forward<A>(a)...)
            , made(m) {
            }

            T value;
            time_point made;
        };

        // The entry in the map's node: the first value and the moment it
        // was put, never modified; the box of a later put, null until
        // then; the tick of the last access, which get writes and the
        // eviction reads; and the flag of its erasure: the cache erases
        // an entry by its node, and the one thread that sets the flag is
        // the one that erases the node and counts it off. A put that
        // finds the entry stores its box and then looks at the flag, an
        // eraser sets the flag and then looks at the box (a store then a
        // load on each side, seq_cst: one of the two sees the other), so
        // a put is never lost under an erasure: the put that sees the flag
        // puts again, the eraser that sees a fresh box gives the flag back
        struct Entry {
            template<class... A>
            explicit Entry(uint64_t s, time_point m, A&&... a)
            : value(std::forward<A>(a)...)
            , made(m)
            , stamp(s) {
            }

            T value;
            time_point made;
            atomic<tracked_ptr<Box>> replaced;
            atomic<uint64_t> stamp;
            atomic<bool> dead = {false};
        };

        using Map = concurrent_map<Key, Entry, Hash, KeyEqual>;
        using iterator = typename Map::iterator;

        // Where a stripe's last eviction ended, for its next to go on
        // from: an iterator, which holds its node, in a managed object
        // swapped whole under a compare-exchange. The node held is the
        // first of the next sample, so the next pass lets go of it; an
        // entry erased under it by a get or an erase lives on until then.
        struct Cursor {
            explicit Cursor(iterator it) noexcept
            : pos(it) {
            }

            iterator pos;
        };

        // The clock, the count and the stripes, on cache lines of their
        // own (the map's Counters has the same shape): the tick every put
        // advances and every get reads, with the count of the entries
        // next to it (the cache's own, exact: one word a put reads rather
        // than the sum of the map's stripes, which threads inserting at
        // once keep dirty), then a cell per thread with its hits, its
        // misses and its cursor, so that the threads do not fight over
        // one word, and threads evicting at once walk different stretches
        // of the list rather than the same one for the same victim
        static constexpr unsigned Stripes = 16;

        struct Counters {
            struct Cell {
                atomic<uint64_t> hits = {0};
                atomic<uint64_t> misses = {0};
                atomic<tracked_ptr<Cursor>> cursor;
                unsigned char _pad[config::CacheLineSize - 2 * sizeof(atomic<uint64_t>) - sizeof(atomic<tracked_ptr<Cursor>>)] = {};
            };

            atomic<uint64_t> tick = {0};
            unsigned char _pad0[config::CacheLineSize - sizeof(atomic<uint64_t>)] = {};
            atomic<long> size = {0};
            unsigned char _pad1[config::CacheLineSize - sizeof(atomic<long>)] = {};
            Cell cell[Stripes];
        };

    public:
        // A cache of `capacity` entries, with no time to live (ttl zero)
        // or one; `sample` entries are looked at per eviction. The map
        // gets its buckets for the capacity up front.
        explicit concurrent_cache(size_type capacity, duration ttl = duration::zero(), unsigned sample = DefaultSample, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual())
        : _map(std::max<size_type>(capacity, 16), hash, equal)
        , _counters(make_tracked<Counters>())
        , _capacity(capacity)
        , _ttl(ttl)
        , _sample(sample ? sample : 1) {
        }

        concurrent_cache(const concurrent_cache&) = delete;
        concurrent_cache& operator=(const concurrent_cache&) = delete;

        // A copy of the value under the key, or nothing: absent, or older
        // than the time to live (erased then). Wait-free: the map's
        // search, a load of the entry's box, a relaxed store of the tick
        // into the entry when it changed. With a key of another type the
        // hash and the equality take (is_transparent: a string_view for a
        // string), no key is built for the search.
        optional<T> get(const Key& key) {
            return _get(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        optional<T> get(const K& key) {
            return _get(key);
        }

        // Inserts a copy of the value under the key, or replaces the one
        // there; then, if the size is past the capacity, evicts down to it
        void put(const Key& key, const T& value) {
            _put(key, value);
        }

        void put(const Key& key, T&& value) {
            _put(key, std::move(value));
        }

        // The value under the key, computed by f() and put when it is
        // absent or stale. Two threads that find the key absent at once
        // both compute; one insertion wins and both return its value, so
        // f is a function of the key alone, called once or more.
        template<class F>
        T get_or_compute(const Key& key, F&& f) {
            if (optional<T> v = _get(key)) {
                return *std::move(v);
            }
            T value = std::forward<F>(f)();
            time_point made = _now();
            uint64_t tick = _tick();
            tracked_ptr<Box> mine;
            for (;;) {
                auto [it, inserted] = mine ? _map.try_emplace(key, tick, made, std::as_const(mine->value)) : _map.try_emplace(key, tick, made, std::as_const(value));
                if (inserted) {
                    _evict(_inserted());
                    return mine ? mine->value : value;
                }
                Entry& e = it->second;   // another thread's, made meanwhile: its value, unless it is stale already
                for (;;) {
                    tracked_ptr<Box> box = e.replaced.load(std::memory_order_acquire);
                    if (!_stale(box ? box->made : e.made, _now())) {
                        e.stamp.store(tick, std::memory_order_relaxed);
                        return box ? box->value : e.value;
                    }
                    if (!mine) {
                        mine = make_tracked<Box>(made, std::move(value));
                    }
                    if (e.replaced.compare_exchange_strong(box, mine, std::memory_order_seq_cst, std::memory_order_acquire)) {
                        if (e.dead.load(std::memory_order_seq_cst)) {
                            break;   // an eraser claimed the entry: the node may be gone with the box; put again (Entry)
                        }
                        e.stamp.store(tick, std::memory_order_relaxed);
                        return mine->value;
                    }
                }
            }
        }

        // Erases the entry under the key: whether there was one (an entry
        // another thread is erasing at the moment counts as gone)
        bool erase(const Key& key) {
            return _erase(key);
        }

        template<class K> requires detail::TransparentLookup<Hash, KeyEqual>
        bool erase(const K& key) {
            return _erase(key);
        }

        // Erases every entry there is at the time of the walk, and lets go
        // of the cursors, which hold the nodes the last evictions ended
        // at; the counts of the gets stay
        void clear() {
            for (iterator it = _map.begin(); it != _map.end(); ++it) {
                _erase_node(it);
            }
            for (auto& c : _counters->cell) {
                c.cursor.store(nullptr, std::memory_order_release);
            }
        }

        // The number of entries: exact, and at most the capacity once the
        // puts have returned
        size_type size() const noexcept {
            long n = _counters->size.load(std::memory_order_relaxed);
            return n > 0 ? size_type(n) : 0;
        }

        bool empty() const noexcept {
            return _map.empty();
        }

        size_type capacity() const noexcept {
            return _capacity;
        }

        duration ttl() const noexcept {
            return _ttl;
        }

        unsigned sample_size() const noexcept {
            return _sample;
        }

        // The gets that found a value and the gets that did not (an entry
        // found stale is a miss), the sums of the stripes
        uint64_t hits() const noexcept {
            uint64_t n = 0;
            for (auto& c : _counters->cell) {
                n += c.hits.load(std::memory_order_relaxed);
            }
            return n;
        }

        uint64_t misses() const noexcept {
            uint64_t n = 0;
            for (auto& c : _counters->cell) {
                n += c.misses.load(std::memory_order_relaxed);
            }
            return n;
        }

        hasher hash_function() const {
            return _map.hash_function();
        }

        key_equal key_eq() const {
            return _map.key_eq();
        }

    private:
        // The clock is read only when there is a time to live
        time_point _now() const noexcept {
            return _ttl != duration::zero() ? clock::now() : time_point();
        }

        bool _stale(time_point made, time_point now) const noexcept {
            return _ttl != duration::zero() && now - made > _ttl;
        }

        // The clock of the cache, advanced by every put: the tick before
        // the advance is the stamp of what is put, so that an entry put
        // is as recent as the gets just before it and older than any get
        // after it
        uint64_t _tick() noexcept {
            return _counters->tick.fetch_add(1, std::memory_order_relaxed);
        }

        typename Counters::Cell& _cell() noexcept {
            static atomic<unsigned> next_stripe = {0};
            thread_local unsigned stripe = next_stripe.fetch_add(1, std::memory_order_relaxed) % Stripes;
            return _counters->cell[stripe];
        }

        template<class K>
        optional<T> _get(const K& key) {
            iterator it = _map.find(key);
            if (it == _map.end()) {
                _cell().misses.fetch_add(1, std::memory_order_relaxed);
                return nullopt;
            }
            Entry& e = it->second;
            tracked_ptr<Box> box = e.replaced.load(std::memory_order_acquire);
            if (_stale(box ? box->made : e.made, _now())) {
                if (!e.dead.exchange(true, std::memory_order_seq_cst)) {   // claimed: erased here, unless a put came meanwhile (Entry)
                    box = e.replaced.load(std::memory_order_seq_cst);
                    if (_stale(box ? box->made : e.made, _now())) {
                        _map.erase(it);
                        _counters->size.fetch_sub(1, std::memory_order_relaxed);
                        _cell().misses.fetch_add(1, std::memory_order_relaxed);
                        return nullopt;
                    }
                    e.dead.store(false, std::memory_order_seq_cst);   // a fresh value after all: the entry stays, and this is a hit
                } else {
                    _cell().misses.fetch_add(1, std::memory_order_relaxed);
                    return nullopt;
                }
            }
            uint64_t tick = _counters->tick.load(std::memory_order_relaxed);
            if (e.stamp.load(std::memory_order_relaxed) != tick) {
                e.stamp.store(tick, std::memory_order_relaxed);
            }
            _cell().hits.fetch_add(1, std::memory_order_relaxed);
            return box ? box->value : e.value;
        }

        // The node takes a copy of the value, so that an insertion lost
        // to another thread's (the node built and dropped) loses nothing;
        // a key already there gets the value in a box, moved when it may be
        template<class V>
        void _put(const Key& key, V&& value) {
            time_point made = _now();
            uint64_t tick = _tick();
            tracked_ptr<Box> box;
            for (;;) {
                auto [it, inserted] = box ? _map.try_emplace(key, tick, made, std::as_const(box->value)) : _map.try_emplace(key, tick, made, std::as_const(value));
                if (inserted) {
                    _evict(_inserted());
                    return;
                }
                if (!box) {
                    box = make_tracked<Box>(made, std::forward<V>(value));
                }
                Entry& e = it->second;
                e.replaced.store(box, std::memory_order_seq_cst);
                if (e.dead.load(std::memory_order_seq_cst)) {
                    continue;   // an eraser claimed the entry: the node may be gone with the box; put again (Entry)
                }
                e.stamp.store(tick, std::memory_order_relaxed);
                return;
            }
        }

        // The count kept: an insertion counted once its node is linked,
        // an erasure once by the one thread that claimed the node (Entry:
        // dead), so the count is the map's, exact
        long _inserted() noexcept {
            return _counters->size.fetch_add(1, std::memory_order_relaxed) + 1;
        }

        // The node claimed and erased: true when this thread erased it
        bool _erase_node(iterator it) {
            if (it->second.dead.exchange(true, std::memory_order_seq_cst)) {
                return false;
            }
            _map.erase(it);
            _counters->size.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }

        template<class K>
        bool _erase(const K& key) {
            iterator it = _map.find(key);
            return it != _map.end() && _erase_node(it);
        }

        // Down to the capacity, from a count of `n`: a pass per entry
        // over, the count read again after each, until a pass finds
        // nothing to look at (the map emptied under it)
        void _evict(long n) {
            while (n > long(_capacity)) {
                if (!_evict_one()) {
                    return;
                }
                n = _counters->size.load(std::memory_order_relaxed);
            }
        }

        // One pass: `sample` entries on from the stripe's cursor (wrapping
        // at the end; a stripe's first pass starts a stretch of its own
        // into the list), the stale ones erased on the way, then the
        // oldest stamp among the rest erased unless the stale were
        // enough; the cursor moved to where the walk ended. False when
        // there was nothing to walk.
        bool _evict_one() {
            typename Counters::Cell& cell = _cell();
            tracked_ptr<Cursor> cursor = cell.cursor.load(std::memory_order_acquire);
            iterator it = cursor ? cursor->pos : _map.begin();
            if (!cursor) {
                for (unsigned n = unsigned(&cell - _counters->cell) * _sample; n != 0 && it != _map.end(); --n) {
                    ++it;
                }
            }
            if (it == _map.end()) {
                it = _map.begin();
                if (it == _map.end()) {
                    return false;
                }
            }
            time_point now = _now();
            iterator oldest = _map.end();
            uint64_t oldest_stamp = std::numeric_limits<uint64_t>::max();
            bool stale = false;
            for (unsigned n = 0; n < _sample; ++n) {
                if (it == _map.end()) {
                    it = _map.begin();
                    if (it == _map.end()) {
                        break;
                    }
                }
                Entry& e = it->second;
                if (_ttl != duration::zero()) {
                    tracked_ptr<Box> box = e.replaced.load(std::memory_order_acquire);
                    if (_stale(box ? box->made : e.made, now)) {
                        iterator victim = it++;
                        stale |= _erase_node(victim);
                        continue;
                    }
                }
                uint64_t stamp = e.stamp.load(std::memory_order_relaxed);
                if (stamp < oldest_stamp) {
                    oldest_stamp = stamp;
                    oldest = it;
                }
                ++it;
            }
            if (oldest != _map.end() && (!stale || size() > _capacity)) {
                _erase_node(oldest);
            }
            cell.cursor.compare_exchange_strong(cursor, make_tracked<Cursor>(it), std::memory_order_acq_rel, std::memory_order_relaxed);
            return true;
        }

        Map _map;
        tracked_ptr<Counters> _counters;
        const size_type _capacity;
        const duration _ttl;
        const unsigned _sample;
    };
}
