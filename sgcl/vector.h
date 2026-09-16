//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "detail/contiguous_iterator.h"
#include "detail/synth_three_way.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

namespace sgcl {
    // std::vector over a managed buffer. The object is one word: a tracked
    // pointer to the first element; the buffer's header (detail::ArrayBase,
    // just before the elements) holds the number of constructed elements
    // and the capacity. The collector reads that count when it reclaims a
    // buffer that nobody holds any more, so it always equals the number of
    // live elements: elements are constructed and destroyed one at a time
    // and the count follows every step.
    //
    // An explicit removal (erase, pop_back, clear, resize, assign) destroys
    // the elements at once, like in std::vector. The vector destroys its
    // elements itself, always: on removal, on a reallocation (the moved-from
    // ones), in its destructor, wherever that runs. The collector never
    // destroys a buffer, it only frees one nobody refers to. A buffer is
    // referenced only through a pointer to its first element, the one the
    // vector holds: a pointer to an element does not keep the buffer, so it
    // dangles once the vector is gone, as in std (README, "Containers").
    // The buffer is held by a tracked_ptr, so the vector lives where one
    // may: on a stack or inside a managed object.
    template<class T>
    class vector {
    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using pointer = T*;
        using const_pointer = const T*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using iterator = detail::ContiguousIterator<value_type>;
        using const_iterator = detail::ContiguousIterator<const value_type>;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        vector() noexcept = default;

        explicit vector(size_type count) {
            _construct_default(count);
        }

        vector(size_type count, const T& value) {
            _construct_fill(count, value);
        }

        template<std::input_iterator InputIt>
        vector(InputIt first, InputIt last) {
            _construct_range(first, last);
        }

        vector(std::initializer_list<T> ilist)
        : vector(ilist.begin(), ilist.end()) {
        }

        vector(const vector& other)
        : vector(other.begin(), other.end()) {
        }

        vector(vector&& other) noexcept
        : _ptr(other._ptr)
        , _size(other._size)
        , _capacity(other._capacity) {
            other._ptr = nullptr;
            other._size = 0;
            other._capacity = 0;
        }

        // The elements die here, wherever the vector dies: on a stack, or
        // in a sweep inside a managed object. The buffer is the collector's
        // to free, never to destroy (array_base.h), so the elements are
        // intact until this runs, whatever thread the sweep is on.
        ~vector() {
            _destroy_range(_data(), _size);
        }

        vector& operator=(const vector& other) {
            if (this != &other) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        vector& operator=(vector&& other) noexcept {
            if (this != &other) {
                _destroy_range(_data(), _size);
                _ptr = other._ptr;
                _size = other._size;
                _capacity = other._capacity;
                other._ptr = nullptr;
                other._size = 0;
                other._capacity = 0;
            }
            return *this;
        }

        vector& operator=(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
            return *this;
        }

        void assign(size_type count, const T& value) {
            if (_inside(&value)) {
                T copy(value);
                assign(count, copy);
                return;
            }
            if (count > capacity()) {
                vector fresh(count, value);
                swap(fresh);
                return;
            }
            auto data = _data();
            auto s = size();
            auto n = std::min(s, count);
            for (size_type i = 0; i < n; ++i) {
                data[i] = value;
            }
            if (count > s) {
                for (auto i = s; i < count; ++i) {
                    _construct(data + i, value);
                }
            } else {
                _destroy(data + count, s - count);
            }
        }

        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last) {
            if constexpr(std::forward_iterator<InputIt>) {
                auto count = (size_type)std::distance(first, last);
                if (count > capacity()) {
                    vector fresh(first, last);
                    swap(fresh);
                    return;
                }
                auto data = _data();
                auto s = size();
                auto n = std::min(s, count);
                for (size_type i = 0; i < n; ++i, ++first) {
                    data[i] = *first;
                }
                if (count > s) {
                    for (auto i = s; i < count; ++i, ++first) {
                        _construct(data + i, *first);
                    }
                } else {
                    _destroy(data + count, s - count);
                }
            } else {
                clear();
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            }
        }

