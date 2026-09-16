//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../make_tracked.h"
#include "../tracked_ptr.h"
#include "anchor.h"
#include "slot.h"
#include "synth_three_way.h"

#include <algorithm>
#include <compare>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

namespace sgcl::detail {
    // The red-black tree behind sgcl::map, set, multimap and multiset.
    //
    // Layout as in libstdc++: a managed header node whose parent is the
    // root, left the leftmost and right the rightmost node, so that begin()
    // is O(1) and --end() is the maximum; the header is red and the root
    // black, which is how a decrement tells them apart. Links are tracked
    // pointers, so the whole tree hangs off the container's header pointer
    // and is traced from there. Everything else runs on raw pointers:
    // descents, the read-only lookups and the iterators, which hold one
    // raw node pointer and cost nothing to copy or advance (no write
    // barrier, no card check). That is sound because a linked node is
    // rooted by the tree, and an iterator to an erased element is invalid
    // exactly as in std. A node that is unlinked and then still touched
    // (its links cleared, its element destroyed, or handed to a node
    // handle) is held by a tracked_ptr on the mutator's stack for the
    // duration: a raw pointer may live only in a register, which the
    // collector does not see, and a concurrent cycle could sweep the node.
    // Elements live in a Slot and are destroyed at erase time; the node
    // memory is reclaimed by the collector later.
    //
    // The header is allocated on the first insertion, so an empty container
    // costs nothing and the default constructor cannot throw. Until then
    // begin() and end() are both null iterators (they compare equal, and
    // neither may be dereferenced or moved); an end() taken before the
    // first insertion does not compare equal to end() after it.

    template<class Compare>
    concept TransparentCompare = requires { typename Compare::is_transparent; };

    struct RbNodeBase {
        tracked_ptr<RbNodeBase> parent;
        tracked_ptr<RbNodeBase> left;
        tracked_ptr<RbNodeBase> right;
        bool red;
    };

    template<class V>
    struct RbNode : RbNodeBase {
        Slot<V> slot;
    };

    using RbPtr = tracked_ptr<RbNodeBase>;

    // The red-black tree algorithms of the standard library's tree (the
    // header node's left and right are the leftmost and rightmost nodes,
    // its parent the root): the extremes of a subtree, the in-order
    // neighbours, the rotations, and the rebalancing after an insertion
    // and before an erase. The links are tracked pointers, relinked with
    // reset (one store with its barrier).
    inline RbNodeBase* rb_minimum(RbNodeBase* x) noexcept {
        while (x->left) {
            x = x->left.get();
        }
        return x;
    }

    inline RbNodeBase* rb_maximum(RbNodeBase* x) noexcept {
        while (x->right) {
            x = x->right.get();
        }
        return x;
    }

    inline RbNodeBase* rb_increment(RbNodeBase* x) noexcept {
        if (x->right) {
            x = x->right.get();
            while (x->left) {
                x = x->left.get();
            }
        } else {
            RbNodeBase* y = x->parent.get();
            while (x == y->right.get()) {
                x = y;
                y = y->parent.get();
            }
            if (x->right.get() != y) {
                x = y;
            }
        }
        return x;
    }

    inline RbNodeBase* rb_decrement(RbNodeBase* x) noexcept {
        if (x->red && x->parent && x->parent->parent.get() == x) {
            // the header: its predecessor is the maximum
            x = x->right.get();
        } else if (x->left) {
            x = x->left.get();
            while (x->right) {
                x = x->right.get();
            }
        } else {
            RbNodeBase* y = x->parent.get();
            while (x == y->left.get()) {
                x = y;
                y = y->parent.get();
            }
            x = y;
        }
        return x;
    }

    inline void rb_rotate_left(RbNodeBase* x, RbNodeBase* header) noexcept {
        RbNodeBase* y = x->right.get();
        x->right = y->left;
        if (y->left) {
            y->left->parent.reset(x);
        }
        y->parent = x->parent;
        if (x == header->parent.get()) {
            header->parent.reset(y);
        } else if (x == x->parent->left.get()) {
            x->parent->left.reset(y);
        } else {
            x->parent->right.reset(y);
        }
        y->left.reset(x);
        x->parent.reset(y);
    }

    inline void rb_rotate_right(RbNodeBase* x, RbNodeBase* header) noexcept {
        RbNodeBase* y = x->left.get();
        x->left = y->right;
        if (y->right) {
            y->right->parent.reset(x);
        }
        y->parent = x->parent;
        if (x == header->parent.get()) {
            header->parent.reset(y);
        } else if (x == x->parent->right.get()) {
            x->parent->right.reset(y);
        } else {
            x->parent->left.reset(y);
        }
        y->right.reset(x);
        x->parent.reset(y);
    }

