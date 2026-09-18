//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../core/make_tracked.h"
#include "../core/tracked_ptr.h"
#include "detail/synth_three_way.h"
#include "m_sequence.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <compare>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <ranges>

namespace sgcl {
    namespace detail {
        // A block of a deque: raw storage for N elements, of which those at
        // [first, last) are constructed, a range the deque maintains as it
        // pushes and pops. The elements come first, so that a pointer to
        // the block is a pointer to its first slot. The collector runs the
        // destructor when the block is garbage; a deque that dies inside a
        // garbage managed object has its map pointer nulled before its own
        // destructor, and the elements still in the range are destroyed
        // here. The range is read on the collector's thread after the
        // deque let the block go; the ordering between the deque's last
        // write and that read is the collector's (a block is destroyed
        // only after a cycle found it unreferenced), which the thread
        // sanitizer cannot see: the reads are hidden from it.
        // The page is zero when it is issued to this type and a destroyed
        // element's tracked pointers are null again (maker.h: _init), so
        // the pointer map of the block never meets a stale pointer in an
        // unconstructed slot.
        template<class T, size_t N>
        struct DequeBlock {
            union {
                T elems[N];
            };
            uint32_t first;
            uint32_t last;

            DequeBlock() noexcept
            : first(0)
            , last(0) {
            }

            DequeBlock(const DequeBlock&) = delete;
            DequeBlock& operator=(const DequeBlock&) = delete;

            SGCL_NO_SANITIZE ~DequeBlock() {
                if constexpr (!std::is_trivially_destructible_v<T>) {
                    auto f = *(const volatile uint32_t*)&first;
                    auto l = *(const volatile uint32_t*)&last;
                    for (; f < l; ++f) {
                        elems[f].~T();
                    }
                }
            }
        };

        // A block may hold pointers only when its elements may: a block of
        // plain data is not scanned.
        template<class T, size_t N>
        struct MayContainTracked<DequeBlock<T, N>> {
            static constexpr auto value = MayContainTracked<T>::value;
        };
    }

    // Elements live in blocks on the managed heap, addressed through a
    // managed array of block pointers (the map). The deque constructs and
    // destroys its elements itself, at insert and erase time, as std::deque
    // does; the blocks and the maps are reclaimed by the collector once
    // nothing points at them. A map that is outgrown is replaced by a
    // fresh one (never shifted in place). A block that becomes empty stays
    // in the map as the spare block of its end, ready for the next push
    // there or, moved across, at the other end, so that a window of
    // elements travelling through the deque allocates no blocks; an older
    // spare at the same end goes, and every block goes when the deque
    // becomes empty. The non-null entries of the map are thus the blocks
    // holding elements plus at most one spare before them and one after;
    // one more entry, always null, follows the last, so that an iterator
    // stepping onto the end at a block boundary reads an entry. An empty
    // deque starts at a block boundary.
    //
    // An iterator is raw: a pointer to the map, the index of its slot and
    // a pointer to the element, so that a step within a block and the
    // access are plain loads, and a step across a boundary one load of
    // the map. The deque object roots the map and, through it, the
    // blocks, and an iterator in a frame roots them too, since the
    // collector scans the stacks conservatively. Copying an iterator costs
    // nothing and it may live anywhere, in a std::vector too. Invalidation
    // follows std::deque: an insertion or erasure at either end keeps
    // references valid but invalidates iterators, and any other insertion
    // or erasure invalidates both; an invalid iterator must not be used,
    // as in std.
    template<class T>
    class deque : public m_sequence<deque<T>> {   // the algorithms as members
        static constexpr size_t BlockSize = std::bit_floor(std::max<size_t>(1, 4096 / sizeof(T)));

        using Block = detail::DequeBlock<T, BlockSize>;
        using BlockPtr = tracked_ptr<Block>;
        using MapPtr = tracked_ptr<tracked_ptr<Block>>;