        void assign(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        reference at(size_type pos) {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::vector::at");
            }
            return _values()[pos];
        }

        const_reference at(size_type pos) const {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::vector::at");
            }
            return _values()[pos];
        }

        reference operator[](size_type pos) noexcept {
            return _values()[pos];
        }

        const_reference operator[](size_type pos) const noexcept {
            return _values()[pos];
        }

        reference front() noexcept {
            return _values()[0];
        }

        const_reference front() const noexcept {
            return _values()[0];
        }

        reference back() noexcept {
            return _values()[size() - 1];
        }

        const_reference back() const noexcept {
            return _values()[size() - 1];
        }

        T* data() noexcept {
            return _values();
        }

        const T* data() const noexcept {
            return _values();
        }

        iterator begin() noexcept {
            return iterator(_values());
        }

        const_iterator begin() const noexcept {
            return const_iterator(_values());
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        iterator end() noexcept {
            return iterator(_values() + size());
        }

        const_iterator end() const noexcept {
            return const_iterator(_values() + size());
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
            return size() == 0;
        }

        size_type size() const noexcept {
            return _size;
        }

        size_type max_size() const noexcept {
            return (size_type)std::numeric_limits<difference_type>::max() / sizeof(T);
        }

        void reserve(size_type new_capacity) {
            if (new_capacity > capacity()) {
                _check_size(new_capacity);
                _reallocate(new_capacity);
            }
        }

        size_type capacity() const noexcept {
            return _capacity;
        }

        // The buffer is replaced by one sized for the elements; a size class
        // may still round it up a little.
        void shrink_to_fit() {
            if (size() < capacity()) {
                if (empty()) {
                    _ptr = nullptr;
                    _capacity = 0;
                } else {
                    _reallocate(size());
                }
            }
        }

        void clear() noexcept {
            _destroy(_data(), size());
        }

        iterator insert(const_iterator pos, const T& value) {
            return emplace(pos, value);
        }

        iterator insert(const_iterator pos, T&& value) {
            return emplace(pos, std::move(value));
        }

        iterator insert(const_iterator pos, size_type count, const T& value) {
            auto index = (size_type)(pos - cbegin());
            if (!count) {
                return begin() + index;
            }
            if (_inside(&value)) {
                T copy(value);
                return insert(pos, count, copy);
            }
            auto s = size();
            _check_size(s + count);
            if (s + count > capacity()) {
                // the copies go into the new buffer first: `value` may be an
                // element of this vector, intact until the elements move
                tracked_ptr<T> lock = _ptr;   // the old buffer held from this frame
                    auto data = _allocate_at_least(s + count);
                size_type constructed = 0;
                try {
                    for (; constructed < count; ++constructed) {
                        detail::Maker<T>::construct(data + index + constructed, value);
                    }
                } catch (...) {
                    _destroy_range(data + index, constructed);
                    _restore(lock);
                    throw;
                }
                _relocate(lock, data, index, count, s);
                return begin() + index;
            }
            auto data = _data();
            _open_gap(data, s, index, count);
            _fill_gap(data, s, index, count, [&](T* p) { detail::Maker<T>::construct(p, value); }, [&](T& e) { e = value; });
            return begin() + index;
        }

        template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            auto index = (size_type)(pos - cbegin());
            if constexpr(std::forward_iterator<InputIt>) {
                auto count = (size_type)std::distance(first, last);
                if (!count) {
                    return begin() + index;
                }
                auto s = size();
                _check_size(s + count);
                if (s + count > capacity()) {
                    tracked_ptr<T> lock = _ptr;   // the old buffer held from this frame
                            auto data = _allocate_at_least(s + count);
                    size_type constructed = 0;
                    try {
                        for (auto it = first; constructed < count; ++constructed, ++it) {
                            detail::Maker<T>::construct(data + index + constructed, *it);
                        }
                    } catch (...) {
                        _destroy_range(data + index, constructed);
                        _restore(lock);
                        throw;
                    }
                    _relocate(lock, data, index, count, s);
                    return begin() + index;
                }
                auto data = _data();
                _open_gap(data, s, index, count);
                auto it = first;
                _fill_gap(data, s, index, count, [&](T* p) { detail::Maker<T>::construct(p, *it); ++it; }, [&](T& e) { e = *it; ++it; });
                return begin() + index;
            } else {
                // single pass: collect first, then insert by moving
                vector collected;
                for (; first != last; ++first) {
                    collected.emplace_back(*first);
                }
                return insert(pos, std::make_move_iterator(collected.begin()), std::make_move_iterator(collected.end()));
            }
        }

        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        template<class... A>
        iterator emplace(const_iterator pos, A&&... a) {
            auto index = (size_type)(pos - cbegin());
            auto s = size();
            if (index == s) {
                emplace_back(std::forward<A>(a)...);
                return begin() + index;
            }
            _check_size(s + 1);
            if (s + 1 > capacity()) {
                tracked_ptr<T> lock = _ptr;   // the old buffer held from this frame
                    auto data = _allocate_at_least(s + 1);
                try {
                    detail::Maker<T>::construct(data + index, std::forward<A>(a)...);
                } catch (...) {
                    _restore(lock);
                    throw;
                }
                _relocate(lock, data, index, 1, s);
                return begin() + index;
            }
            // the arguments may refer to an element that is about to move
            T value(std::forward<A>(a)...);
            auto data = _data();
            _open_gap(data, s, index, 1);
            _fill_gap(data, s, index, 1, [&](T* p) { detail::Maker<T>::construct(p, std::move(value)); }, [&](T& e) { e = std::move(value); });
            return begin() + index;
        }