    // Links x under p (left or right child) and restores the invariants.
    inline void rb_insert_and_rebalance(bool insert_left, RbNodeBase* x, RbNodeBase* p, RbNodeBase* header) noexcept {
        x->parent.reset(p);
        x->left = nullptr;
        x->right = nullptr;
        x->red = true;
        if (insert_left) {
            p->left.reset(x);
            if (p == header) {
                header->parent.reset(x);
                header->right.reset(x);
            } else if (p == header->left.get()) {
                header->left.reset(x);
            }
        } else {
            p->right.reset(x);
            if (p == header->right.get()) {
                header->right.reset(x);
            }
        }
        while (x != header->parent.get() && x->parent->red) {
            RbNodeBase* xp = x->parent.get();
            RbNodeBase* xpp = xp->parent.get();
            if (xp == xpp->left.get()) {
                RbNodeBase* y = xpp->right.get();
                if (y && y->red) {
                    xp->red = false;
                    y->red = false;
                    xpp->red = true;
                    x = xpp;
                } else {
                    if (x == xp->right.get()) {
                        x = xp;
                        rb_rotate_left(x, header);
                        xp = x->parent.get();
                    }
                    xp->red = false;
                    xpp->red = true;
                    rb_rotate_right(xpp, header);
                }
            } else {
                RbNodeBase* y = xpp->left.get();
                if (y && y->red) {
                    xp->red = false;
                    y->red = false;
                    xpp->red = true;
                    x = xpp;
                } else {
                    if (x == xp->left.get()) {
                        x = xp;
                        rb_rotate_right(x, header);
                        xp = x->parent.get();
                    }
                    xp->red = false;
                    xpp->red = true;
                    rb_rotate_left(xpp, header);
                }
            }
        }
        header->parent->red = false;
    }

    // Unlinks z from the tree and restores the invariants. z's own links
    // are left as they were; the caller clears them (a stale word pointing
    // at a dead node, on a conservatively scanned stack, must not retain
    // the nodes it once linked to) and destroys the element or hands the
    // node over to a node handle.
    inline void rb_rebalance_for_erase(RbNodeBase* z, RbNodeBase* header) noexcept {
        RbNodeBase* y = z;
        RbNodeBase* x = nullptr;
        RbNodeBase* x_parent = nullptr;
        if (!y->left) {
            x = y->right.get();
        } else if (!y->right) {
            x = y->left.get();
        } else {
            y = y->right.get();
            while (y->left) {
                y = y->left.get();
            }
            x = y->right.get();
        }
        if (y != z) {
            // y, the successor, takes z's place
            z->left->parent.reset(y);
            y->left = z->left;
            if (y != z->right.get()) {
                x_parent = y->parent.get();
                if (x) {
                    x->parent = y->parent;
                }
                y->parent->left.reset(x);
                y->right = z->right;
                z->right->parent.reset(y);
            } else {
                x_parent = y;
            }
            if (header->parent.get() == z) {
                header->parent.reset(y);
            } else if (z->parent->left.get() == z) {
                z->parent->left.reset(y);
            } else {
                z->parent->right.reset(y);
            }
            y->parent = z->parent;
            std::swap(y->red, z->red);
            y = z;
        } else {
            x_parent = y->parent.get();
            if (x) {
                x->parent = y->parent;
            }
            if (header->parent.get() == z) {
                header->parent.reset(x);
            } else if (z->parent->left.get() == z) {
                z->parent->left.reset(x);
            } else {
                z->parent->right.reset(x);
            }
            if (header->left.get() == z) {
                if (!z->right) {
                    header->left = z->parent;
                } else {
                    header->left.reset(rb_minimum(x));
                }
            }
            if (header->right.get() == z) {
                if (!z->left) {
                    header->right = z->parent;
                } else {
                    header->right.reset(rb_maximum(x));
                }
            }
        }
        if (!y->red) {
            while (x != header->parent.get() && (!x || !x->red)) {
                if (x == x_parent->left.get()) {
                    RbNodeBase* w = x_parent->right.get();
                    if (w->red) {
                        w->red = false;
                        x_parent->red = true;
                        rb_rotate_left(x_parent, header);
                        w = x_parent->right.get();
                    }
                    if ((!w->left || !w->left->red) && (!w->right || !w->right->red)) {
                        w->red = true;
                        x = x_parent;
                        x_parent = x_parent->parent.get();
                    } else {
                        if (!w->right || !w->right->red) {
                            w->left->red = false;
                            w->red = true;
                            rb_rotate_right(w, header);
                            w = x_parent->right.get();
                        }
                        w->red = x_parent->red;
                        x_parent->red = false;
                        if (w->right) {
                            w->right->red = false;
                        }
                        rb_rotate_left(x_parent, header);
                        break;
                    }
                } else {
                    RbNodeBase* w = x_parent->left.get();
                    if (w->red) {
                        w->red = false;
                        x_parent->red = true;
                        rb_rotate_right(x_parent, header);
                        w = x_parent->left.get();
                    }
                    if ((!w->right || !w->right->red) && (!w->left || !w->left->red)) {
                        w->red = true;
                        x = x_parent;
                        x_parent = x_parent->parent.get();
                    } else {
                        if (!w->left || !w->left->red) {
                            w->right->red = false;
                            w->red = true;
                            rb_rotate_left(w, header);
                            w = x_parent->left.get();
                        }
                        w->red = x_parent->red;
                        x_parent->red = false;
                        if (w->left) {
                            w->left->red = false;
                        }
                        rb_rotate_right(x_parent, header);
                        break;
                    }
                }
            }
            if (x) {
                x->red = false;
            }
        }
    }