        template<class U>
        class Iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;
            using value_type = std::remove_const_t<U>;
            using difference_type = ptrdiff_t;
            using pointer = U*;
            using reference = U&;
            using reverse_iterator = std::reverse_iterator<Iterator>;

            Iterator() noexcept
            : _map(nullptr)
            , _index(0)
            , _elem(nullptr) {
            }

            Iterator(const Iterator&) = default;
            Iterator& operator=(const Iterator&) = default;

            // A dead iterator does not keep its map or block alive: the
            // collector scans the stack conservatively, and a temporary
            // left behind in a frame would count as a root until the word
            // is overwritten.
            ~Iterator() noexcept {
                _map = nullptr;
                _elem = nullptr;
            }

            reference operator*() const noexcept {
                return *_elem;
            }

            pointer operator->() const noexcept {
                return _elem;
            }

            reference operator[](difference_type n) const noexcept {
                return *_elem_at(_index + n);
            }

            Iterator& operator++() noexcept {
                ++_index;
                if (_index % BlockSize) {
                    ++_elem;
                } else {
                    _elem = _block(_index);
                }
                return *this;
            }

            Iterator operator++(int) noexcept {
                Iterator tmp = *this;
                ++*this;
                return tmp;
            }

            Iterator& operator--() noexcept {
                if (_index % BlockSize) {
                    --_elem;
                } else {
                    _elem = _block(_index - 1) + (BlockSize - 1);
                }
                --_index;
                return *this;
            }

            Iterator operator--(int) noexcept {
                Iterator tmp = *this;
                --*this;
                return tmp;
            }

            Iterator& operator+=(difference_type n) noexcept {
                _index += n;
                _elem = _elem_at(_index);
                return *this;
            }

            Iterator& operator-=(difference_type n) noexcept {
                _index -= n;
                _elem = _elem_at(_index);
                return *this;
            }

            Iterator operator+(difference_type n) const noexcept {
                return Iterator(_map, _index + n);
            }

            Iterator operator-(difference_type n) const noexcept {
                return Iterator(_map, _index - n);
            }

            friend Iterator operator+(difference_type n, const Iterator& i) noexcept {
                return i + n;
            }

            difference_type operator-(const Iterator& other) const noexcept {
                return difference_type(_index) - difference_type(other._index);
            }

            reverse_iterator make_reverse_iterator() const noexcept {
                return reverse_iterator(*this);
            }

            operator Iterator<const value_type>() const noexcept {
                return Iterator<const value_type>(_map, _index, _elem);
            }

        private:
            // Without a map (an empty deque that never held anything) the
            // iterator is the end, whatever the index: begin() + 0 and
            // the like read no entry
            Iterator(BlockPtr* map, size_t index) noexcept
            : _map(map)
            , _index(index)
            , _elem(map ? _elem_at(index) : nullptr) {
            }

            Iterator(BlockPtr* map, size_t index, value_type* elem) noexcept
            : _map(map)
            , _index(index)
            , _elem(elem) {
            }

            // The first slot of the block holding the slot `index`: one
            // plain load of the map; null when the block is not allocated,
            // which is the end at a block boundary.
            value_type* _block(size_t index) const noexcept {
                return reinterpret_cast<value_type*>(_map[index / BlockSize].get());
            }

            value_type* _elem_at(size_t index) const noexcept {
                return _block(index) + index % BlockSize;
            }

            BlockPtr* _map;
            size_t _index;
            value_type* _elem;

            friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept {
                return lhs._index == rhs._index;
            }

            friend std::strong_ordering operator<=>(const Iterator& lhs, const Iterator& rhs) noexcept {
                return lhs._index <=> rhs._index;
            }

            template<class> friend class Iterator;
            template<class> friend class deque;
        };

    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using iterator = Iterator<T>;
        using const_iterator = Iterator<const T>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        deque()
        : _map_size(0)
        , _start(0)
        , _size(0) {
        }