        iterator erase(const_iterator pos) {
            return pos == cend() ? end() : erase(pos, pos + 1);
        }

        iterator erase(const_iterator first, const_iterator last) {
            auto index = (size_type)(first - cbegin());
            auto count = (size_type)(last - first);
            if (count) {
                auto data = _data();
                auto s = size();
                if constexpr(std::is_trivially_copyable_v<T> && !detail::TypeInfo<T>::MayContainTracked) {
                    std::memmove((void*)(data + index), data + index + count, (s - index - count) * sizeof(T));
                } else {
                    for (auto i = index + count; i < s; ++i) {
                        data[i - count] = std::move(data[i]);
                    }
                }
                _destroy(data + s - count, count);
            }
            return begin() + index;
        }

        void push_back(const T& value) {
            emplace_back(value);
        }

        void push_back(T&& value) {
            emplace_back(std::move(value));
        }

        // The common case is a few instructions and inlines into the
        // caller's loop; the growth is a cold call.
        template<class... A>
        SGCL_INLINE_HOT reference emplace_back(A&&... a) {
            if (auto data = _data()) {
                auto s = _size;
                if (s < _capacity) {
                    auto p = data + s;
                    detail::Maker<T>::construct(p, std::forward<A>(a)...);
                    _size = s + 1;
                    return _value(p);
                }
            }
            if constexpr(sizeof...(A) == 1 && std::is_trivially_copyable_v<T> && sizeof(T) <= 2 * sizeof(void*) && std::is_constructible_v<T, A&&...>) {
                // by value: a reference would pin the caller's variable to memory
                return _emplace_back_grow(T(std::forward<A>(a)...));
            } else {
                return _emplace_back_grow(std::forward<A>(a)...);
            }
        }

        void pop_back() {
            --_size;
            if constexpr(!std::is_trivially_destructible_v<T>) {
                detail::Maker<T>::destroy(_data() + _size);
            }
        }

        void resize(size_type count) {
            auto s = size();
            if (count < s) {
                _destroy(_data() + count, s - count);
            } else if (count > s) {
                reserve(count);
                auto data = _data();
                for (auto i = s; i < count; ++i) {
                    _construct(data + i);
                }
            }
        }

