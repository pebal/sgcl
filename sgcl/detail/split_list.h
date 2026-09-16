//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../aliases.h"
#include "../array.h"
#include "../atomic.h"
#include "../atomic_ref.h"
#include "../config.h"
#include "../make_tracked.h"
#include "../tracked_ptr.h"
#include "managed.h"
#include "os.h"

#include <atomic>
#include <bit>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // Root: the kind of the word by which the container holds its bucket
    // array, its head node and its counters, and its iterators hold their
    // node (types.h); the links between the nodes are tracked_ptrs
    // whatever it is.
    template<class Key, class T, class Hash, class KeyEqual, template<class> class Root>
    struct ConcurrentUnorderedMapTraits {
        using key_type = Key;
        using value_type = pair<const Key, T>;
        using stored_type = managed_value_t<value_type>;
        using hasher = Hash;
        using key_equal = KeyEqual;
        template<class U> using root = Root<U>;
        static constexpr bool const_iterators = false;

        template<class P>
        static const auto& key(const P& p) noexcept {
            return p.first;
        }
    };

    template<class Key, class Hash, class KeyEqual, template<class> class Root>
    struct ConcurrentUnorderedSetTraits {
        using key_type = Key;
        using value_type = Key;
        using stored_type = managed_t<value_type>;
        using hasher = Hash;
        using key_equal = KeyEqual;
        template<class U> using root = Root<U>;
        static constexpr bool const_iterators = true;

        template<class K>
        static const K& key(const K& k) noexcept {
            return k;
        }
    };

    // The hash table under concurrent_unordered_map and
    // concurrent_unordered_set (concurrent_unordered_map.h has the account
    // of the algorithm): the split-ordered list of Shalev and Shavit, one
    // lock-free sorted list holding every element, ordered by the bit
    // reversal of the hash, with an array of buckets that point into it at
    // dummy nodes; the array doubles, the list never moves a node.
    template<class Traits>
    class SplitList {
    public:
        using key_type = typename Traits::key_type;
        using value_type = typename Traits::value_type;
        using hasher = typename Traits::hasher;
        using key_equal = typename Traits::key_equal;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

    protected:
        using Key = key_type;
        template<class U> using Ptr = typename Traits::template root<U>;
        using Value = typename Traits::stored_type;

    private:
        // The first words of every node: the link, the split-order key
        // (the bit reversal of the hash with the low bit set for an
        // element, of the bucket's number with it clear for a bucket's
        // dummy node), and what the node is. A marker is a node of its
        // own linked after the node it marks, holding what followed.
        enum Kind : uint8_t { Element, Dummy, Marker };

        struct NodeBase {
            NodeBase(uint64_t k, Kind what) noexcept
            : skey(k)
            , kind(what) {
            }

            NodeBase(tracked_ptr<NodeBase> succ) noexcept   // a marker
            : next(succ)
            , skey(0)
            , kind(Marker) {
            }

            atomic<tracked_ptr<NodeBase>> next;
            uint64_t skey;   // set before the node is published
            const Kind kind;
            unsigned char _pad[7] = {};
        };
        static_assert(sizeof(NodeBase) == 24);

        struct Node : NodeBase {
            template<class... A>
            explicit Node(uint64_t k, A&&... a)
            : NodeBase(k, Element)
            , value(std::forward<A>(a)...) {
            }

            Value value;
        };

        // The bucket array: a managed buffer of links to the dummies, the
        // slot of bucket 0 the head of the list; a slot is null until the
        // bucket is used. Replaced by one twice as long when the elements
        // outnumber the buckets; the old one is garbage once nothing walks
        // it, and a bucket a thread initialized in the old array after the
        // copy is initialized again in the new, a second dummy with the
        // same key that the list takes in its stride.
        struct Buckets {
            explicit Buckets(size_type n)
            : slots(n) {
            }

            array<tracked_ptr<NodeBase>> slots;
        };

        // The count of the elements, striped over cache lines so that the
        // threads do not fight over one word (Java's LongAdder): a thread
        // adds to its own stripe, size() sums them
        static constexpr unsigned Stripes = 16;

        struct Counters {
            struct Cell {
                atomic<long> n = {0};
                unsigned char _pad[config::CacheLineSize - sizeof(atomic<long>)] = {};
            };

            Cell cell[Stripes];
        };

        static constexpr size_type InitialBuckets = 16;

        // The bits of a word in reverse order: one instruction where the
        // compiler has it (rbit on arm64), six swaps elsewhere
        static uint64_t _reverse(uint64_t x) noexcept {
#if defined(__has_builtin)
#if __has_builtin(__builtin_bitreverse64)
            return __builtin_bitreverse64(x);
#endif
#endif
            x = ((x >> 1) & 0x5555555555555555ull) | ((x & 0x5555555555555555ull) << 1);
            x = ((x >> 2) & 0x3333333333333333ull) | ((x & 0x3333333333333333ull) << 2);
            x = ((x >> 4) & 0x0F0F0F0F0F0F0F0Full) | ((x & 0x0F0F0F0F0F0F0F0Full) << 4);
            x = ((x >> 8) & 0x00FF00FF00FF00FFull) | ((x & 0x00FF00FF00FF00FFull) << 8);
            x = ((x >> 16) & 0x0000FFFF0000FFFFull) | ((x & 0x0000FFFF0000FFFFull) << 16);
            return (x >> 32) | (x << 32);
        }

        static uint64_t _element_key(size_t hash) noexcept {
            return _reverse(uint64_t(hash)) | 1;
        }

        static uint64_t _dummy_key(size_type bucket) noexcept {
            return _reverse(uint64_t(bucket));
        }

        // The bucket that a bucket's dummy is inserted after: the number
        // with its highest bit cleared
        static size_type _parent(size_type bucket) noexcept {
            assert(bucket != 0);
            return bucket & ~(size_type(1) << (std::bit_width(bucket) - 1));
        }

        static value_type& _value(NodeBase* node) noexcept {
            assert(node->kind == Element);
            return reinterpret_cast<value_type&>(static_cast<Node*>(node)->value);
        }

        static const Key& _key(NodeBase* node) noexcept {
            return Traits::key(_value(node));
        }

        // The first element after `node` on the list, or null: what an
        // iterator steps to; markers lead on, dummies and marked nodes are
        // passed
        static tracked_ptr<NodeBase> _next_live(NodeBase* node) noexcept {
            tracked_ptr<NodeBase> curr = node->next.load(std::memory_order_acquire);
            for (;;) {
                if (!curr) {
                    return curr;
                }
                if (curr->kind == Marker) {
                    curr = curr->next.load(std::memory_order_acquire);
                    continue;
                }
                tracked_ptr<NodeBase> succ = curr->next.load(std::memory_order_acquire);
                if ((succ && succ->kind == Marker) || curr->kind == Dummy) {
                    curr = succ && succ->kind == Marker ? succ->next.load(std::memory_order_acquire) : succ;
                    continue;
                }
                return curr;
            }
        }

        template<class U>
        class Iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using iterator_concept = std::forward_iterator_tag;
            using value_type = std::remove_const_t<U>;
            using difference_type = ptrdiff_t;
            using pointer = U*;
            using reference = U&;

            Iterator() noexcept = default;

            reference operator*() const noexcept {
                return _value(_node.get());
            }

            pointer operator->() const noexcept {
                return &_value(_node.get());
            }

            Iterator& operator++() noexcept {
                _node = _next_live(_node.get());
                return *this;
            }

            Iterator operator++(int) noexcept {
                Iterator it = *this;
                ++*this;
                return it;
            }

            operator Iterator<const SplitList::value_type>() const noexcept {
                return Iterator<const SplitList::value_type>(_node);
            }

        private:
            explicit Iterator(tracked_ptr<NodeBase> node) noexcept
            : _node(node) {
            }

            Ptr<NodeBase> _node;

            friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept {
                return lhs._node.get() == rhs._node.get();
            }

            template<class> friend class Iterator;
            template<class> friend class SplitList;
        };

    public:
        using iterator = Iterator<std::conditional_t<Traits::const_iterators, const value_type, value_type>>;
        using const_iterator = Iterator<const value_type>;

        SplitList()
        : SplitList(InitialBuckets) {
        }

        explicit SplitList(size_type buckets, const hasher& hash = hasher(), const key_equal& equal = key_equal())
        : _head(make_tracked<NodeBase>(uint64_t(0), Dummy))
        , _counters(make_tracked<Counters>())
        , _hash(hash)
        , _equal(equal) {
            size_type n = std::bit_ceil(std::max<size_type>(buckets, 2));
            tracked_ptr<Buckets> b = make_tracked<Buckets>(n);
            b->slots[0] = _head;
            _buckets.store(b, std::memory_order_release);
        }

        template<std::input_iterator InputIt>
        SplitList(InputIt first, InputIt last, size_type buckets = InitialBuckets, const hasher& hash = hasher(), const key_equal& equal = key_equal())
        : SplitList(buckets, hash, equal) {
            insert(first, last);
        }

        SplitList(std::initializer_list<value_type> ilist, size_type buckets = InitialBuckets, const hasher& hash = hasher(), const key_equal& equal = key_equal())
        : SplitList(ilist.begin(), ilist.end(), buckets, hash, equal) {
        }

        SplitList(const SplitList&) = delete;
        SplitList& operator=(const SplitList&) = delete;

        // Iteration: the elements in no particular order (the order of the
        // list, the bit reversal of their hashes), weakly consistent
        iterator begin() noexcept {
            return iterator(_next_live(_head.get()));
        }

        const_iterator begin() const noexcept {
            return const_iterator(_next_live(_head.get()));
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            return iterator(tracked_ptr<NodeBase>());
        }

        const_iterator end() const noexcept {
            return const_iterator(tracked_ptr<NodeBase>());
        }

        const_iterator cend() const noexcept {
            return end();
        }

        bool empty() const noexcept {
            return !_next_live(_head.get());
        }

        // The number of elements: the sum of the stripes, a snapshot of
        // no particular moment when other threads modify the container
        size_type size() const noexcept {
            long n = 0;
            for (auto& c : _counters->cell) {
                n += c.n.load(std::memory_order_relaxed);
            }
            return n > 0 ? size_type(n) : 0;
        }

        size_type bucket_count() const noexcept {
            tracked_ptr<Buckets> b = _buckets.load(std::memory_order_acquire);
            return b->slots.size();
        }

        // Buckets for at least `count` elements: the array grown now
        // rather than by the insertions
        void reserve(size_type count) {
            for (;;) {
                tracked_ptr<Buckets> b = _buckets.load(std::memory_order_acquire);
                if (b->slots.size() >= count) {
                    return;
                }
                _grow(b);
            }
        }

        hasher hash_function() const {
            return _hash;
        }

        key_equal key_eq() const {
            return _equal;
        }

        // Lookup: wait-free
        iterator find(const Key& key) noexcept {
            return iterator(_search(key));
        }

        const_iterator find(const Key& key) const noexcept {
            return const_iterator(_search(key));
        }

        bool contains(const Key& key) const noexcept {
            return _search(key) != nullptr;
        }

        size_type count(const Key& key) const noexcept {
            return contains(key) ? 1 : 0;
        }

        // Insertion: the element and whether it was inserted, or the one
        // already there under the key and false, as std::unordered_map.
        // emplace builds the element first, in a node of its own, and
        // drops the node when the key turns out to be taken.
        template<class... A>
        pair<iterator, bool> emplace(A&&... a) {
            tracked_ptr<NodeBase> node = make_tracked<Node>(uint64_t(0), std::forward<A>(a)...);
            return _insert(node, _hash(_key(node.get())));
        }

        pair<iterator, bool> insert(const value_type& value) {
            return emplace(value);
        }

        pair<iterator, bool> insert(value_type&& value) {
            return emplace(std::move(value));
        }

        template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last) {
            for (; first != last; ++first) {
                emplace(*first);
            }
        }

        void insert(std::initializer_list<value_type> ilist) {
            insert(ilist.begin(), ilist.end());
        }

        // Erasure: the number of elements erased, 0 or 1; erase(it) erases
        // the element the iterator addresses, if it is still there. The
        // node is marked (the linearization point: one thread wins it),
        // then unlinked by a search.
        size_type erase(const Key& key) {
            size_t hash = _hash(key);
            tracked_ptr<Buckets> b = _buckets.load(std::memory_order_acquire);
            tracked_ptr<NodeBase> pred, curr;
            if (!_find(_bucket(*b, hash & (b->slots.size() - 1)), _element_key(hash), &key, pred, curr)) {
                return 0;
            }
            return _erase(curr, hash) ? 1 : 0;
        }

        iterator erase(const_iterator pos) {
            tracked_ptr<NodeBase> node(pos._node);
            _erase(node, _hash(_key(node.get())));
            return iterator(_next_live(node.get()));
        }

        // Erases every element there is at the time of the walk
        void clear() {
            for (tracked_ptr<NodeBase> node = _next_live(_head.get()); node; node = _next_live(node.get())) {
                _erase(node, _hash(_key(node.get())));
            }
        }

    protected:
        template<class... A>
        static tracked_ptr<NodeBase> _make_node(A&&... a) {
            return make_tracked<Node>(uint64_t(0), std::forward<A>(a)...);
        }

        static tracked_ptr<NodeBase> _make_marker(tracked_ptr<NodeBase> succ) {
            return make_tracked<NodeBase>(succ);
        }

        // The wait-free search: from the bucket's dummy along the list
        // while the split keys are less than the one sought, then through
        // the elements of the same split key for the key itself, stepping
        // over erased nodes without touching anything. The element, or
        // null.
        tracked_ptr<NodeBase> _search(const Key& key) const noexcept {
            size_t hash = _hash(key);
            uint64_t skey = _element_key(hash);
            tracked_ptr<Buckets> b = _buckets.load(std::memory_order_acquire);
            tracked_ptr<NodeBase> curr = _bucket_or_parent(*b, hash & (b->slots.size() - 1));
            curr = curr->next.load(std::memory_order_acquire);
            for (;;) {
                if (curr && curr->kind == Marker) {
                    curr = curr->next.load(std::memory_order_acquire);
                    continue;
                }
                if (!curr || curr->skey > skey) {
                    return tracked_ptr<NodeBase>();
                }
                tracked_ptr<NodeBase> succ = curr->next.load(std::memory_order_acquire);
                if (succ && succ->kind == Marker) {
                    curr = succ->next.load(std::memory_order_acquire);
                    continue;
                }
                if (curr->skey == skey && curr->kind == Element && _equal(_key(curr.get()), key)) {
                    return curr;
                }
                curr = succ;
            }
        }

        // The search of Harris and Michael: the same walk from `start`,
        // an erased node met on the way unlinked with a compare-exchange
        // on its predecessor's link (and the walk started over when that
        // fails). Leaves pred and curr around the place of the split key:
        // curr the node found (an element with the key, or, for a dummy
        // search with no key, the dummy of that split key), returning
        // true; or the first node past the split key, returning false.
        bool _find(tracked_ptr<NodeBase> start, uint64_t skey, const Key* key, tracked_ptr<NodeBase>& pred, tracked_ptr<NodeBase>& curr) {
        retry:
            pred = start;
            curr = pred->next.load(std::memory_order_acquire);
            for (;;) {
                if (curr && curr->kind == Marker) {   // pred is erased
                    goto retry;
                }
                if (!curr || curr->skey > skey) {
                    return false;
                }
                tracked_ptr<NodeBase> succ = curr->next.load(std::memory_order_acquire);
                if (succ && succ->kind == Marker) {   // curr is erased: unlink it
                    tracked_ptr<NodeBase> after = succ->next.load(std::memory_order_acquire);
                    if (!pred->next.compare_exchange_strong(curr, after, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = after;
                    continue;
                }
                if (curr->skey == skey) {
                    if (key ? (curr->kind == Element && _equal(_key(curr.get()), *key)) : curr->kind == Dummy) {
                        return true;
                    }
                }
                pred = curr;
                curr = succ;
            }
        }

        // The dummy of a bucket, made on first use: inserted into the list
        // after the parent bucket's dummy (made first if need be) and set
        // into the slot with a compare-exchange; another thread's dummy
        // found in either place is taken instead
        tracked_ptr<NodeBase> _bucket(Buckets& b, size_type i) {
            tracked_ptr<NodeBase> d = atomic_ref(b.slots[i]).load(std::memory_order_acquire);
            if (d) {
                return d;
            }
            tracked_ptr<NodeBase> parent = _bucket(b, _parent(i));
            uint64_t skey = _dummy_key(i);
            tracked_ptr<NodeBase> dummy = make_tracked<NodeBase>(skey, Dummy);
            tracked_ptr<NodeBase> pred, curr;
            for (;;) {
                if (_find(parent, skey, nullptr, pred, curr)) {
                    dummy = curr;
                    break;
                }
                dummy->next.store(curr, std::memory_order_relaxed);
                if (pred->next.compare_exchange_strong(curr, dummy, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    break;
                }
            }
            tracked_ptr<NodeBase> expected;
            if (!atomic_ref(b.slots[i]).compare_exchange_strong(expected, dummy, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return expected;
            }
            return dummy;
        }

        // For a lookup, which never writes: the bucket's dummy, or the
        // nearest initialized ancestor's, from which the walk is only
        // longer
        tracked_ptr<NodeBase> _bucket_or_parent(Buckets& b, size_type i) const noexcept {
            for (;;) {
                tracked_ptr<NodeBase> d = atomic_ref(b.slots[i]).load(std::memory_order_acquire);
                if (d) {
                    return d;
                }
                i = _parent(i);
            }
        }

        // Links a node holding the key of hash `hash`: into its bucket's
        // list at the split key, with the compare-exchange that makes it
        // an element of the container
        pair<iterator, bool> _insert(tracked_ptr<NodeBase> node, size_t hash) {
            uint64_t skey = _element_key(hash);
            const Key& key = _key(node.get());
            for (;;) {
                tracked_ptr<Buckets> b = _buckets.load(std::memory_order_acquire);
                tracked_ptr<NodeBase> start = _bucket(*b, hash & (b->slots.size() - 1));
                tracked_ptr<NodeBase> pred, curr;
                if (_find(start, skey, &key, pred, curr)) {
                    return {iterator(curr), false};
                }
                node->skey = skey;
                node->next.store(curr, std::memory_order_relaxed);
                if (pred->next.compare_exchange_strong(curr, node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    _count(1, *b);
                    return {iterator(node), true};
                }
            }
        }

        bool _erase(tracked_ptr<NodeBase> node, size_t hash) {
            tracked_ptr<NodeBase> succ = node->next.load(std::memory_order_acquire);
            for (;;) {
                if (succ && succ->kind == Marker) {
                    return false;
                }
                if (node->next.compare_exchange_strong(succ, _make_marker(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    tracked_ptr<Buckets> b = _buckets.load(std::memory_order_acquire);
                    tracked_ptr<NodeBase> pred, curr;
                    _find(_bucket(*b, hash & (b->slots.size() - 1)), _element_key(hash), &_key(node.get()), pred, curr);
                    _count(-1, *b);
                    return true;
                }
            }
        }

        // The count, on this thread's stripe; every 64 insertions on a
        // stripe the sum is looked at, and the array doubled once the
        // elements outnumber the buckets
        void _count(long delta, Buckets& b) {
            static atomic<unsigned> next_stripe = {0};
            thread_local unsigned stripe = next_stripe.fetch_add(1, std::memory_order_relaxed) % Stripes;
            long n = _counters->cell[stripe].n.fetch_add(delta, std::memory_order_relaxed) + delta;
            if (delta > 0 && (n & 63) == 0 && size() > b.slots.size()) {
                _grow(tracked_ptr<Buckets>(&b));
            }
        }

        // The array doubled: the slots copied, the rest null, and the new
        // array published with a compare-exchange (a failure is another
        // thread's growth)
        void _grow(tracked_ptr<Buckets> old) {
            size_type n = old->slots.size();
            tracked_ptr<Buckets> b = make_tracked<Buckets>(n * 2);
            for (size_type i = 0; i < n; ++i) {
                b->slots[i] = atomic_ref(old->slots[i]).load(std::memory_order_acquire);
            }
            _buckets.compare_exchange_strong(old, b, std::memory_order_acq_rel, std::memory_order_relaxed);
        }

        atomic<Ptr<Buckets>> _buckets;
        Ptr<NodeBase> _head;        // the dummy of bucket 0: the head of the list
        Ptr<Counters> _counters;
        [[no_unique_address]] hasher _hash;
        [[no_unique_address]] key_equal _equal;
    };
}