        explicit deque(size_type count) requires std::default_initializable<T>
        : deque() {
            try {
                resize(count);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        deque(size_type count, const T& value)
        : deque() {
            try {
                assign(count, value);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        template<std::input_iterator InputIt>
        deque(InputIt first, InputIt last)
        : deque() {
            try {
                assign(first, last);
            }
            catch (...) {
                clear();
                throw;
            }
        }

        // From a range of what the elements are made of (the pieces of a
        // string, a view, another container), as C++23's from_range: a
        // container is copied by its own constructor, not this one
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, deque>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit deque(R&& r)
        : deque(std::ranges::begin(r), std::ranges::end(r)) {
        }

        deque(std::initializer_list<T> ilist)
        : deque(ilist.begin(), ilist.end()) {
        }

        deque(const deque& other)
        : deque(other.begin(), other.end()) {
        }

        deque(deque&& other) noexcept
        : _map(std::move(other._map))
        , _map_size(other._map_size)
        , _start(other._start)
        , _size(other._size) {
            other._map = nullptr;
            other._map_size = 0;
            other._start = 0;
            other._size = 0;
        }

        // The elements are destroyed here, unless the deque dies in a
        // sweep (_destroy_all): its blocks are garbage of the same sweep,
        // possibly destroyed already, and destroy the elements they still
        // hold themselves.
        ~deque() {
            if (_map) {
                _destroy_all();
            }
        }

        deque& operator=(const deque& other) {
            if (this != &other) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        deque& operator=(deque&& other) noexcept {
            if (this != &other) {
                clear();
                _map = other._map;
                _map_size = other._map_size;
                _start = other._start;
                _size = other._size;
                other._map = nullptr;
                other._map_size = 0;
                other._start = 0;
                other._size = 0;
            }
            return *this;
        }

        deque& operator=(std::initializer_list<T> ilist) {
            assign(ilist);
            return *this;
        }

        void assign(size_type count, const T& value) {
            size_type i = 0;
            for (; i < count && i < _size; ++i) {
                _elem(i) = value;
            }
            while (_size > count) {
                pop_back();
            }
            for (; i < count; ++i) {
                push_back(value);
            }
        }

        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last) {
            size_type i = 0;
            for (; first != last && i < _size; ++first, ++i) {
                _elem(i) = *first;
            }
            while (_size > i) {
                pop_back();
            }
            for (; first != last; ++first) {
                emplace_back(*first);
            }
        }

        void assign(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        reference at(size_type pos) {
            if (pos >= _size) {
                throw std::out_of_range("sgcl::deque");
            }
            return _value(pos);
        }

        const_reference at(size_type pos) const {
            if (pos >= _size) {
                throw std::out_of_range("sgcl::deque");
            }
            return _value(pos);
        }

        reference operator[](size_type pos) {
            return _value(pos);
        }

        const_reference operator[](size_type pos) const {
            return _value(pos);
        }

        reference front() {
            return _value(0);
        }

        const_reference front() const {
            return _value(0);
        }

        reference back() {
            return _value(_size - 1);
        }

        const_reference back() const {
            return _value(_size - 1);
        }

        iterator begin() noexcept {
            auto map = _map.get_plain();
            return map ? iterator(map, _start) : iterator();
        }

        const_iterator begin() const noexcept {
            auto map = _map.get_plain();
            return map ? const_iterator(map, _start) : const_iterator();
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            auto map = _map.get_plain();
            return map ? iterator(map, _start + _size) : iterator();
        }

        const_iterator end() const noexcept {
            auto map = _map.get_plain();
            return map ? const_iterator(map, _start + _size) : const_iterator();
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

        bool empty() const noexcept {
            return _size == 0;
        }

        size_type size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<difference_type>::max();
        }

        // Replaces the map by one holding exactly the blocks in use: the
        // spare blocks go.
        void shrink_to_fit() {
            if (!_size) {
                clear();
                return;
            }
            auto used = _used_blocks();
            if (used < _map_size) {
                _reallocate_map(used, 0);
            }
        }

        void clear() noexcept {
            if (_map) {
                _destroy_all();
            }
            _map = nullptr;
            _map_size = 0;
            _start = 0;
            _size = 0;
        }

        iterator insert(const_iterator pos, const T& value) {
            return emplace(pos, value);
        }

        iterator insert(const_iterator pos, T&& value) {
            return emplace(pos, std::move(value));
        }

        iterator insert(const_iterator pos, size_type count, const T& value) {
            size_type index = pos._index - _start;
            if (!count) {
                return begin() + index;
            }
            if (_inside(&value)) {   // an element of this deque: copied before anything moves
                T copy(value);
                return insert(pos, count, copy);
            }
            return _insert(index, count, true,
                [&] {
                    for (size_type i = 0; i < count; ++i) {
                        push_front(value);
                    }
                },
                [&] {
                    for (size_type i = 0; i < count; ++i) {
                        push_back(value);
                    }
                });
        }

        template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            size_type index = pos._index - _start;
            if constexpr (!std::forward_iterator<InputIt>) {
                // single pass: the elements are collected first, so that
                // the count is known and a failure leaves the deque as it was
                deque tmp(first, last);
                return insert(begin() + index, std::make_move_iterator(tmp.begin()), std::make_move_iterator(tmp.end()));
            } else {
                if (first == last) {
                    return begin() + index;
                }
                size_type count = std::distance(first, last);
                return _insert(index, count, std::bidirectional_iterator<InputIt>,
                    [&] {
                        if constexpr (std::bidirectional_iterator<InputIt>) {
                            for (auto i = last; i != first;) {
                                emplace_front(*--i);
                            }
                        }
                    },
                    [&] {
                        for (auto i = first; i != last; ++i) {
                            emplace_back(*i);
                        }
                    });
            }
        }

        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        template<class... A>
        iterator emplace(const_iterator pos, A&&... a) {
            size_type index = pos._index - _start;
            if (index == 0) {
                emplace_front(std::forward<A>(a)...);
                return begin();
            }
            if (index == _size) {
                emplace_back(std::forward<A>(a)...);
                return end() - 1;
            }
            T value(std::forward<A>(a)...);   // before anything moves: the arguments may refer into this deque
            if (index <= _size / 2) {
                emplace_front(std::move(front()));
                auto first = begin();
                std::move(first + 2, first + index + 1, first + 1);
                *(first + index) = std::move(value);
            } else {
                emplace_back(std::move(back()));
                auto first = begin();
                std::move_backward(first + index, first + _size - 2, first + _size - 1);
                *(first + index) = std::move(value);
            }
            return begin() + index;
        }

        // erase(end()) is a no-op that forms no iterator past the end: with
        // one element per block that would read the map entry past the
        // null one
        iterator erase(const_iterator pos) {
            return pos == cend() ? end() : erase(pos, pos + 1);
        }

        iterator erase(const_iterator first, const_iterator last) {
            size_type index = first._index - _start;
            size_type count = last._index - first._index;
            if (index >= _size || !count) {
                return begin() + index;
            }
            if (index < _size - index - count) {
                std::move_backward(begin(), begin() + index, begin() + index + count);
                for (; count; --count) {
                    pop_front();
                }
            } else {
                std::move(begin() + index + count, end(), begin() + index);
                for (; count; --count) {
                    pop_back();
                }
            }
            return begin() + index;
        }

        void push_back(const T& value) {
            emplace_back(value);
        }

        void push_back(T&& value) {
            emplace_back(std::move(value));
        }

        // The common case, a block at the end (the last one in use, or the
        // spare one): a load of the map, a load of the block, the
        // construction, the block's range, the size. The map at its end or
        // the block missing goes the slow way.
        template<class... A>
        reference emplace_back(A&&... a) {
            auto index = _start + _size;
            if (index < _map_size * BlockSize) {
                auto block = _map.get()[index / BlockSize].get();
                if (block) {
                    auto offset = index % BlockSize;
                    assert(block->last == offset);
                    auto& value = *::new (static_cast<void*>(block->elems + offset)) T(std::forward<A>(a)...);
                    block->last = uint32_t(offset + 1);
                    ++_size;
                    return _value(value);
                }
            }
            return _emplace_back_slow(std::forward<A>(a)...);
        }

        // The element is destroyed first; a block left empty stays as the
        // spare of its end (the older spare goes) while the deque holds
        // elements, and every block goes when it holds none. A dropped
        // block is not touched again.
        void pop_back() {
            auto index = _start + _size - 1;
            auto offset = index % BlockSize;
            auto block = _map.get_plain()[index / BlockSize].get_plain();
            assert(block->last == offset + 1);
            block->last = uint32_t(offset);
            block->elems[offset].~T();
            --_size;
            if (!_size) {
                _drop_all_blocks(index / BlockSize);
            } else if (offset == 0) {
                _drop_block(index / BlockSize + 1);
            }
        }

        void push_front(const T& value) {
            emplace_front(value);
        }

        void push_front(T&& value) {
            emplace_front(std::move(value));
        }

        template<class... A>
        reference emplace_front(A&&... a) {
            if (_start) {
                auto index = _start - 1;
                auto block = _map.get_plain()[index / BlockSize].get_plain();
                if (block) {
                    auto offset = index % BlockSize;
                    assert(block->first == offset + 1);
                    auto& value = *::new (static_cast<void*>(block->elems + offset)) T(std::forward<A>(a)...);
                    block->first = uint32_t(offset);
                    --_start;
                    ++_size;
                    return _value(value);
                }
            }
            return _emplace_front_slow(std::forward<A>(a)...);
        }

        void pop_front() {
            auto index = _start;
            auto offset = index % BlockSize;
            auto block = _map.get_plain()[index / BlockSize].get_plain();
            assert(block->first == offset);
            block->first = uint32_t(offset + 1);
            block->elems[offset].~T();
            ++_start;
            --_size;
            if (!_size) {
                _drop_all_blocks(index / BlockSize);
            } else if (offset == BlockSize - 1) {
                _drop_block(index / BlockSize - 1);
            }
        }

        void resize(size_type count) requires std::default_initializable<T> {
            while (_size > count) {
                pop_back();
            }
            while (_size < count) {
                emplace_back();
            }
        }

        void resize(size_type count, const value_type& value) {
            while (_size > count) {
                pop_back();
            }
            while (_size < count) {
                push_back(value);
            }
        }

        void swap(deque& other) noexcept {
            _map.swap(other._map);
            std::swap(_map_size, other._map_size);
            std::swap(_start, other._start);
            std::swap(_size, other._size);
        }

    private:
        tracked_ptr<BlockPtr> _map;   // the root
        size_t _map_size;   // entries in the map, not counting the null one past them
        size_t _start;      // slot index of the first element, at most _map_size * BlockSize
        size_t _size;

        // The map and the blocks are this thread's own (tracked_ptr.h:
        // get_plain): plain loads, which the compiler may keep in a loop
        T& _elem(size_type i) const noexcept {
            auto index = _start + i;
            return _map.get_plain()[index / BlockSize].get_plain()->elems[index % BlockSize];
        }

        // Whether p is one of the elements: a load of the map per block in
        // use, as the elements of a block are its first bytes
        bool _inside(const void* p) const noexcept {
            if (!_size) {
                return false;
            }
            auto map = _map.get_plain();
            for (auto block = _start / BlockSize, last = (_start + _size - 1) / BlockSize; block <= last; ++block) {
                if ((uintptr_t)p - (uintptr_t)map[block].get_plain() < sizeof(T) * BlockSize) {
                    return true;
                }
            }
            return false;
        }

        T& _value(size_type i) const noexcept {
            return _elem(i);
        }

        static T& _value(T& e) noexcept {
            return e;
        }

        // The blocks the elements occupy, and the spare ones at either end
        // of the map
        size_t _used_blocks() const noexcept {
            return _size ? (_start + _size - 1) / BlockSize - _start / BlockSize + 1 : 0;
        }

        // The entries of the spare blocks: just before the first block in
        // use and just after the last one (out of the map's range when
        // there is no room for one). For an empty deque, at its block
        // boundary, the one after is the block of the boundary itself.
        size_t _front_spare() const noexcept {
            return _start / BlockSize - 1;
        }

        size_t _back_spare() const noexcept {
            return (_start + _size + BlockSize - 1) / BlockSize;
        }

        // A block let go of (its elements destroyed already): the map's
        // word nulled, the block is the collector's
        void _drop_block(size_t block) noexcept {
            if (block < _map_size) {
                _map.get_plain()[block] = nullptr;
            }
        }

        // The deque is empty, its last element was in `block`: that block
        // and the spares go, and the deque starts at a block boundary.
        void _drop_all_blocks(size_t block) noexcept {
            auto map = _map.get_plain();
            if (block >= 1) {
                map[block - 1] = nullptr;
            }
            map[block] = nullptr;
            if (block + 1 < _map_size) {
                map[block + 1] = nullptr;
            }
            _start = block * BlockSize;
        }

        // The map is at its end or the block is missing: the map grows if
        // it must, a block is found.
        template<class... A>
        SGCL_NOINLINE reference _emplace_back_slow(A&&... a) {
            if (_start + _size == _map_size * BlockSize) {
                _grow_back();
            }
            auto& value = _construct_in_new_block(_start + _size, _front_spare(), std::forward<A>(a)...);
            ++_size;
            return _value(value);
        }

        // The slow paths of the pushes, out of line: a block to make or the
        // map to grow at that end
        template<class... A>
        SGCL_NOINLINE reference _emplace_front_slow(A&&... a) {
            if (!_start) {
                _grow_front();
            }
            auto& value = _construct_in_new_block(_start - 1, _back_spare(), std::forward<A>(a)...);
            --_start;
            ++_size;
            return _value(value);
        }

        // The entry of the slot `index` is null: the spare block of the
        // other end, at the entry `spare` when there is one, moves in,
        // else a block is allocated; the element is constructed in it and
        // the block's range becomes that one slot. The block is dropped
        // again when the constructor throws.
        template<class... A>
        reference _construct_in_new_block(size_t index, size_t spare, A&&... a) {
            auto map = _map.get_plain();
            auto& entry = map[index / BlockSize];
            assert(!entry);
            if (spare < _map_size && map[spare]) {
                entry = map[spare];
                map[spare] = nullptr;
            } else {
                entry = make_tracked<Block>();
            }
            auto block = entry.get_plain();
            auto offset = index % BlockSize;
            try {
                auto& value = *::new (static_cast<void*>(block->elems + offset)) T(std::forward<A>(a)...);
                block->first = uint32_t(offset);
                block->last = uint32_t(offset + 1);
                return _value(value);
            }
            catch (...) {
                entry = nullptr;
                throw;
            }
        }

        // Block by block, the map and the block loaded once per block; the
        // ranges are emptied, so that the blocks destroy nothing when they
        // are collected. A trivially destructible element has nothing to
        // run: the blocks go with the map as they are. In a sweep (the
        // destructor, a clear or an assignment from the destructor of a
        // dying managed object) the blocks are garbage of the same sweep,
        // possibly destroyed already with their elements: nothing is
        // touched, each block destroys what it still holds.
        void _destroy_all() noexcept {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                if (detail::sweeping) {
                    return;
                }
                auto map = _map.get_plain();
                for (auto index = _start, last = _start + _size; index < last;) {
                    auto block = map[index / BlockSize].get_plain();
                    auto stop = std::min(last, (index / BlockSize + 1) * BlockSize);
                    block->first = block->last = 0;
                    for (; index < stop; ++index) {
                        block->elems[index % BlockSize].~T();
                    }
                }
            }
        }

        // A fresh map of `count` entries, plus the null one past them, with
        // the blocks in use at [first, first + used); the spare blocks
        // travel with them when the new map has room for them.
        void _reallocate_map(size_t count, size_t first) {
            auto used = _used_blocks();
            auto block = _start / BlockSize;
            auto old = _map.get_plain();
            bool with_front = first >= 1 && block >= 1 && old[block - 1];
            bool with_back = first + used < count && block + used < _map_size && old[block + used];
            MapPtr map = unique_ptr<BlockPtr>(detail::Maker<BlockPtr[]>::make_tracked_data(count + 1));
            auto src = old + block;
            auto dst = map.get_plain() + first;
            for (auto i = -ptrdiff_t(with_front), stop = ptrdiff_t(used + with_back); i < stop; ++i) {
                dst[i] = src[i];
            }
            _map = map;
            _map_size = count;
            _start = first * BlockSize + _start % BlockSize;
        }

        // Room for one more block at the back: the blocks in use are
        // recentred in the map when it is at most half full, else in a map
        // twice as large.
        void _grow_back() {
            auto used = _used_blocks();
            auto count = _map_size >= 2 * (used + 1) ? _map_size : std::max(2 * _map_size, used + 3);
            _reallocate_map(count, (count - used) / 2);
        }

        // The map regrown or re-centred so that a block fits at that end
        void _grow_front() {
            auto used = _used_blocks();
            auto count = _map_size >= 2 * (used + 1) ? _map_size : std::max(2 * _map_size, used + 3);
            _reallocate_map(count, (count - used + 1) / 2);
        }

        // Inserts `count` elements before `index`: they are pushed at the
        // nearer end (the front only when `can_front`) and rotated into
        // place. A push that throws is undone.
        template<class Front, class Back>
        iterator _insert(size_type index, size_type count, bool can_front, Front&& push_front_all, Back&& push_back_all) {
            auto old_size = _size;
            if (index == old_size) {
                _guarded_back(push_back_all, old_size);
            } else if (can_front && index <= old_size / 2) {
                _guarded_front(push_front_all, old_size);
                if (index) {
                    std::rotate(begin(), begin() + count, begin() + count + index);
                }
            } else {
                _guarded_back(push_back_all, old_size);
                std::rotate(begin() + index, begin() + old_size, end());
            }
            return begin() + index;
        }

        // A batch of pushes at one end; what was pushed is popped again if
        // one of them throws (the range and fill insertions at the ends)
        template<class F>
        void _guarded_back(F& push_all, size_type old_size) {
            try {
                push_all();
            }
            catch (...) {
                while (_size > old_size) {
                    pop_back();
                }
                throw;
            }
        }

        template<class F>
        void _guarded_front(F& push_all, size_type old_size) {
            try {
                push_all();
            }
            catch (...) {
                while (_size > old_size) {
                    pop_front();
                }
                throw;
            }
        }

        friend bool operator==(const deque& lhs, const deque& rhs) {
            return lhs.size() == rhs.size() && std::equal(lhs.begin(), lhs.end(), rhs.begin());
        }

        friend auto operator<=>(const deque& lhs, const deque& rhs) {
            return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), detail::synth_three_way);
        }
    };

    template<class T>
    class deque<unique_ptr<T>> : public std::deque<unique_ptr<T>> {
    public:
        using std::deque<unique_ptr<T>>::deque;
    };

    template<class T>
    inline void swap(deque<T>& lhs, deque<T>& rhs) noexcept {
        lhs.swap(rhs);
    }

    template<class T, class Pred>
    inline typename deque<T>::size_type erase_if(deque<T>& c, Pred pred) {
        auto it = std::remove_if(c.begin(), c.end(), pred);
        auto removed = c.end() - it;
        c.erase(it, c.end());
        return removed;
    }

    template<class T, class U>
    inline typename deque<T>::size_type erase(deque<T>& c, const U& value) {
        return erase_if(c, [&value](const T& v) { return v == value; });
    }
}

namespace std {
    using sgcl::erase;
    using sgcl::erase_if;
}