        void resize(size_type count, const value_type& value) {
            auto s = size();
            if (count < s) {
                _destroy(_data() + count, s - count);
            } else if (count > s) {
                insert(cend(), count - s, value);
            }
        }

        void swap(vector& other) noexcept {
            _ptr.swap(other._ptr);
            std::swap(_size, other._size);
            std::swap(_capacity, other._capacity);
        }

        friend void swap(vector& l, vector& r) noexcept {
            l.swap(r);
        }

        friend bool operator==(const vector& l, const vector& r) {
            return l.size() == r.size() && std::equal(l.begin(), l.end(), r.begin());
        }

        friend auto operator<=>(const vector& l, const vector& r) {
            return std::lexicographical_compare_three_way(l.begin(), l.end(), r.begin(), r.end(), detail::synth_three_way);
        }

    private:
        using Header = detail::ArrayBase;

        // Three words: the count, the first element and the capacity. The
        // buffer's header (the capacity the size class granted) is read
        // once, when the buffer is taken: a push_back is then a compare of
        // two words of this object, a store of the element and a store of
        // the count, with nothing loaded from the buffer (README,
        // "Containers"). The count and the capacity are not adjacent on
        // purpose: adjacent, the compiler loads them with one 16-byte
        // instruction, which the 8-byte store of the count cannot feed
        // (a stall of a dozen cycles per push).
        size_type _size = 0;
        tracked_ptr<T> _ptr;
        size_type _capacity = 0;

        // The slow path of push_back and emplace_back, out of line: a larger
        // buffer, the elements moved over, the new one constructed last
        template<class... A>
        SGCL_NOINLINE reference _emplace_back_grow(A... a) {
            auto s = size();
            _check_size(s + 1);
            // the new element first: the arguments may refer to an element
            tracked_ptr<T> lock = _ptr;
            auto data = _allocate_at_least(s + 1);
            try {
                detail::Maker<T>::construct(data + s, std::forward<A>(a)...);
            } catch (...) {
                _restore(lock);
                throw;
            }
            _relocate(lock, data, s, 1, s);
            return _value(data + s);
        }


        // The buffer's header lies before its first element (array_base.h)
        static Header* _header(const T* data) noexcept {
            return (Header*)data - 1;
        }

        T* _values() const noexcept {
            return _data();
        }

        static T& _value(T* p) noexcept {
            return *p;
        }

        T* _data() const noexcept {
            return _ptr.get_plain();   // this thread's own buffer: a plain load
        }

        // Whether p is one of the elements: an insertion of a reference into
        // the vector itself copies the value first
        bool _inside(const void* p) const noexcept {
            auto data = (uintptr_t)_data();
            return data && (uintptr_t)p - data < size() * sizeof(T);
        }

        void _check_size(size_type n) const {
            if (n > max_size()) {
                throw std::length_error("sgcl::vector");
            }
        }

        // A fresh buffer for at least `n` elements (geometric growth from
        // the current capacity); replaces _ptr, the caller keeps the old one.
        T* _allocate_at_least(size_type n) {
            auto grown = capacity() * 2;   // an old buffer is not freed at once but collected: doubling halves what waits (README, Containers)
            auto wanted = std::min(std::max(n, grown), max_size());
            return _allocate(wanted);
        }

        // A fresh buffer for n elements (its capacity may be more: the size
        // class), taken over from the maker's unique_ptr
        T* _allocate(size_type n) {
            _ptr = unique_ptr<T>(detail::Maker<T[]>::make_tracked_data(n));
            auto data = _data();
            _capacity = _header(data)->capacity;
            return data;
        }

        // Back to a previous buffer (an exception while filling a new one).
        void _restore(const tracked_ptr<T>& p) noexcept {
            _ptr = p;
            auto data = _data();
            _capacity = data ? _header(data)->capacity : 0;
        }

        // A buffer of exactly `n` (reserve, shrink_to_fit): the elements
        // move over, the count follows.
        void _reallocate(size_type n) {
            tracked_ptr<T> lock = _ptr;
            auto s = size();
            auto data = _allocate(n);
            _relocate(lock, data, s, 0, s);
        }