    // One raw pointer: the node. Trivially copyable, so it may live anywhere
    // (in a std::vector, say), not only on the stack or in a managed
    // object; the node it points at is rooted by the tree for as long as
    // the element is in the container. Element iterators stay valid across
    // insertions, erasures of other elements, swaps and moves of the
    // container, because they follow the node.
    template<class V, bool Const>
    class RbIterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = V;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<Const, const V*, V*>;
        using reference = std::conditional_t<Const, const V&, V&>;

        RbIterator() noexcept = default;

        explicit RbIterator(RbNodeBase* node) noexcept
        : _node(node) {
        }

        template<bool C = Const, std::enable_if_t<C, int> = 0>
        RbIterator(const RbIterator<V, false>& other) noexcept
        : _node(other._node) {
        }

        reference operator*() const noexcept {
            return static_cast<RbNode<V>*>(_node)->slot.value;
        }

        pointer operator->() const noexcept {
            return std::addressof(**this);
        }

        RbIterator& operator++() noexcept {
            _node = rb_increment(_node);
            return *this;
        }

        RbIterator operator++(int) noexcept {
            RbIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        RbIterator& operator--() noexcept {
            _node = rb_decrement(_node);
            return *this;
        }

        RbIterator operator--(int) noexcept {
            RbIterator tmp = *this;
            --(*this);
            return tmp;
        }

    private:
        RbNodeBase* _node = nullptr;

        friend bool operator==(const RbIterator& lhs, const RbIterator& rhs) noexcept {
            return lhs._node == rhs._node;
        }

        template<class, bool> friend class RbIterator;
        template<class> friend class RbTree;
    };

    // The node handle's part that does not depend on map/set: it owns one
    // unlinked node and destroys the element when it dies unused.
    template<class V>
    class NodeHandleBase {
    public:
        NodeHandleBase() noexcept = default;

        NodeHandleBase(NodeHandleBase&& other) noexcept
        : _node(other._node) {
            other._node = nullptr;
        }

        NodeHandleBase& operator=(NodeHandleBase&& other) noexcept {
            if (this != &other) {
                _destroy();
                _node = other._node;
                other._node = nullptr;
            }
            return *this;
        }

        ~NodeHandleBase() {
            _destroy();
        }

        [[nodiscard]] bool empty() const noexcept {
            return !_node;
        }

        explicit operator bool() const noexcept {
            return _node != nullptr;
        }

    protected:
        explicit NodeHandleBase(RbNodeBase* node) noexcept
        : _node(node) {
        }

        V& _value() const noexcept {
            return static_cast<RbNode<V>*>(_node.get())->slot.value;
        }

        void _destroy() noexcept {
            if (_node) {
                static_cast<RbNode<V>*>(_node.get())->slot.destroy();
                _node = nullptr;
            }
        }

        RbPtr _node;

        template<class> friend class RbTree;
    };

    template<class Key, class T>
    class MapNodeHandle : public NodeHandleBase<std::pair<const Key, T>> {
        using Base = NodeHandleBase<std::pair<const Key, T>>;

    public:
        using key_type = Key;
        using mapped_type = T;

        MapNodeHandle() noexcept = default;

        // Writable, as in std: the node is out of any tree, so its key may
        // change before it goes back in.
        key_type& key() const noexcept {
            return const_cast<key_type&>(this->_value().first);
        }

        mapped_type& mapped() const noexcept {
            return this->_value().second;
        }

    private:
        explicit MapNodeHandle(RbNodeBase* node) noexcept
        : Base(node) {
        }

        template<class> friend class RbTree;
    };

    template<class Key>
    class SetNodeHandle : public NodeHandleBase<Key> {
        using Base = NodeHandleBase<Key>;

    public:
        using value_type = Key;

        SetNodeHandle() noexcept = default;

        value_type& value() const noexcept {
            return this->_value();
        }

    private:
        explicit SetNodeHandle(RbNodeBase* node) noexcept
        : Base(node) {
        }

        template<class> friend class RbTree;
    };

    template<class Iterator, class NodeType>
    struct InsertReturn {
        Iterator position;
        bool inserted;
        NodeType node;
    };

    template<class Key, class T, class Compare>
    class MapValueCompare {
    public:
        using value_type = std::pair<const Key, T>;

        bool operator()(const value_type& lhs, const value_type& rhs) const {
            return comp(lhs.first, rhs.first);
        }

    protected:
        Compare comp;

        MapValueCompare(Compare c)
        : comp(c) {
        }

        template<class> friend class RbTree;
    };

    template<class Key, class T, class Compare, bool Multi>
    struct MapTraits {
        using key_type = Key;
        using value_type = std::pair<const Key, T>;
        using key_compare = Compare;
        using value_compare = MapValueCompare<Key, T, Compare>;
        using node_type = MapNodeHandle<Key, T>;
        static constexpr bool multi = Multi;
        static constexpr bool const_iterators = false;

        template<class P>
        static const auto& key(const P& p) noexcept {
            return p.first;
        }
    };

    template<class Key, class Compare, bool Multi>
    struct SetTraits {
        using key_type = Key;
        using value_type = Key;
        using key_compare = Compare;
        using value_compare = Compare;
        using node_type = SetNodeHandle<Key>;
        static constexpr bool multi = Multi;
        static constexpr bool const_iterators = true;

        template<class K>
        static const K& key(const K& k) noexcept {
            return k;
        }
    };

