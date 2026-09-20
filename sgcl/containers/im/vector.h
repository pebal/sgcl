//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/make_tracked.h"
#include "../../core/mixin/mixin.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace sgcl::im {
    namespace detail {
        // A branch of the trie of vector: 32 children, each a
        // branch or, at the lowest level, a leaf (VectorLeaf); which of the
        // two is known from the level, so the children are pointers to
        // void. Never modified once a vector holds it: a change is a copy
        // with one child replaced, and every other child is shared with
        // the branch it was copied from. A type of its own rather than an
        // array<tracked_ptr<void>, 32>, on purpose: the pool is per type,
        // so a page holds branches and nothing else, and the branches a
        // walk touches lie together.
        //
        // Made by make() only (the constructors are private), so that a
        // branch is always a managed object and the copy may shade its
        // source: the copy takes the words without the barrier
        // (tracked_ptr::store with detail::unshaded), then makes the
        // source reachable in this cycle through a tracked_ptr to it,
        // whose construction is the barrier. The source never changes,
        // so the marking, visiting it, marks every child the copy holds:
        // one barrier for the node in place of 32. Sound while the source
        // is held through the copy, which the version being copied does.
        struct VectorBranch {
            tracked_ptr<void> children[32];

            static unique_ptr<VectorBranch> make() {
                return make_tracked<VectorBranch>();
            }

            // The copy: its words taken without the barrier, then, the
            // copy complete, the source shaded
            static unique_ptr<VectorBranch> make(const VectorBranch& from) {
                unique_ptr<VectorBranch> copy = make_tracked<VectorBranch>(from);
                ((tracked_ptr<const VectorBranch>)&from).shade();
                return copy;
            }

        private:
            friend class sgcl::detail::MakerBase;

            VectorBranch() noexcept = default;

            VectorBranch(const VectorBranch& o) noexcept {
                for (size_t i = 0; i < 32; ++i) {
                    children[i].store(o.children[i], sgcl::detail::unshaded);
                }
            }
        };

        // A leaf: up to 32 elements, `count` of them constructed, always
        // the first ones. A leaf inside the trie is full; the tail (the
        // last leaf, held by the vector itself) is the one that may not
        // be. A leaf of a trivial type is a trivial object: the collector
        // has no map to build for it and no destructor to run. Otherwise
        // the elements live in a union, constructed one by one as a leaf
        // is filled and destroyed by the destructor, which the collector
        // runs when no version reaches the leaf any more: an element
        // holding tracked pointers is traced through the leaf's map, and
        // the unconstructed slots hold null pointers only (a fresh page is
        // zero, a destroyed tracked_ptr leaves null).
        template<class T, bool Trivial = std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>>
        struct VectorLeaf {
            uint32_t count;
            T values[32];
        };

        template<class T>
        struct VectorLeaf<T, false> {
            uint32_t count = 0;
            union {
                T values[32];
            };

            VectorLeaf() noexcept {
            }

            VectorLeaf(const VectorLeaf&) = delete;
            VectorLeaf& operator=(const VectorLeaf&) = delete;

            ~VectorLeaf() {
                for (auto i = count; i > 0; --i) {
                    values[i - 1].~T();
                }
            }
        };
    }

    // The immutable vector (the persistent vector of Clojure and Scala):
    // a sequence every operation of which returns a new vector and leaves
    // the old one as it was, the two sharing everything but the path that changed. A trie of
    // 32-way nodes indexed by five bits of the position per level, and a
    // tail of up to 32 elements held apart from the trie: an element is
    // read by walking log32(n) branches (three for a million elements,
    // none for the last 32), `set` copies the branches on that path and
    // the leaf, five objects at most, `push_back` copies the tail and,
    // once in 32 pushes, the path a full tail is hung on, `pop_back` the
    // reverse. Nothing is ever modified: a vector held by any number of
    // threads is read by all of them without a lock, and a version is
    // published, and replaced by the next, through a copy_on_write or an
    // atomic; a state of a program is such a vector, and the next state a
    // new one, the two compared by their roots (README: The im module).
    //
    // The vector is four words: the size, the height of the trie, a
    // tracked_ptr to the root and one to the tail. It lives where a
    // tracked_ptr may, on a stack or inside a managed object, and a copy
    // of it is a copy of those words. The branches and the leaves are
    // managed objects that no version owns: a leaf reached by ten
    // versions is one leaf, and it is collected once the last of them is
    // dropped, which is the question a persistent structure without a
    // collector answers with a reference count per node and this one
    // does not have to answer at all. Elements are const through the
    // vector; a T holding tracked pointers is traced where it lives, in
    // a leaf. An element's copy constructor is what a change costs: the
    // 32 elements of a leaf at most, plus the branches.
    template<class T>
    class vector   // read as any range; == its own (a version and its copy by the trie); no mixin::sequence: nothing written in place
    : public mixin::enumerable<vector<T>>
    , public mixin::random_access<vector<T>>
    , public mixin::bidirectional<vector<T>>
    , public mixin::comparable<vector<T>>
    , public mixin::ordered<vector<T>> {
        using Branch = detail::VectorBranch;
        using Leaf = detail::VectorLeaf<T>;

        static constexpr unsigned Bits = 5;         // the bits of the position one level consumes
        static constexpr size_t Width = 1 << Bits;  // the children of a branch, the elements of a leaf
        static constexpr size_t Mask = Width - 1;

    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using pointer = const T*;
        using const_pointer = const T*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        // A random-access iterator: the position and the vector it is in,
        // with the leaf of the position remembered so that a walk costs
        // the trie once in 32 elements. It keeps nothing alive: valid
        // while the vector object it was taken from exists (a version held
        // elsewhere is another object), as an iterator of std is.
        class const_iterator {
        public:
            using iterator_concept = std::random_access_iterator_tag;
            using iterator_category = std::random_access_iterator_tag;
            using value_type = T;
            using reference = const T&;
            using pointer = const T*;
            using difference_type = ptrdiff_t;

            const_iterator() noexcept = default;
            const_iterator(const const_iterator&) noexcept = default;
            const_iterator& operator=(const const_iterator&) noexcept = default;

            // A dead iterator keeps no word that the conservative scan of
            // the stack could take for a root
            ~const_iterator() noexcept {   // volatile, as tracked_ptr's: a store the compiler may not drop as dead
                *(void* volatile*)&_vector = nullptr;
                *(void* volatile*)&_leaf = nullptr;
            }

            reference operator*() const noexcept {
                if (_index - _first >= _count) {
                    _vector->_leaf_of(_index, _leaf, _first, _count);
                }
                return _leaf[_index - _first];
            }

            pointer operator->() const noexcept {
                return std::addressof(**this);
            }

            reference operator[](difference_type n) const noexcept {
                return *(*this + n);
            }

            const_iterator& operator++() noexcept {
                ++_index;
                return *this;
            }

            const_iterator operator++(int) noexcept {
                auto tmp = *this;
                ++_index;
                return tmp;
            }

            const_iterator& operator--() noexcept {
                --_index;
                return *this;
            }

            const_iterator operator--(int) noexcept {
                auto tmp = *this;
                --_index;
                return tmp;
            }

            const_iterator& operator+=(difference_type n) noexcept {
                _index += n;
                return *this;
            }

            const_iterator& operator-=(difference_type n) noexcept {
                _index -= n;
                return *this;
            }

            friend const_iterator operator+(const_iterator it, difference_type n) noexcept {
                it._index += n;
                return it;
            }

            friend const_iterator operator+(difference_type n, const_iterator it) noexcept {
                it._index += n;
                return it;
            }

            friend const_iterator operator-(const_iterator it, difference_type n) noexcept {
                it._index -= n;
                return it;
            }

            friend difference_type operator-(const const_iterator& a, const const_iterator& b) noexcept {
                return difference_type(a._index) - difference_type(b._index);
            }

            friend bool operator==(const const_iterator& a, const const_iterator& b) noexcept {
                return a._index == b._index;
            }

            friend std::strong_ordering operator<=>(const const_iterator& a, const const_iterator& b) noexcept {
                return a._index <=> b._index;
            }

        private:
            friend class vector;

            const_iterator(const vector* vector, size_t index) noexcept
            : _vector(vector)
            , _index(index) {
            }

            const vector* _vector = nullptr;
            size_t _index = 0;
            mutable const T* _leaf = nullptr;   // the elements of the leaf holding _first, when looked up
            mutable size_t _first = 0;          // the position the leaf begins at
            mutable size_t _count = 0;          // its elements: 0 until looked up
        };

        using iterator = const_iterator;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using reverse_iterator = const_reverse_iterator;

        vector() noexcept = default;

        template<std::input_iterator InputIt>
        vector(InputIt first, InputIt last) {
            for (; first != last; ++first) {
                auto count = _tail_count();
                if (count == Width) {   // the tail full: into the trie, a new one begun
                    _absorb_tail();
                    count = 0;
                }
                if (count == 0) {
                    _tail = _make_leaf();
                }
                _append(*_tail, *first);   // in place: nobody holds this vector yet
                ++_size;
            }
        }

        vector(std::initializer_list<T> ilist)
        : vector(ilist.begin(), ilist.end()) {
        }

        vector(const vector&) noexcept = default;
        vector(vector&&) noexcept = default;
        vector& operator=(const vector&) noexcept = default;
        vector& operator=(vector&&) noexcept = default;

        // The elements
        const_reference operator[](size_type i) const noexcept {
            assert(i < _size);
            return _at(i);
        }

        const_reference at(size_type i) const {
            if (i >= _size) {
                throw std::out_of_range("sgcl::vector::at");
            }
            return _at(i);
        }

        const_reference front() const noexcept {
            assert(_size > 0);
            return _at(0);
        }

        const_reference back() const noexcept {
            assert(_size > 0);
            return _tail->values[_tail_count() - 1];
        }

        const_iterator begin() const noexcept {
            return const_iterator(this, 0);
        }

        const_iterator end() const noexcept {
            return const_iterator(this, _size);
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        }

        const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        }

        const_reverse_iterator crbegin() const noexcept {
            return rbegin();
        }

        const_reverse_iterator crend() const noexcept {
            return rend();
        }

        // The size
        size_type size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return _size == 0;
        }

        // The height of the trie: how many branches a random access
        // walks, for the curious and the tests
        unsigned depth() const noexcept {
            return _root ? unsigned(_shift / Bits) : 0;
        }

        // The vector with `value` after its last element: the tail copied
        // with one more element (a full tail is first hung on the trie as
        // it is, along a copied path, and the new tail holds `value` alone)
        vector push_back(const T& value) const {
            return _push_back(value);
        }

        vector push_back(T&& value) const {
            return _push_back(std::move(value));
        }

        template<class... A>
        vector emplace_back(A&&... a) const {
            return _push_back(std::forward<A>(a)...);
        }

        // The vector without its last element: the tail copied one element
        // shorter, or, when the tail held one element, the trie's last
        // leaf taken out along a copied path to be the tail
        vector pop_back() const {
            assert(_size > 0);
            if (_size == 1) {
                return vector();
            }
            auto count = _tail_count();
            if (count > 1) {
                return vector(_size - 1, _shift, _root, _copy_leaf(*_tail, count - 1));
            }
            auto last = _size - 2;   // the new last element, in the trie's last leaf
            tracked_ptr<Leaf> tail = static_pointer_cast<Leaf>(_leaf_link(last));
            if (last < Width) {      // that leaf was the trie's only one: the trie is empty now
                return vector(_size - 1, 0, nullptr, tail);
            }
            unique_ptr<Branch> root = _pop_leaf(_shift, *_root, last);
            if (_shift > Bits && !root->children[1]) {   // one child left: the trie loses a level
                return vector(_size - 1, _shift - Bits, static_pointer_cast<Branch>(root->children[0]), tail);
            }
            return vector(_size - 1, _shift, std::move(root), tail);
        }

        // The vector with the element at `i` replaced: the path to its leaf
        // copied, log32(n) branches and the leaf, everything else shared
        vector set(size_type i, const T& value) const {
            return _set(i, value);
        }

        vector set(size_type i, T&& value) const {
            return _set(i, std::move(value));
        }

        friend bool operator==(const vector& a, const vector& b) {
            if (a._root == b._root && a._tail == b._tail && a._size == b._size) {   // the same trie and tail: a version and its copy
                return true;
            }
            return a._size == b._size && std::equal(a.begin(), a.end(), b.begin());
        }

        friend bool operator!=(const vector& a, const vector& b) {
            return !(a == b);
        }

    private:
        vector(size_t size, size_t shift, tracked_ptr<Branch> root, tracked_ptr<Leaf> tail) noexcept
        : _size(size)
        , _shift(shift)
        , _root(std::move(root))
        , _tail(std::move(tail)) {
        }

        // Where the tail begins: every element from there on is in it
        size_t _tail_offset() const noexcept {
            return _size < Width ? 0 : ((_size - 1) >> Bits) << Bits;
        }

        uint32_t _tail_count() const noexcept {
            return uint32_t(_size - _tail_offset());
        }

        const T& _at(size_t i) const noexcept {
            auto offset = _tail_offset();
            if (i >= offset) {
                return _tail->values[i - offset];
            }
            return static_cast<const Leaf*>(_leaf_link(i).get())->values[i & Mask];
        }

        // The link to the leaf holding position i, in the trie (i below
        // the tail): the branches walked from the root, five bits a level
        const tracked_ptr<void>& _leaf_link(size_t i) const noexcept {
            const Branch* node = _root.get();
            for (auto level = _shift; level > Bits; level -= Bits) {
                node = static_cast<const Branch*>(node->children[(i >> level) & Mask].get());
            }
            return node->children[(i >> Bits) & Mask];
        }

        // For the iterator: the leaf holding position i, where it begins
        // and how many elements it has
        void _leaf_of(size_t i, const T*& leaf, size_t& first, size_t& count) const noexcept {
            auto offset = _tail_offset();
            if (i >= offset) {
                leaf = _tail->values;
                first = offset;
                count = _size - offset;
            } else {
                leaf = static_cast<const Leaf*>(_leaf_link(i).get())->values;
                first = i & ~Mask;
                count = Width;
            }
        }

        // A leaf holding nothing yet; a unique_ptr while it is filled, so
        // that an exception on the way destroys what was constructed
        static unique_ptr<Leaf> _make_leaf() {
            unique_ptr<Leaf> leaf = make_tracked<Leaf>();
            leaf->count = 0;
            return leaf;
        }

        // One more element constructed in a leaf, the count raised after
        template<class... A>
        static void _append(Leaf& leaf, A&&... a) {
            ::new (static_cast<void*>(&leaf.values[leaf.count])) T(std::forward<A>(a)...);
            ++leaf.count;
        }

        // A leaf holding copies of the first n elements of `from`
        static unique_ptr<Leaf> _copy_leaf(const Leaf& from, uint32_t n) {
            unique_ptr<Leaf> leaf = _make_leaf();
            for (uint32_t i = 0; i < n; ++i) {
                _append(*leaf, from.values[i]);
            }
            return leaf;
        }

        // The same with the element at `at` replaced by `value`
        template<class U>
        static unique_ptr<Leaf> _copy_leaf(const Leaf& from, uint32_t n, uint32_t at, U&& value) {
            unique_ptr<Leaf> leaf = _make_leaf();
            for (uint32_t i = 0; i < n; ++i) {
                if (i == at) {
                    _append(*leaf, std::forward<U>(value));
                } else {
                    _append(*leaf, from.values[i]);
                }
            }
            return leaf;
        }

        // A path of branches from `level` down to the leaf, each holding
        // the next as its first child
        static tracked_ptr<void> _new_path(size_t level, const tracked_ptr<void>& leaf) {
            if (level == 0) {
                return leaf;
            }
            unique_ptr<Branch> branch = Branch::make();
            branch->children[0] = _new_path(level - Bits, leaf);
            return branch;
        }

        // The subtrie under `node` (at `level`) with `leaf` hung at
        // position `index`: the branches on the path copied, the rest shared
        static unique_ptr<Branch> _push_leaf(size_t level, const Branch& node, size_t index, const tracked_ptr<void>& leaf) {
            unique_ptr<Branch> copy = Branch::make(node);
            auto i = (index >> level) & Mask;
            if (level == Bits) {
                copy->children[i] = leaf;
            } else if (auto child = node.children[i].get()) {
                copy->children[i] = _push_leaf(level - Bits, *static_cast<const Branch*>(child), index, leaf);
            } else {
                copy->children[i] = _new_path(level - Bits, leaf);
            }
            return copy;
        }

        // The subtrie under `node` without the leaf holding position
        // `index`, its last: the path copied; null when nothing is left
        static unique_ptr<Branch> _pop_leaf(size_t level, const Branch& node, size_t index) {
            auto i = (index >> level) & Mask;
            if (level == Bits) {
                if (i == 0) {
                    return nullptr;
                }
                unique_ptr<Branch> copy = Branch::make(node);
                copy->children[i] = nullptr;
                return copy;
            }
            unique_ptr<Branch> child = _pop_leaf(level - Bits, *static_cast<const Branch*>(node.children[i].get()), index);
            if (!child && i == 0) {
                return nullptr;
            }
            unique_ptr<Branch> copy = Branch::make(node);
            copy->children[i] = std::move(child);
            return copy;
        }

        // The subtrie under `node` with the element at `index` replaced
        template<class U>
        static unique_ptr<Branch> _set_in(size_t level, const Branch& node, size_t index, U&& value) {
            unique_ptr<Branch> copy = Branch::make(node);
            auto i = (index >> level) & Mask;
            if (level == Bits) {
                auto& leaf = *static_cast<const Leaf*>(node.children[i].get());
                copy->children[i] = _copy_leaf(leaf, leaf.count, uint32_t(index & Mask), std::forward<U>(value));
            } else {
                copy->children[i] = _set_in(level - Bits, *static_cast<const Branch*>(node.children[i].get()), index, std::forward<U>(value));
            }
            return copy;
        }

        // The full tail hung on the trie as its next leaf, on a vector
        // nobody else holds (one being built): the root replaced, the
        // trie given a level when it is full at its height
        void _absorb_tail() {
            auto index = _size - Width;   // where the tail begins
            if (!_root) {
                unique_ptr<Branch> root = Branch::make();
                root->children[0] = _tail;
                _root = std::move(root);
                _shift = Bits;
            } else if (index == Width << _shift) {
                unique_ptr<Branch> root = Branch::make();
                root->children[0] = _root;
                root->children[1] = _new_path(_shift, _tail);
                _root = std::move(root);
                _shift += Bits;
            } else {
                _root = _push_leaf(_shift, *_root, index, _tail);
            }
        }

        template<class... A>
        vector _push_back(A&&... a) const {
            vector v = *this;
            auto count = _tail_count();
            if (count == Width) {
                v._absorb_tail();
                count = 0;
            }
            unique_ptr<Leaf> tail = count ? _copy_leaf(*_tail, count) : _make_leaf();
            _append(*tail, std::forward<A>(a)...);
            v._tail = std::move(tail);
            ++v._size;
            return v;
        }

        template<class U>
        vector _set(size_type i, U&& value) const {
            if (i >= _size) {
                throw std::out_of_range("sgcl::vector::set");
            }
            auto offset = _tail_offset();
            if (i >= offset) {
                return vector(_size, _shift, _root, _copy_leaf(*_tail, _tail_count(), uint32_t(i - offset), std::forward<U>(value)));
            }
            return vector(_size, _shift, _set_in(_shift, *_root, i, std::forward<U>(value)), _tail);
        }

        size_t _size = 0;
        size_t _shift = 0;           // the level of the root: 5 when its children are leaves, 10 above that
        tracked_ptr<Branch> _root;   // null while every element is in the tail
        tracked_ptr<Leaf> _tail;     // null when empty
    };

    template<std::input_iterator InputIt>
    vector(InputIt, InputIt) -> vector<typename std::iterator_traits<InputIt>::value_type>;
}