        // One more element, at p, the count raised after it is constructed
        template<class... A>
        void _construct(T* p, A&&... a) {
            detail::Maker<T>::construct(p, std::forward<A>(a)...);
            ++_size;
        }

        // Destroys the last `n` elements starting at `first` (which must be
        // the tail of the constructed elements), keeping the count exact.
        void _destroy(T* first, size_type n) noexcept {
            for (auto i = n; i > 0; --i) {
                --_size;
                if constexpr(!std::is_trivially_destructible_v<T>) {
                    detail::Maker<T>::destroy(first + i - 1);
                }
            }
        }

        // The elements of a buffer the vector abandons (an old buffer after
        // a reallocation, the buffer of a dying or overwritten vector, a
        // new buffer dropped by an exception). Tracked pointers are left as
        // they are: their destructor only nulls the word, and a buffer
        // nothing refers to is not traced (one a stale word still keeps
        // alive holds its targets for a cycle, as any dead frame does).
        static void _destroy_range(T* first, size_type n) noexcept {
            if constexpr(!std::is_trivially_destructible_v<T> && !detail::TypeInfo<T>::IsTracked) {
                for (auto i = n; i > 0; --i) {
                    detail::Maker<T>::destroy(first + i - 1);
                }
            }
        }

        // Moves the `s` elements of `old` into `data`, leaving a gap of
        // `count` already constructed elements at `index`; the new count is
        // s + count. The moved-from elements of the old buffer are
        // destroyed here, as std::vector does: a pointer to one of them is
        // invalid after a reallocation.
        void _relocate(const tracked_ptr<T>& lock, T* data, size_type index, size_type count, size_type s) {
            auto old = lock.get_plain();
            if constexpr(std::is_trivially_copyable_v<T> && !detail::TypeInfo<T>::MayContainTracked) {
                if (index) {
                    std::memcpy((void*)data, old, index * sizeof(T));
                }
                if (s > index) {
                    std::memcpy((void*)(data + index + count), old + index, (s - index) * sizeof(T));
                }
                _size = s + count;
            } else if constexpr(detail::TypeInfo<T>::IsTracked) {
                // A buffer of tracked pointers moves as words, with no
                // barrier per target: the barrier is on the old buffer,
                // set by `lock` (a copy of the handle taken before the
                // handle moved to the new buffer). The old buffer is
                // reachable through this cycle, from the state after the
                // demotion and from `lock` on this stack for the scan, and
                // its words stay as they are (_destroy_range), so its
                // targets are reached through it whether the new buffer is
                // traced before or after the copy. It is garbage the cycle
                // after, by which time the new buffer has been traced.
                // (The state may already be demoted by a cycle that began
                // meanwhile: then `lock` is what the scan finds.)
                if (index) {
                    std::memcpy((void*)data, old, index * sizeof(T));
                }
                if (s > index) {
                    std::memcpy((void*)(data + index + count), old + index, (s - index) * sizeof(T));
                }
                _size = s + count;
            } else {
                size_type i = 0;
                try {
                    for (; i < index; ++i) {
                        detail::Maker<T>::construct(data + i, std::move_if_noexcept(old[i]));
                    }
                    for (; i < s; ++i) {
                        detail::Maker<T>::construct(data + count + i, std::move_if_noexcept(old[i]));
                    }
                } catch (...) {
                    // only a copying type can throw here: the old buffer is
                    // intact, the new one is dropped with what it holds
                    _destroy_range(data, std::min(i, index));
                    if (i > index) {
                        _destroy_range(data + index + count, i - index);
                    }
                    _destroy_range(data + index, count);
                    _restore(lock);
                    throw;
                }
                _size = s + count;
                _destroy_range(old, s);
            }
        }