    template<class Traits>
    class RbTree {
    public:
        using key_type = typename Traits::key_type;
        using value_type = typename Traits::value_type;
        using key_compare = typename Traits::key_compare;
        using value_compare = typename Traits::value_compare;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using const_iterator = RbIterator<value_type, true>;
        using iterator = std::conditional_t<Traits::const_iterators, const_iterator, RbIterator<value_type, false>>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using node_type = typename Traits::node_type;

        static constexpr bool Multi = Traits::multi;

    protected:
        using NodeBase = RbNodeBase;
        using Node = RbNode<value_type>;
        using Ptr = RbPtr;
        using insert_return_type = InsertReturn<iterator, node_type>;
        using insert_result = std::conditional_t<Multi, iterator, std::pair<iterator, bool>>;
        using insert_result_node = std::conditional_t<Multi, iterator, insert_return_type>;

        // Where a new node goes: under parent, as its left or right child;
        // or nothing, because existing has an equivalent key (unique trees).
        struct InsertPos {
            NodeBase* parent = nullptr;
            bool left = false;
            NodeBase* existing = nullptr;
        };

    public:
        RbTree() noexcept(std::is_nothrow_default_constructible_v<key_compare>)
        : _comp() {
        }

        explicit RbTree(const key_compare& comp)
        : _comp(comp) {
        }

        template<std::input_iterator InputIt>
        RbTree(InputIt first, InputIt last, const key_compare& comp = key_compare())
        : _comp(comp) {
            _guarded_insert(first, last);
        }

        RbTree(std::initializer_list<value_type> ilist, const key_compare& comp = key_compare())
        : RbTree(ilist.begin(), ilist.end(), comp) {
        }

        RbTree(const RbTree& other)
        : _comp(other._comp) {
            _guarded_insert(other.begin(), other.end());
        }

        RbTree(RbTree&& other) noexcept(std::is_nothrow_move_constructible_v<key_compare>)
        : _header(other._header)
        , _size(other._size)
        , _comp(std::move(other._comp)) {
            other._header = nullptr;
            other._size = 0;
        }

        ~RbTree() {
            if (!sweeping) {
                clear();
            }
        }

        RbTree& operator=(const RbTree& other) {
            if (this != &other) {
                key_compare comp = other._comp;
                clear();
                _comp = std::move(comp);
                insert(other.begin(), other.end());
            }
            return *this;
        }

        RbTree& operator=(RbTree&& other) noexcept(std::is_nothrow_move_assignable_v<key_compare>) {
            if (this != &other) {
                clear();
                _header = other._header;
                _size = other._size;
                _comp = std::move(other._comp);
                other._header = nullptr;
                other._size = 0;
            }
            return *this;
        }

        RbTree& operator=(std::initializer_list<value_type> ilist) {
            clear();
            insert(ilist.begin(), ilist.end());
            return *this;
        }

        key_compare key_comp() const {
            return _comp;
        }

        value_compare value_comp() const {
            return value_compare(_comp);
        }

        iterator begin() noexcept {
            return iterator(_leftmost());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_leftmost());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            return iterator(_hdr());
        }

        const_iterator end() const noexcept {
            return const_iterator(_hdr());
        }

        const_iterator cend() const noexcept {
            return end();
        }

        reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        }

        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        }

        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        const_reverse_iterator crend() const noexcept {
            return rend();
        }

        [[nodiscard]] bool empty() const noexcept {
            return _size == 0;
        }

        size_type size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        void clear() noexcept {
            if (_header) {
                _destroy_subtree(_root());
                NodeBase* h = _hdr();
                h->parent = nullptr;
                h->left = _header;
                h->right = _header;
                _size = 0;
            }
        }

        insert_result insert(const value_type& value) {
            return _insert(value);
        }

        insert_result insert(value_type&& value) {
            return _insert(std::move(value));
        }

        iterator insert(const_iterator hint, const value_type& value) {
            return _insert_hint(hint._node, value);
        }

        iterator insert(const_iterator hint, value_type&& value) {
            return _insert_hint(hint._node, std::move(value));
        }

        template<std::input_iterator InputIt>
        void insert(InputIt first, InputIt last) {
            if (first == last) {
                return;
            }
            _ensure_header();
            for (; first != last; ++first) {
                _insert_hint(_hdr(), *first);
            }
        }

        void insert(std::initializer_list<value_type> ilist) {
            insert(ilist.begin(), ilist.end());
        }

        // Unique trees: {position, inserted, node}; the handle keeps the
        // node when an equivalent key is already there.
        insert_result_node insert(node_type&& nh) {
            if constexpr (Multi) {
                if (nh.empty()) {
                    return end();
                }
                _ensure_header();
                NodeBase* n = nh._node.get();
                InsertPos pos = _equal_pos(_key(n));
                _link(pos, n);
                nh._node = nullptr;
                return iterator(n);
            } else {
                insert_return_type result{end(), false, node_type()};
                if (nh.empty()) {
                    return result;
                }
                _ensure_header();
                NodeBase* n = nh._node.get();
                InsertPos pos = _unique_pos(_key(n));
                if (pos.existing) {
                    result.position = iterator(pos.existing);
                    result.node = std::move(nh);
                    return result;
                }
                _link(pos, n);
                nh._node = nullptr;
                result.position = iterator(n);
                result.inserted = true;
                return result;
            }
        }

        iterator insert(const_iterator hint, node_type&& nh) {
            if (nh.empty()) {
                return end();
            }
            _ensure_header();
            NodeBase* n = nh._node.get();
            InsertPos pos = _hint_pos(hint._node, _key(n));
            if (pos.existing) {
                return iterator(pos.existing);
            }
            _link(pos, n);
            nh._node = nullptr;
            return iterator(n);
        }

        // The element is built before its place is known, as in std; a
        // comparator that throws destroys it again.
        template<class... A>
        insert_result emplace(A&&... a) {
            _ensure_header();
            Ptr n = make_tracked<Node>();
            Node* node = _node(n.get());
            node->slot.construct(std::forward<A>(a)...);
            InsertPos pos;
            try {
                pos = _pos(_key(n.get()));
            }
            catch (...) {
                node->slot.destroy();
                throw;
            }
            if constexpr (!Multi) {
                if (pos.existing) {
                    node->slot.destroy();
                    return {iterator(pos.existing), false};
                }
            }
            _link(pos, n.get());
            return _result(iterator(n.get()), true);
        }

        template<class... A>
        iterator emplace_hint(const_iterator hint, A&&... a) {
            _ensure_header();
            Ptr n = make_tracked<Node>();
            Node* node = _node(n.get());
            node->slot.construct(std::forward<A>(a)...);
            InsertPos pos;
            try {
                pos = _hint_pos(hint._node, _key(n.get()));
            }
            catch (...) {
                node->slot.destroy();
                throw;
            }
            if (pos.existing) {
                node->slot.destroy();
                return iterator(pos.existing);
            }
            _link(pos, n.get());
            return iterator(n.get());
        }

        // The node is held by _erase_node while it is unlinked and cleared;
        // next stays linked, so the tree roots it.
        iterator erase(const_iterator pos) {
            NodeBase* n = pos._node;
            NodeBase* next = rb_increment(n);
            _erase_node(n);
            return iterator(next);
        }

        iterator erase(iterator pos) requires (!std::is_same_v<iterator, const_iterator>) {
            return erase(const_iterator(pos));
        }

        iterator erase(const_iterator first, const_iterator last) {
            if (first == begin() && last == end()) {
                clear();
            } else {
                while (first != last) {
                    first = erase(first);
                }
            }
            return iterator(last._node);
        }

        size_type erase(const key_type& key) {
            if (!_header) {
                return 0;
            }
            if constexpr (Multi) {
                auto [first, last] = _equal_range(key);
                size_type old_size = _size;
                erase(const_iterator(first), const_iterator(last));
                return old_size - _size;
            } else {
                NodeBase* n = _find(key);
                if (n == _hdr()) {
                    return 0;
                }
                _erase_node(n);
                return 1;
            }
        }

        void swap(RbTree& other) noexcept(std::is_nothrow_swappable_v<key_compare>) {
            _header.swap(other._header);
            std::swap(_size, other._size);
            using std::swap;
            swap(_comp, other._comp);
        }

        node_type extract(const_iterator pos) {
            NodeBase* n = pos._node;
            tracked_ptr<NodeBase> keep(n);   // rooted from the unlinking until the handle holds it
            rb_rebalance_for_erase(n, _hdr());
            _unlink(n);
            --_size;
            return node_type(n);
        }

        node_type extract(iterator pos) requires (!std::is_same_v<iterator, const_iterator>) {
            return extract(const_iterator(pos));
        }

        node_type extract(const key_type& key) {
            const_iterator it = find(key);
            if (it == end()) {
                return node_type();
            }
            return extract(it);
        }

        // Relinks the source's nodes whose keys fit (all of them, for a
        // multi tree); the rest stay in the source.
        template<class Traits2>
        void merge(RbTree<Traits2>& source) requires (std::is_same_v<typename Traits2::key_type, key_type> && std::is_same_v<typename Traits2::value_type, value_type>) {
            if constexpr (std::is_same_v<Traits2, Traits>) {
                if (this == &source) {
                    return;
                }
            }
            if (source.empty()) {
                return;
            }
            _ensure_header();
            Ptr hold;   // roots the node between the trees; next stays linked in the source
            NodeBase* n = source._leftmost();
            while (n != source._hdr()) {
                NodeBase* next = rb_increment(n);
                InsertPos pos = _pos(_key(n));
                if (!pos.existing) {
                    hold = Ptr(n);
                    rb_rebalance_for_erase(n, source._hdr());
                    --source._size;
                    _link(pos, n);
                }
                n = next;
            }
        }

        template<class Traits2>
        void merge(RbTree<Traits2>&& source) requires (std::is_same_v<typename Traits2::key_type, key_type> && std::is_same_v<typename Traits2::value_type, value_type>) {
            merge(source);
        }

        size_type count(const key_type& key) const {
            return _count(key);
        }

        template<class K> requires TransparentCompare<key_compare>
        size_type count(const K& key) const {
            return _count(key);
        }

        iterator find(const key_type& key) {
            return iterator(_find(key));
        }

        const_iterator find(const key_type& key) const {
            return const_iterator(_find(key));
        }

        template<class K> requires TransparentCompare<key_compare>
        iterator find(const K& key) {
            return iterator(_find(key));
        }

        template<class K> requires TransparentCompare<key_compare>
        const_iterator find(const K& key) const {
            return const_iterator(_find(key));
        }

        bool contains(const key_type& key) const {
            return _header && _find(key) != _hdr();
        }

        template<class K> requires TransparentCompare<key_compare>
        bool contains(const K& key) const {
            return _header && _find(key) != _hdr();
        }

        std::pair<iterator, iterator> equal_range(const key_type& key) {
            auto [first, last] = _equal_range(key);
            return {iterator(first), iterator(last)};
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type& key) const {
            auto [first, last] = _equal_range(key);
            return {const_iterator(first), const_iterator(last)};
        }

        template<class K> requires TransparentCompare<key_compare>
        std::pair<iterator, iterator> equal_range(const K& key) {
            auto [first, last] = _equal_range(key);
            return {iterator(first), iterator(last)};
        }

        template<class K> requires TransparentCompare<key_compare>
        std::pair<const_iterator, const_iterator> equal_range(const K& key) const {
            auto [first, last] = _equal_range(key);
            return {const_iterator(first), const_iterator(last)};
        }

        iterator lower_bound(const key_type& key) {
            return iterator(_lower_bound(key));
        }

        const_iterator lower_bound(const key_type& key) const {
            return const_iterator(_lower_bound(key));
        }

        template<class K> requires TransparentCompare<key_compare>
        iterator lower_bound(const K& key) {
            return iterator(_lower_bound(key));
        }

        template<class K> requires TransparentCompare<key_compare>
        const_iterator lower_bound(const K& key) const {
            return const_iterator(_lower_bound(key));
        }

        iterator upper_bound(const key_type& key) {
            return iterator(_upper_bound(key));
        }

        const_iterator upper_bound(const key_type& key) const {
            return const_iterator(_upper_bound(key));
        }

        template<class K> requires TransparentCompare<key_compare>
        iterator upper_bound(const K& key) {
            return iterator(_upper_bound(key));
        }

        template<class K> requires TransparentCompare<key_compare>
        const_iterator upper_bound(const K& key) const {
            return const_iterator(_upper_bound(key));
        }

        // The red-black invariants, the header links, the parent links, the
        // order and the size; for the tests.
        bool _check() const {
            if (!_header) {
                return _size == 0;
            }
            NodeBase* h = _hdr();
            NodeBase* root = _root();
            if (!h->red) {
                return false;
            }
            if (!root) {
                return _size == 0 && h->left.get() == h && h->right.get() == h;
            }
            if (root->red || root->parent.get() != h) {
                return false;
            }
            if (h->left.get() != rb_minimum(root) || h->right.get() != rb_maximum(root)) {
                return false;
            }
            int expected = -1;
            size_t count = 0;
            if (!_check_subtree(root, 0, expected, count) || count != _size) {
                return false;
            }
            NodeBase* prev = nullptr;
            size_t walked = 0;
            for (NodeBase* n = h->left.get(); n != h; n = rb_increment(n)) {
                ++walked;
                if (prev) {
                    if (_comp(_key(n), _key(prev))) {
                        return false;
                    }
                    if constexpr (!Multi) {
                        if (!_comp(_key(prev), _key(n))) {
                            return false;
                        }
                    }
                    if (rb_decrement(n) != prev) {
                        return false;
                    }
                }
                prev = n;
            }
            return walked == _size && prev == h->right.get() && rb_decrement(h) == prev;
        }

    protected:
        tracked_ptr<NodeBase> _header;   // the root of the container
        size_t _size = 0;
        [[no_unique_address]] key_compare _comp;

        NodeBase* _hdr() const noexcept {
            return _header.get();
        }

        NodeBase* _root() const noexcept {
            return _hdr()->parent.get();
        }

        NodeBase* _leftmost() const noexcept {
            return _header ? _hdr()->left.get() : nullptr;
        }

        NodeBase* _rightmost() const noexcept {
            return _header ? _hdr()->right.get() : nullptr;
        }

        static Node* _node(NodeBase* n) noexcept {
            return static_cast<Node*>(n);
        }

        // The node an iterator stands on, and the key of a node
        static NodeBase* _raw(const const_iterator& it) noexcept {
            return it._node;
        }

        static const key_type& _key(NodeBase* n) noexcept {
            return Traits::key(_node(n)->slot.value);
        }

        // The header node, made on the first insertion (an empty tree
        // holds nothing): red, so that it is never taken for a black node
        // by the rebalancing, its extremes itself
        void _ensure_header() {
            if (!_header) {
                _header = make_tracked<NodeBase>();
                NodeBase* h = _hdr();
                h->red = true;
                h->left = _header;
                h->right = _header;
            }
        }

        // What insert returns: the iterator alone (a multi tree), or with
        // whether the node was inserted
        static insert_result _result(iterator it, bool inserted) noexcept {
            if constexpr (Multi) {
                return it;
            } else {
                return {it, inserted};
            }
        }

        // A range inserted into a fresh tree; the tree cleared if any
        // insertion throws (the constructors from a range)
        template<std::input_iterator InputIt>
        void _guarded_insert(InputIt first, InputIt last) {
            try {
                insert(first, last);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        // Recursion on the right subtree only: bounded by the height. The
        // tree stays linked from the header while it runs, so every node
        // is rooted until its turn.
        static void _destroy_subtree(NodeBase* first) noexcept {
            if (!first) {
                return;
            }
            Anchor keep(first);   // rooted here once its parent lets go
            auto x = first;
            while (x) {
                _destroy_subtree(x->right.get());
                auto y = x->left.get();
                _unlink(x);
                _node(x)->slot.destroy();
                keep.reset(y);   // y was rooted by x until here
                x = y;
            }
        }

        // The links of a node taken out nulled: a dead node holds nothing
        static void _unlink(NodeBase* n) noexcept {
            n->parent = nullptr;
            n->left = nullptr;
            n->right = nullptr;
        }

        // One node out of the tree and its element destroyed; the node
        // itself is the collector's
        void _erase_node(NodeBase* n) noexcept {
            Anchor keep(n);   // rooted while unlinked and cleared
            rb_rebalance_for_erase(n, _hdr());
            _unlink(n);
            _node(n)->slot.destroy();
            --_size;
        }

        // A new node at its position
        void _link(const InsertPos& pos, NodeBase* n) noexcept {
            rb_insert_and_rebalance(pos.left, n, pos.parent, _hdr());
            ++_size;
        }

        // A new node for the value built from a, linked at pos.
        template<class... A>
        iterator _insert_at(const InsertPos& pos, A&&... a) {
            auto n = make_tracked<Node>();   // rooted by its state (UniqueLock) and by this frame until it is linked
            _node(n.get())->slot.construct(std::forward<A>(a)...);
            // shared from here: released from the unique state before the
            // links to the node are stored (reset() refuses a unique object)
            Page::set_state_released(n.get());
            _link(pos, n.get());
            return iterator(n.release());
        }

        // The insertions behind insert and emplace: the position found for
        // the key (with the hint tried first, for the hinted forms), a node
        // made and linked unless an equivalent key exists in a unique tree
        template<class Arg>
        insert_result _insert(Arg&& value) {
            _ensure_header();
            InsertPos pos = _pos(Traits::key(value));
            if constexpr (!Multi) {
                if (pos.existing) {
                    return {iterator(pos.existing), false};
                }
            }
            return _result(_insert_at(pos, std::forward<Arg>(value)), true);
        }

        template<class Arg>
        iterator _insert_hint(NodeBase* hint, Arg&& value) {
            _ensure_header();
            InsertPos pos = _hint_pos(hint, Traits::key(value));
            if (pos.existing) {
                return iterator(pos.existing);
            }
            return _insert_at(pos, std::forward<Arg>(value));
        }

        template<class K>
        InsertPos _pos(const K& key) const {
            if constexpr (Multi) {
                return _equal_pos(key);
            } else {
                return _unique_pos(key);
            }
        }

        template<class K>
        InsertPos _hint_pos(NodeBase* hint, const K& key) const {
            if constexpr (Multi) {
                return _equal_hint_pos(hint, key);
            } else {
                return _unique_hint_pos(hint, key);
            }
        }

        // The positions, as the standard library's tree finds them: for a
        // unique key the node with an equivalent key if there is one, else
        // the leaf to hang the new node from; for a multi tree after the
        // last equivalent key (_equal_lower_pos: before the first); with a
        // hint, the hint's neighbourhood checked before a search
        template<class K>
        InsertPos _unique_pos(const K& key) const {
            NodeBase* x = _root();
            NodeBase* y = _hdr();
            bool comp = true;
            while (x) {
                y = x;
                auto l = x->left.get();   // both children before the compare: a select, not a branch
                auto r = x->right.get();
                comp = _comp(key, _key(x));
                x = comp ? l : r;
            }
            NodeBase* j = y;
            if (comp) {
                if (j == _hdr()->left.get()) {
                    return {y, true, nullptr};
                }
                j = rb_decrement(j);
            }
            if (_comp(_key(j), key)) {
                return {y, comp, nullptr};
            }
            return {nullptr, false, j};
        }

        // Equivalent keys go after the ones already there.
        template<class K>
        InsertPos _equal_pos(const K& key) const {
            NodeBase* x = _root();
            NodeBase* y = _hdr();
            bool left = true;
            while (x) {
                y = x;
                auto l = x->left.get();
                auto r = x->right.get();
                left = _comp(key, _key(x));
                x = left ? l : r;
            }
            return {y, left, nullptr};
        }

        template<class K>
        InsertPos _equal_lower_pos(const K& key) const {
            NodeBase* x = _root();
            NodeBase* y = _hdr();
            bool left = true;
            while (x) {
                y = x;
                auto l = x->left.get();
                auto r = x->right.get();
                left = !_comp(_key(x), key);
                x = left ? l : r;
            }
            return {y, left, nullptr};
        }

        // A hint is used when the key belongs right before it; an append
        // in sorted order at end() costs one comparison.
        template<class K>
        InsertPos _unique_hint_pos(NodeBase* pos, const K& key) const {
            if (!pos || pos == _hdr()) {
                if (_size > 0 && _comp(_key(_rightmost()), key)) {
                    return {_rightmost(), false, nullptr};
                }
                return _unique_pos(key);
            } else if (_comp(key, _key(pos))) {
                if (pos == _leftmost()) {
                    return {pos, true, nullptr};
                }
                NodeBase* before = rb_decrement(pos);
                if (_comp(_key(before), key)) {
                    if (!before->right) {
                        return {before, false, nullptr};
                    }
                    return {pos, true, nullptr};
                }
                return _unique_pos(key);
            } else if (_comp(_key(pos), key)) {
                if (pos == _rightmost()) {
                    return {pos, false, nullptr};
                }
                NodeBase* after = rb_increment(pos);
                if (_comp(key, _key(after))) {
                    if (!pos->right) {
                        return {pos, false, nullptr};
                    }
                    return {after, true, nullptr};
                }
                return _unique_pos(key);
            }
            return {nullptr, false, pos};
        }

        template<class K>
        InsertPos _equal_hint_pos(NodeBase* pos, const K& key) const {
            if (!pos || pos == _hdr()) {
                if (_size > 0 && !_comp(key, _key(_rightmost()))) {
                    return {_rightmost(), false, nullptr};
                }
                return _equal_pos(key);
            } else if (!_comp(_key(pos), key)) {
                if (pos == _leftmost()) {
                    return {pos, true, nullptr};
                }
                NodeBase* before = rb_decrement(pos);
                if (!_comp(key, _key(before))) {
                    if (!before->right) {
                        return {before, false, nullptr};
                    }
                    return {pos, true, nullptr};
                }
                return _equal_pos(key);
            } else {
                if (pos == _rightmost()) {
                    return {pos, false, nullptr};
                }
                NodeBase* after = rb_increment(pos);
                if (!_comp(_key(after), key)) {
                    if (!pos->right) {
                        return {pos, false, nullptr};
                    }
                    return {after, true, nullptr};
                }
                return _equal_lower_pos(key);
            }
        }

        // The searches: the first node not less than the key, the first
        // greater, the node equal to it, the range of the equivalent ones,
        // their count; from a subtree x with y the answer so far
        template<class K>
        NodeBase* _lower_bound_from(NodeBase* x, NodeBase* y, const K& key) const {
            while (x) {
                auto l = x->left.get();
                auto r = x->right.get();
                bool ge = !_comp(_key(x), key);
                y = ge ? x : y;
                x = ge ? l : r;
            }
            return y;
        }

        template<class K>
        NodeBase* _upper_bound_from(NodeBase* x, NodeBase* y, const K& key) const {
            while (x) {
                auto l = x->left.get();
                auto r = x->right.get();
                bool lt = _comp(key, _key(x));
                y = lt ? x : y;
                x = lt ? l : r;
            }
            return y;
        }

        template<class K>
        NodeBase* _lower_bound(const K& key) const {
            if (!_header) {
                return nullptr;
            }
            return _lower_bound_from(_root(), _hdr(), key);
        }

        template<class K>
        NodeBase* _upper_bound(const K& key) const {
            if (!_header) {
                return nullptr;
            }
            return _upper_bound_from(_root(), _hdr(), key);
        }

        template<class K>
        NodeBase* _find(const K& key) const {
            if (!_header) {
                return nullptr;
            }
            auto h = _hdr();
            NodeBase* j = _lower_bound_from(h->parent.get(), h, key);
            return (j == h || _comp(key, _key(j))) ? h : j;
        }

        template<class K>
        std::pair<NodeBase*, NodeBase*> _equal_range(const K& key) const {
            if (!_header) {
                return {nullptr, nullptr};
            }
            NodeBase* x = _root();
            NodeBase* y = _hdr();
            while (x) {
                if (_comp(_key(x), key)) {
                    x = x->right.get();
                } else if (_comp(key, _key(x))) {
                    y = x;
                    x = x->left.get();
                } else {
                    if constexpr (!Multi) {
                        return {x, rb_increment(x)};
                    } else {
                        NodeBase* xu = x->right.get();
                        NodeBase* yu = y;
                        y = x;
                        x = x->left.get();
                        return {_lower_bound_from(x, y, key), _upper_bound_from(xu, yu, key)};
                    }
                }
            }
            return {y, y};
        }

        template<class K>
        size_type _count(const K& key) const {
            if constexpr (Multi) {
                auto [first, last] = _equal_range(key);
                size_type n = 0;
                for (; first != last; first = rb_increment(first)) {
                    ++n;
                }
                return n;
            } else {
                return _header && _find(key) != _hdr() ? 1 : 0;
            }
        }

        // The invariants of a subtree (the tests: verify): no red node with
        // a red child, the same black height on every path, the count
        static bool _check_subtree(NodeBase* x, int black, int& expected, size_t& count) {
            if (!x) {
                if (expected < 0) {
                    expected = black;
                }
                return expected == black;
            }
            ++count;
            if (x->red) {
                if ((x->left && x->left->red) || (x->right && x->right->red)) {
                    return false;
                }
            } else {
                ++black;
            }
            if (x->left && x->left->parent.get() != x) {
                return false;
            }
            if (x->right && x->right->parent.get() != x) {
                return false;
            }
            return _check_subtree(x->left.get(), black, expected, count) && _check_subtree(x->right.get(), black, expected, count);
        }

        friend bool operator==(const RbTree& lhs, const RbTree& rhs) {
            return lhs._size == rhs._size && std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }

        friend auto operator<=>(const RbTree& lhs, const RbTree& rhs) {
            return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), synth_three_way);
        }

        template<class> friend class RbTree;
    };
}
