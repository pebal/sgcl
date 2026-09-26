//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/detail/transparent.h"
#include "../../core/aliases.h"
#include "../../core/detail/os.h"
#include "../../core/make_tracked.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../core/atomic.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace sgcl::concurrent::detail {
    using namespace sgcl::detail;
    template<class Key, class T, class Compare>
    struct ConcurrentSortedMapTraits {
        using key_type = Key;
        using value_type = pair<const Key, T>;
        using key_compare = Compare;
        static constexpr bool const_iterators = false;

        template<class P>
        static const auto& key(const P& p) noexcept {
            return p.first;
        }
    };

    template<class Key, class Compare>
    struct ConcurrentSortedSetTraits {
        using key_type = Key;
        using value_type = Key;
        using key_compare = Compare;
        static constexpr bool const_iterators = true;

        template<class K>
        static const K& key(const K& k) noexcept {
            return k;
        }
    };

    // The skip list under sorted_map and sorted_set (sorted_map.h
    // has the account of the algorithm): Traits names the element
    // (value_type), the key inside it, the comparison and whether the
    // iterators are const.
    template<class Traits>
    class SkipList {
    public:
        using key_type = typename Traits::key_type;
        using value_type = typename Traits::value_type;
        using key_compare = typename Traits::key_compare;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

    protected:
        using Key = key_type;
        using Compare = key_compare;
        using Value = value_type;

    private:

        // Up to 32 levels: with a quarter of the nodes rising to each next
        // level, that is the height of a list of 4^32 elements
        static constexpr size_t MaxHeight = 32;

        // The first word of every node: its bottom link (level 0) and its
        // height. A marker is a NodeBase of its own: linked after the node
        // it marks, at one level, holding what followed that node there.
        struct NodeBase {
            explicit NodeBase(unsigned h) noexcept
            : height(uint8_t(h))
            , marker(false) {
            }

            struct MarkerTag {};

            NodeBase(MarkerTag, tracked_ptr<NodeBase> succ) noexcept
            : next(succ)
            , height(1)
            , marker(true) {
            }

            atomic<tracked_ptr<NodeBase>> next;
            const uint8_t height;
            const bool marker;
            unsigned char _pad[6] = {};   // no tail padding: the value follows at a known offset
        };
        static_assert(sizeof(NodeBase) == 16);

        using Link = atomic<tracked_ptr<NodeBase>>;

        static constexpr size_t _align_up(size_t n, size_t a) noexcept {
            return (n + a - 1) & ~(a - 1);
        }

        // The element and the upper links at the same offsets in a node
        // of any height and in the head: what a search reads without
        // knowing the node's type
        static constexpr size_t ValueOffset = _align_up(sizeof(NodeBase), alignof(Value));
        static constexpr size_t LinkOffset = _align_up(ValueOffset + sizeof(Value), alignof(Link));

        template<size_t N, class = void>
        struct Tower {
            Link up[N];
        };

        template<class D>
        struct Tower<0, D> {
        };

        // A node of height H: the bottom link, the element, H - 1 links
        template<size_t H>
        struct Node : NodeBase {
            template<class... A>
            explicit Node(A&&... a)
            : NodeBase(unsigned(H))
            , value(std::forward<A>(a)...) {
                assert(reinterpret_cast<char*>(&value) - reinterpret_cast<char*>(this) == ptrdiff_t(ValueOffset));
                if constexpr (H > 1) {
                    assert(reinterpret_cast<char*>(&tower.up[0]) - reinterpret_cast<char*>(this) == ptrdiff_t(LinkOffset));
                }
            }

            Value value;
            [[no_unique_address]] Tower<H - 1> tower;
        };

        // The sentinel: every level's list starts here; no element, its
        // place left so that the links sit where a node's do
        struct Head : NodeBase {
            Head() noexcept
            : NodeBase(unsigned(MaxHeight)) {
                assert(reinterpret_cast<char*>(&up[0]) - reinterpret_cast<char*>(this) == ptrdiff_t(LinkOffset));
            }

            alignas(alignof(Value)) unsigned char place[sizeof(Value)] = {};
            Link up[MaxHeight - 1];
        };

        static Link& _link(NodeBase* node, size_t level) noexcept {
            assert(level < node->height);
            return level == 0 ? node->next : reinterpret_cast<Link*>(reinterpret_cast<char*>(node) + LinkOffset)[level - 1];
        }

        static value_type& _value(NodeBase* node) noexcept {
            assert(!node->marker);
            return *reinterpret_cast<value_type*>(reinterpret_cast<char*>(node) + ValueOffset);
        }

        static const Key& _key(NodeBase* node) noexcept {
            return Traits::key(_value(node));
        }

        // The first node after `node` on the bottom list that is not
        // erased, or null: what an iterator steps to. A marker after
        // `node` means `node` is erased and the marker leads on; a marker
        // after the candidate means the candidate is erased.
        static tracked_ptr<NodeBase> _next_live(NodeBase* node) noexcept {
            tracked_ptr<NodeBase> curr = node->next.load(std::memory_order_acquire);
            for (;;) {
                if (!curr) {
                    return curr;
                }
                if (curr->marker) {
                    curr = curr->next.load(std::memory_order_acquire);
                    continue;
                }
                tracked_ptr<NodeBase> succ = curr->next.load(std::memory_order_acquire);
                if (succ && succ->marker) {
                    curr = succ->next.load(std::memory_order_acquire);
                    continue;
                }
                return curr;
            }
        }

        // The iterator holds its node by a tracked_ptr: the node lives
        // for as long as the iterator does.
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

            operator Iterator<const SkipList::value_type>() const noexcept {
                return Iterator<const SkipList::value_type>(_node);
            }

        private:
            explicit Iterator(tracked_ptr<NodeBase> node) noexcept
            : _node(node) {
            }

            tracked_ptr<NodeBase> _node;

            friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept {
                return lhs._node.get() == rhs._node.get();
            }

            template<class> friend class Iterator;
            template<class> friend class SkipList;
        };

    public:
        using iterator = Iterator<std::conditional_t<Traits::const_iterators, const value_type, value_type>>;
        using const_iterator = Iterator<const value_type>;

        SkipList()
        : SkipList(Compare()) {
        }

        explicit SkipList(const Compare& comp)
        : _head(make_tracked<Head>())
        , _comp(comp) {
        }

        // A list built at once from a range, nobody sharing it yet: the
        // elements taken into a buffer, their order sorted (the first of
        // two with one key kept, as the inserts would keep it), and the
        // nodes linked in that order at every level behind the last one
        // there, a plain store each and no search; against an insert per
        // element, a search each
        template<std::input_iterator InputIt>
        SkipList(InputIt first, InputIt last, const Compare& comp = Compare())
        : SkipList(comp) {
            vector<value_type> items;   // the elements may hold tracked pointers: a managed buffer
            for (; first != last; ++first) {
                items.emplace_back(*first);
            }
            vector<size_t> order;
            order.reserve(items.size());
            for (size_t i = 0; i < items.size(); ++i) {
                order.push_back(i);
            }
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                return _comp(Traits::key(items[a]), Traits::key(items[b]));
            });
            NodeBase* tails[MaxHeight];   // the last node linked at each level
            detail::os::escape(tails);
            for (auto& t : tails) {
                t = static_cast<NodeBase*>(_head.get());
            }
            const value_type* previous = nullptr;
            for (size_t i : order) {
                if (previous && !_comp(Traits::key(*previous), Traits::key(items[i]))) {
                    continue;   // the same key as the one before: the first stays
                }
                previous = &items[i];
                unsigned h = _height();
                tracked_ptr<NodeBase> node = _make_node(h, std::move(items[i]));
                for (unsigned level = 0; level < h; ++level) {
                    _link(tails[level], level).store(node, std::memory_order_relaxed);
                    tails[level] = node.get();
                }
            }
        }

        SkipList(std::initializer_list<value_type> ilist, const Compare& comp = Compare())
        : SkipList(ilist.begin(), ilist.end(), comp) {
        }

        SkipList(const SkipList&) = delete;
        SkipList& operator=(const SkipList&) = delete;

        // Iteration: the elements in key order, weakly consistent
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

        // The number of elements at some moment of the walk: linear
        size_type size() const noexcept {
            size_type n = 0;
            for (tracked_ptr<NodeBase> node = _next_live(_head.get()); node; node = _next_live(node.get())) {
                ++n;
            }
            return n;
        }

        // Lookup: wait-free, the element as it was found. With a key of
        // another type the comparison takes (is_transparent: a string_view
        // for a string), no key is built for the search.
        iterator find(const Key& key) noexcept {
            return iterator(_find_node(key));
        }

        const_iterator find(const Key& key) const noexcept {
            return const_iterator(_find_node(key));
        }

        bool contains(const Key& key) const noexcept {
            return _find_node(key) != nullptr;
        }

        size_type count(const Key& key) const noexcept {
            return contains(key) ? 1 : 0;
        }

        iterator lower_bound(const Key& key) noexcept {
            return iterator(_search(key));
        }

        const_iterator lower_bound(const Key& key) const noexcept {
            return const_iterator(_search(key));
        }

        iterator upper_bound(const Key& key) noexcept {
            return iterator(_upper_node(key));
        }

        const_iterator upper_bound(const Key& key) const noexcept {
            return const_iterator(_upper_node(key));
        }

        template<class K> requires TransparentCompare<Compare>
        iterator find(const K& key) noexcept {
            return iterator(_find_node(key));
        }

        template<class K> requires TransparentCompare<Compare>
        const_iterator find(const K& key) const noexcept {
            return const_iterator(_find_node(key));
        }

        template<class K> requires TransparentCompare<Compare>
        bool contains(const K& key) const noexcept {
            return _find_node(key) != nullptr;
        }

        template<class K> requires TransparentCompare<Compare>
        size_type count(const K& key) const noexcept {
            return contains(key) ? 1 : 0;
        }

        template<class K> requires TransparentCompare<Compare>
        iterator lower_bound(const K& key) noexcept {
            return iterator(_search(key));
        }

        template<class K> requires TransparentCompare<Compare>
        const_iterator lower_bound(const K& key) const noexcept {
            return const_iterator(_search(key));
        }

        template<class K> requires TransparentCompare<Compare>
        iterator upper_bound(const K& key) noexcept {
            return iterator(_upper_node(key));
        }

        template<class K> requires TransparentCompare<Compare>
        const_iterator upper_bound(const K& key) const noexcept {
            return const_iterator(_upper_node(key));
        }

        // Insertion: the element and whether it was inserted, or the one
        // already there under the key and false, as std::map. emplace
        // builds the element first, in a node of its own, and drops the
        // node when the key turns out to be taken.
        template<class... A>
        pair<iterator, bool> emplace(A&&... a) {
            unsigned h = _height();
            tracked_ptr<NodeBase> node = _make_node(h, std::forward<A>(a)...);
            return _insert(node, h);
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
        // node is marked at each of its levels, the bottom one last (the
        // linearization point: one thread wins it), then unlinked by a
        // search.
        size_type erase(const Key& key) {
            return _erase_key(key);
        }

        template<class K> requires TransparentCompare<Compare>
            && (!std::is_convertible_v<const K&, const_iterator>)
        size_type erase(const K& key) {
            return _erase_key(key);
        }

        iterator erase(const_iterator pos) {
            tracked_ptr<NodeBase> node(pos._node);
            _erase(node);
            return iterator(_next_live(node.get()));
        }

        // Erases every element there is at the time of the walk
        void clear() {
            for (tracked_ptr<NodeBase> node = _next_live(_head.get()); node; node = _next_live(node.get())) {
                _erase(node);
            }
        }

        key_compare key_comp() const {
            return _comp;
        }

    protected:
        // The height of a new node: 1 with probability 3/4, one more with
        // a quarter of that, and so on (a xorshift64 of the thread's own),
        // and never more than one above the levels in use, as Java does
        // it, so that the top levels are not one node long
        unsigned _height() noexcept {
            return _height(_top.load(std::memory_order_relaxed));
        }

        // The height against the levels in use as the caller saw them
        // (a search's `top`): no more than one above those, so that the
        // search's neighbours cover every level of the node but a new
        // top one; the levels in use are raised when the height is
        // above them
        unsigned _height(unsigned top) noexcept {
            thread_local uint64_t state = 0x9E3779B97F4A7C15ull ^ reinterpret_cast<uintptr_t>(&state);
            uint64_t s = state;
            s ^= s << 13;
            s ^= s >> 7;
            s ^= s << 17;
            state = s;
            unsigned h = 1 + unsigned(std::countr_zero(s | (uint64_t(1) << 62))) / 2;
            if (h > top + 1) {
                h = top + 1;
            }
            unsigned used = _top.load(std::memory_order_relaxed);
            while (h > used && !_top.compare_exchange_weak(used, h, std::memory_order_relaxed)) {
            }
            return h;
        }

        // A node of height h, as a managed object of the type of that
        // height, built from the element's arguments
        template<size_t H = 1, class... A>
        static tracked_ptr<NodeBase> _make_node(unsigned h, A&&... a) {
            if constexpr (H < MaxHeight) {
                if (h != H) {
                    return _make_node<H + 1>(h, std::forward<A>(a)...);
                }
            }
            return make_tracked<Node<H>>(std::forward<A>(a)...);
        }

        static tracked_ptr<NodeBase> _make_marker(tracked_ptr<NodeBase> succ) {
            return make_tracked<NodeBase>(typename NodeBase::MarkerTag(), succ);
        }

        // The wait-free search of Herlihy and Shavit's contains: from the
        // top level in use, along each level while the keys are less than
        // the one sought, stepping over erased nodes (a marker after a
        // node) without touching anything, down to the bottom list. Returns
        // the first node there whose key is not less than the one sought,
        // null when there is none.
        template<class K>
        size_type _erase_key(const K& key) {
            NodeBase* preds[MaxHeight];
            NodeBase* succs[MaxHeight];
            detail::os::escape(preds);
            detail::os::escape(succs);
            unsigned top;
            if (!_find(key, preds, succs, top)) {
                return 0;
            }
            tracked_ptr<NodeBase> node(succs[0]);
            if (!_mark(node)) {
                return 0;
            }
            // Unlinked with the predecessors of the search above, from
            // the top level down: at each, a compare-exchange from the
            // node to what its marker there leads to; the first that
            // fails (the predecessor erased or moved on) hands the rest
            // to a search, which unlinks every marked node it meets
            for (unsigned level = node->height; level-- > 0;) {
                NodeBase* expected = node.get();
                tracked_ptr<NodeBase> after = _link(node.get(), level).load(std::memory_order_acquire)->next.load(std::memory_order_acquire);   // the marker's next
                tracked_ptr<NodeBase> e(expected);
                if (!_link(preds[level], level).compare_exchange_strong(e, after, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    _find(key, preds, succs, top);
                    break;
                }
            }
            return 1;
        }

        // The node with the key, null when there is none; the node after
        // the key's place
        template<class K>
        tracked_ptr<NodeBase> _find_node(const K& key) const noexcept {
            tracked_ptr<NodeBase> node = _search(key);
            return node && !_comp(key, _key(node.get())) ? node : tracked_ptr<NodeBase>();
        }

        template<class K>
        tracked_ptr<NodeBase> _upper_node(const K& key) const noexcept {
            tracked_ptr<NodeBase> node = _search(key);
            if (node && !_comp(key, _key(node.get()))) {
                node = _next_live(node.get());
            }
            return node;
        }

        template<class K>
        tracked_ptr<NodeBase> _search(const K& key) const noexcept {
            unsigned top = _top.load(std::memory_order_acquire);
            tracked_ptr<NodeBase> pred(static_cast<NodeBase*>(_head.get()));
            tracked_ptr<NodeBase> curr;
            for (unsigned level = top; level-- > 0;) {
                curr = _link(pred.get(), level).load(std::memory_order_acquire);
                for (;;) {
                    if (curr && curr->marker) {   // pred is erased at this level: its marker leads on
                        curr = curr->next.load(std::memory_order_acquire);
                        continue;
                    }
                    if (!curr) {
                        break;
                    }
                    tracked_ptr<NodeBase> succ = _link(curr.get(), level).load(std::memory_order_acquire);
                    if (succ && succ->marker) {   // curr is erased: step over it
                        curr = succ->next.load(std::memory_order_acquire);
                        continue;
                    }
                    if (!_comp(_key(curr.get()), key)) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            return curr;
        }

        // The search of Herlihy and Shavit's find: the same walk, but an
        // erased node met on the way is unlinked from the level with a
        // compare-exchange on its predecessor's link (and the walk starts
        // over when that fails: the predecessor changed, or was erased
        // itself). Fills preds[i] and succs[i], the nodes between which the
        // key sits at level i, for every level in use; returns whether
        // succs[0] holds the key, and the levels in use in `top`. The
        // arrays are the caller's, in its frame: raw pointers the
        // conservative scan sees, each copied there from a tracked_ptr of
        // this frame that held the node when it was loaded.
        template<class K>
        bool _find(const K& key, NodeBase** preds, NodeBase** succs, unsigned& top) {
        retry:
            top = _top.load(std::memory_order_acquire);
            tracked_ptr<NodeBase> pred(static_cast<NodeBase*>(_head.get()));
            tracked_ptr<NodeBase> curr;
            for (unsigned level = top; level-- > 0;) {
                curr = _link(pred.get(), level).load(std::memory_order_acquire);
                for (;;) {
                    if (curr && curr->marker) {   // pred is erased at this level
                        goto retry;
                    }
                    if (!curr) {
                        break;
                    }
                    tracked_ptr<NodeBase> succ = _link(curr.get(), level).load(std::memory_order_acquire);
                    if (succ && succ->marker) {   // curr is erased at this level: unlink it
                        tracked_ptr<NodeBase> after = succ->next.load(std::memory_order_acquire);
                        if (!_link(pred.get(), level).compare_exchange_strong(curr, after, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            goto retry;
                        }
                        curr = after;
                        continue;
                    }
                    if (!_comp(_key(curr.get()), key)) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
                preds[level] = pred.get();
                succs[level] = curr.get();
            }
            return curr && !_comp(key, _key(curr.get()));
        }

        // Links a node of height h whose element holds the key: into the
        // bottom list first, with the compare-exchange that makes it an
        // element of the map, then into each upper level, refreshing the
        // neighbours by a search when a compare-exchange fails; the node's
        // own link at a level is brought up to date under a
        // compare-exchange, which fails, and ends the linking, once the
        // node is erased there.
        pair<iterator, bool> _insert(tracked_ptr<NodeBase> node, unsigned h) {
            NodeBase* preds[MaxHeight];
            NodeBase* succs[MaxHeight];
            detail::os::escape(preds);
            detail::os::escape(succs);
            unsigned top;
            if (_find(_key(node.get()), preds, succs, top)) {
                return {iterator(tracked_ptr<NodeBase>(succs[0])), false};
            }
            return _link_node(node, h, preds, succs, top);
        }

        // Insertion of a key that may be there (try_emplace, the set's
        // insert): one search, and the node built by `make`, of the
        // height given, only when the key is absent, linked between the
        // neighbours that search found. The allocation sits between the
        // search and the link's compare-exchange: a neighbour changed
        // meanwhile fails the exchange and the search is done again, as
        // after any lost exchange. The height is drawn against the levels
        // the search saw, so that its neighbours cover every level of the
        // node but a new top one, where the head and null are the
        // neighbours; the levels in use are raised for a node that goes
        // in, never for a key found.
        template<class K, class Make>
        pair<iterator, bool> _insert_absent(const K& key, Make make) {
            NodeBase* preds[MaxHeight];
            NodeBase* succs[MaxHeight];
            detail::os::escape(preds);
            detail::os::escape(succs);
            unsigned top;
            if (_find(key, preds, succs, top)) {
                return {iterator(tracked_ptr<NodeBase>(succs[0])), false};
            }
            unsigned h = _height(top);
            if (h > top) {
                preds[top] = static_cast<NodeBase*>(_head.get());
                succs[top] = nullptr;
                top = h;
            }
            return _link_node(make(h), h, preds, succs, top);
        }

        // The linking, from the neighbours a search found: the bottom
        // list first, with the compare-exchange that makes the node an
        // element (the search done again when it fails, and the key may
        // be there by then), then the upper levels
        pair<iterator, bool> _link_node(tracked_ptr<NodeBase> node, unsigned h, NodeBase** preds, NodeBase** succs, unsigned top) {
            const Key& key = _key(node.get());
            for (;;) {
                assert(top >= h);   // _height raised the levels in use to h
                for (unsigned i = 0; i < h; ++i) {
                    _link(node.get(), i).store(tracked_ptr<NodeBase>(succs[i]), std::memory_order_relaxed);
                }
                tracked_ptr<NodeBase> expected(succs[0]);
                if (_link(preds[0], 0).compare_exchange_strong(expected, node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    break;
                }
                if (_find(key, preds, succs, top)) {
                    return {iterator(tracked_ptr<NodeBase>(succs[0])), false};
                }
            }
            for (unsigned i = 1; i < h; ++i) {
                for (;;) {
                    tracked_ptr<NodeBase> expected(succs[i]);
                    if (_link(preds[i], i).compare_exchange_strong(expected, node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        tracked_ptr<NodeBase> mine = _link(node.get(), i).load(std::memory_order_acquire);
                        if (mine && mine->marker) {   // erased meanwhile, and the eraser's search may have passed this level before the link: unlinked here, or the node stays reachable at this level (a search steps over it, only a mutating one unlinks) and holds its element
                            _find(key, preds, succs, top);
                            return {iterator(node), true};
                        }
                        break;
                    }
                    if (!_find(key, preds, succs, top) || succs[0] != node.get()) {   // erased meanwhile
                        return {iterator(node), true};
                    }
                    tracked_ptr<NodeBase> old = _link(node.get(), i).load(std::memory_order_acquire);
                    if (old && old->marker) {
                        return {iterator(node), true};
                    }
                    if (old.get() != succs[i] && !_link(node.get(), i).compare_exchange_strong(old, tracked_ptr<NodeBase>(succs[i]), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        return {iterator(node), true};
                    }
                }
            }
            return {iterator(node), true};
        }

        // Marks the node at each of its levels from the top down, the
        // bottom one last: the thread whose marker lands at the bottom has
        // erased the element, and unlinks the node with a search. Returns
        // false when another thread had marked the bottom first.
        bool _erase(tracked_ptr<NodeBase> node) {
            if (!_mark(node)) {
                return false;
            }
            NodeBase* preds[MaxHeight];
            NodeBase* succs[MaxHeight];
            detail::os::escape(preds);
            detail::os::escape(succs);
            unsigned top;
            _find(_key(node.get()), preds, succs, top);
            return true;
        }

        // The marking alone: true for the thread whose marker landed at
        // the bottom, which erased the element
        bool _mark(const tracked_ptr<NodeBase>& node) {
            for (unsigned level = node->height; level-- > 1;) {
                tracked_ptr<NodeBase> succ = _link(node.get(), level).load(std::memory_order_acquire);
                while (!(succ && succ->marker)) {
                    if (_link(node.get(), level).compare_exchange_strong(succ, _make_marker(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
            }
            tracked_ptr<NodeBase> succ = node->next.load(std::memory_order_acquire);
            for (;;) {
                if (succ && succ->marker) {
                    return false;
                }
                if (node->next.compare_exchange_strong(succ, _make_marker(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            }
        }

        tracked_ptr<Head> _head;
        atomic<unsigned> _top = 1;   // the levels in use: a search starts at the top one
        [[no_unique_address]] Compare _comp;
    };
}