        // Shifts elements [index, s) up by `count` within the buffer. After
        // the call [index, index + count) holds either moved-from elements
        // (where the old element count reached) or nothing; _fill_gap fills
        // them accordingly.
        void _open_gap(T* data, size_type s, size_type index, size_type count) {
            auto tail = s - index;
            if (tail > count) {
                for (auto i = s; i > s - count; --i) {
                    detail::Maker<T>::construct(data + i - 1 + count, std::move(data[i - 1]));
                    ++_size;
                }
                for (auto i = s - count; i > index; --i) {
                    data[i - 1 + count] = std::move(data[i - 1]);
                }
            } else {
                // every tail element moves into raw storage; the count is
                // raised past the gap when the gap is filled
                for (auto i = s; i > index; --i) {
                    detail::Maker<T>::construct(data + i - 1 + count, std::move(data[i - 1]));
                }
            }
        }

        // The gap _open_gap made filled: assigned where old elements were,
        // constructed where the storage is raw
        template<class Construct, class Assign>
        void _fill_gap(T* data, size_type s, size_type index, size_type count, Construct construct, Assign assign) {
            auto tail = s - index;
            if (tail > count) {
                for (auto i = index; i < index + count; ++i) {
                    assign(data[i]);
                }
            } else {
                for (auto i = index; i < s; ++i) {
                    assign(data[i]);
                }
                for (auto i = s; i < index + count; ++i) {
                    construct(data + i);
                    ++_size;
                }
                _size += tail;
            }
        }

        // The constructors' bodies: count elements value-initialized, or
        // copies of a value, or a range, into a buffer of the right size
        void _construct_default(size_type count) {
            if (!count) {
                return;
            }
            _check_size(count);
            auto data = _allocate(count);
            if constexpr(detail::TypeInfo<T>::IsTracked) {
                // the buffer is zeroed: null tracked pointers already
                (void)data;
                _size = count;
            } else {
                _guarded([&] {
                    for (size_type i = 0; i < count; ++i) {
                        _construct(data + i);
                    }
                });
            }
        }

        void _construct_fill(size_type count, const T& value) {
            if (!count) {
                return;
            }
            _check_size(count);
            auto data = _allocate(count);
            _guarded([&] {
                for (size_type i = 0; i < count; ++i) {
                    _construct(data + i, value);
                }
            });
        }

        // A constructor's loop: an element that throws takes the ones
        // constructed before it with it (the destructor will not run).
        template<class F>
        void _guarded(F fill) {
            try {
                fill();
            } catch (...) {
                _destroy_range(_data(), _size);
                _size = 0;
                throw;
            }
        }

        template<std::input_iterator InputIt>
        void _construct_range(InputIt first, InputIt last) {
            if constexpr(std::forward_iterator<InputIt>) {
                auto count = (size_type)std::distance(first, last);
                if (!count) {
                    return;
                }
                _check_size(count);
                auto data = _allocate(count);
                _guarded([&] {
                    for (size_type i = 0; i < count; ++i, ++first) {
                        _construct(data + i, *first);
                    }
                });
            } else {
                for (; first != last; ++first) {
                    emplace_back(*first);
                }
            }
        }
    };

    template<std::input_iterator InputIt>
    vector(InputIt, InputIt) -> vector<std::iter_value_t<InputIt>>;

    // unique_ptr owns its object and needs no tracing: a plain std::vector.
    template<typename T>
    class vector<unique_ptr<T>> : public std::vector<unique_ptr<T>> {
    public:
        using std::vector<unique_ptr<T>>::vector;
        using std::vector<unique_ptr<T>>::operator=;
    };
}

namespace std {
    template<class T, class U>
    size_t erase(sgcl::vector<T>& v, const U& value) {
        auto it = std::remove(v.begin(), v.end(), value);
        auto n = (size_t)(v.end() - it);
        v.erase(it, v.end());
        return n;
    }

    template<class T, class Pred>
    size_t erase_if(sgcl::vector<T>& v, Pred pred) {
        auto it = std::remove_if(v.begin(), v.end(), pred);
        auto n = (size_t)(v.end() - it);
        v.erase(it, v.end());
        return n;
    }
}
