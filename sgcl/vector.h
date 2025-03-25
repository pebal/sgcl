//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "collector.h"
#include "make_tracked.h"
#include "tracked_ptr.h"

#include <vector>

namespace sgcl {
    template<class T>
    class vector {
        template<class U>
        class Iterator {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = U;
            using difference_type = ptrdiff_t;
            using pointer = U*;
            using const_pointer = const U*;
            using reference = U&;
            using const_reference = const U&;
            using reverse_iterator = std::reverse_iterator<Iterator>;

            Iterator() noexcept
            : _ptr(nullptr) {
            }

            Iterator(pointer ptr) noexcept
            : _ptr(ptr) {
            }

            reference operator*() const noexcept {
                return *_ptr;
            }

            pointer operator->() const noexcept {
                return _ptr;
            }

            Iterator& operator++() noexcept {
                ++_ptr;
                return *this;
            }

            Iterator operator++(int) noexcept {
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            Iterator& operator--() noexcept {
                --_ptr;
                return *this;
            }

            Iterator operator--(int) noexcept {
                Iterator tmp = *this;
                --(*this);
                return tmp;
            }

            Iterator operator+(difference_type n) const noexcept {
                return Iterator(_ptr + n);
            }

            Iterator operator-(difference_type n) const noexcept {
                return Iterator(_ptr - n);
            }

            Iterator& operator+=(difference_type n) noexcept {
                _ptr += n;
                return *this;
            }

            Iterator& operator-=(difference_type n) noexcept {
                _ptr -= n;
                return *this;
            }

            difference_type operator-(const Iterator& other) const noexcept {
                return _ptr - other._ptr;
            }

            bool operator==(const Iterator<std::remove_cv_t<T>>& other) const noexcept {
                return (_ptr <=> other._ptr) == 0;
            }

            std::strong_ordering operator<=>(const Iterator<std::remove_cv_t<T>>& other) const noexcept {
                return (_ptr <=> other._ptr);
            }

            bool operator==(const Iterator<const std::remove_cv_t<T>>& other) const noexcept {
                return (_ptr <=> other._ptr) == 0;
            }

            std::strong_ordering operator<=>(const Iterator<const std::remove_cv_t<T>>& other) const noexcept {
                return (_ptr <=> other._ptr);
            }

            reverse_iterator make_reverse_iterator() const noexcept {
                return reverse_iterator(*this);
            }

            operator Iterator<const value_type>() const noexcept {
                return Iterator<const value_type>(_ptr);
            }

        private:
            pointer _ptr;

            template<class> friend class Iterator;
            template<class> friend class vector;
        };

    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using size_type = size_t;
        using iterator = Iterator<value_type>;
        using const_iterator = Iterator<const value_type>;
        using reverse_iterator = typename iterator::reverse_iterator;
        using const_reverse_iterator = typename const_iterator::reverse_iterator;
        using difference_type = std::ptrdiff_t;

        vector() noexcept
        : _size(nullptr)
        , _capacity(nullptr) {
        }

        explicit vector(size_t count)
        : vector() {
            if (count) {
                _make(count, T());
            }
        }

        vector(size_t n, const T& value)
        : vector() {
            assign(n, value);
        }

        vector(std::initializer_list<T> ilist)
        : vector() {
            assign(ilist);
        }

        template<std::input_iterator InputIt>
        vector(InputIt first, InputIt last)
        : vector() {
            assign(first, last);
        }

        vector(const vector& other)
        : vector(other.begin(), other.end()) {
        }

        vector(vector&& other) noexcept
        : _ptr(std::move(other._ptr))
        , _size(other._size)
        , _capacity(other._capacity) {
            other._ptr = nullptr;
            other._size = nullptr;
            other._capacity = nullptr;
        }

        vector& operator=(const vector& other) {
            if (this != &other) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        vector& operator=(vector&& other) noexcept {
            if (this != &other) {
                clear();
                _ptr = other._ptr;
                _size = other._size;
                _capacity = other._capacity;
                other._ptr = nullptr;
                other._size = nullptr;
                other._capacity = nullptr;
            }
            return *this;
        }

        vector& operator=(std::initializer_list<T> ilist) {
            assign(ilist);
            return *this;
        }

        reference operator[](size_t pos) noexcept {
            return _ptr.get()[pos];
        }

        const_reference operator[](size_t pos) const noexcept {
            return _ptr.get()[pos];
        }

        reference at(size_t pos) {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::vector");
            }
            return _ptr.get()[pos];
        }

        const_reference at(size_t pos) const {
            if (pos >= size()) {
                throw std::out_of_range("sgcl::vector");
            }
            return _ptr.get()[pos];
        }

        T* data() noexcept {
            return _ptr.get();
        }

        const T* data() const noexcept {
            return _ptr.get();
        }

        void assign(size_t n, const T& value) {
            if (n) {
                if (capacity() < n) {
                    clear();
                    _make(n, value);
                } else {
                    size_t i = 0;
                    auto ptr = begin();
                    auto s = size();
                    for (;i < s && i < n; ++i) {
                        *(ptr + i) = value;
                    }
                    auto c = capacity();
                    for (;i < c && i < n; ++i) {
                        _construct(ptr + i, value);
                    }
                    if (size() > n) {
                        _destruct(ptr + n, end());
                    }
                }
            } else {
                clear();
            }
        }

        template<std::input_iterator InputIt>
        void assign(InputIt first, InputIt last) {
            if (first != last) {
                size_t n = _distance(first, last);
                if (n) {
                    if (capacity() < n) {
                        clear();
                        _allocate(n);
                        auto ptr = _ptr.get();
                        for (size_t i = 0; i < n; ++i) {
                            _construct(ptr + i, *first);
                            ++first;
                        }
                    } else {
                        size_t i = 0;
                        auto ptr = _ptr.get();
                        auto s = size();
                        for (;i < s && i < n; ++i) {
                            *(ptr + i) = *first;
                            ++first;
                        }
                        auto c = capacity();
                        for (;i < c && i < n; ++i) {
                            _construct(ptr + i, *first);
                            ++first;
                        }
                        if (size() > n) {
                            _destruct(ptr + n, end());
                        }
                    }
                    return;
                }
            }
            clear();
        }

        void assign(std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        }

        T& front() noexcept {
            return *_ptr.get();
        }

        const T& front() const noexcept {
            return *_ptr.get();
        }

        T& back() noexcept {
            return *(_ptr.get() + size() - 1);
        }

        const T& back() const noexcept {
            return *(_ptr.get() + size() - 1);
        }

        iterator begin() noexcept {
            return iterator(_ptr.get());
        }

        const_iterator begin() const noexcept {
            return cbegin();
        }

        iterator end() noexcept {
            return begin() + size();
        }

        const_iterator end() const noexcept {
            return cend();
        }

        const_iterator cbegin() const noexcept {
            return const_iterator(_ptr.get());
        }

        const_iterator cend() const noexcept {
            return cbegin() + size();
        }

        reverse_iterator rbegin() noexcept {
            return end().make_reverse_iterator();
        }

        const_reverse_iterator rbegin() const noexcept {
            return crbegin();
        }

        reverse_iterator rend() noexcept {
            return begin().make_reverse_iterator();
        }

        const_reverse_iterator rend() const noexcept {
            return crend();
        }

        const_reverse_iterator crbegin() const noexcept {
            return cend().make_reverse_iterator();
        }

        const_reverse_iterator crend() const noexcept {
            return cbegin().make_reverse_iterator();
        }

        bool empty() const noexcept {
            return size() == 0;
        }

        size_t size() const noexcept {
            return _size ? *_size : 0;
        }

        size_t capacity() const noexcept {
            return _capacity ? *_capacity : 0;
        }

        size_type max_size() const noexcept {
            return std::numeric_limits<ptrdiff_t>::max();
        }

        void push_back(const T& value) {
            if (!capacity()) {
                _make(1, value);
            } else {
                if (size() >= capacity()) {
                    _resize();
                }
                _construct(end(), value);
            }
        }

        void push_back(T&& value) {
            if (!capacity()) {
                _make(1, std::move(value));
            } else {
                if (size() >= capacity()) {
                    _resize();
                }
                _construct(end(), std::move(value));
            }
        }

        template<class ...A>
        iterator emplace_back(A&&... a) {
            if (!capacity()) {
                _make(1, std::forward<A>(a)...);
            } else {
                if (size() >= capacity()) {
                    _resize();
                }
                _construct(end(), std::forward<A>(a)...);
            }
            return begin() + (size() - 1);
        }

        void reserve(size_t capacity) {
            _reserve(capacity);
        }

        void pop_back() {
            _destruct(end() - 1);
        }

        template<class... A>
        iterator emplace(const_iterator pos, A&&... a) {
            if (pos == end()) {
                return emplace_back(std::forward<A>(a)...);
            } else {
                auto index = pos - begin();
                _move(pos, 1);
                _construct(begin() + index, std::forward<A>(a)...);
                return begin() + index;
            }
        }

        iterator insert(const_iterator pos, const T& value) {
            return emplace(pos, value);
        }

        iterator insert(const_iterator pos, T&& value) {
            return emplace(pos, std::move(value));
        }

        iterator insert(const_iterator pos, size_t count, const T& value) {
            auto index = pos - begin();
            if (!count) {
                return iterator(begin() + index);
            }
            if (pos < end()) {
                _move(pos, count);
            } else {
                _resize(size() + count);
            }
            auto ptr = _ptr.get() + index;
            for (size_t i = 0; i < count; ++i) {
                _construct(ptr + i, value);
            }
            return iterator(ptr);
        }

        template<std::input_iterator InputIt>
        iterator insert(const_iterator pos, InputIt first, InputIt last) {
            auto index = pos - begin();
            if (first == last) {
                return iterator(begin() + index);
            }
            size_t count = _distance(first, last);
            if (pos < end()) {
                _move(pos, count);
            } else {
                _resize(size() + count);
            }
            auto ptr = _ptr.get() + index;
            for (size_t i = 0; i < count; ++i) {
                _construct(ptr + i, *first);
                ++first;
            }
            return iterator(ptr);
        }

        iterator insert(const_iterator pos, std::initializer_list<T> ilist) {
            return insert(pos, ilist.begin(), ilist.end());
        }

        iterator erase(const_iterator pos) noexcept {
            auto index = pos - begin();
            if (pos < end()) {
                _move(pos, -1);
            }
            return iterator(begin() + index);
        }

        iterator erase(const_iterator first, const_iterator last) noexcept {
            auto count = last - first;
            auto index = first - begin();
            if (first < end()) {
                _move(first, -count);
            }
            return iterator(begin() + index);
        }

        void resize(size_type count) {
            resize(count, T());
        }

        void resize(size_type count, const value_type& value) {
            if (count < size()) {
                _destruct(begin() + count, end());
            } else if (count > size()) {
                insert(end(), count - size(), value);
            }
        }

        void swap(vector& other) noexcept {
            _ptr.swap(other._ptr);
            std::swap(_size, other._size);
            std::swap(_capacity, other._capacity);
        }

        void clear() noexcept {
            if (_ptr) {
                _destruct(begin(), end());
            }
        }

    private:
        tracked_ptr<T[]> _ptr;
        size_t* _size;
        const size_t* _capacity;

        template<std::input_iterator InputIt>
        size_t _distance(InputIt first, InputIt last) const noexcept {
            auto distance = std::distance(first, last);
            return distance > 0 ? distance : 0;
        }

        void _allocate(size_t capacity) {
            _ptr = (unique_ptr<T[]>)(detail::Maker<T[]>::make_tracked_data(capacity));
            _size = detail::Pointer::size_ptr(_ptr.get());
            _capacity = detail::Pointer::capacity_ptr(_ptr.get());
        }

        template<class ...A>
        void _make(size_t n, A&&... a) {
            _ptr = make_tracked<T[]>(n, std::forward<A>(a)...);
            _size = detail::Pointer::size_ptr(_ptr.get());
            _capacity = detail::Pointer::capacity_ptr(_ptr.get());
        }

        template<class ...A>
        inline void _construct(iterator i, A&&... a) {
            auto ptr = i._ptr;
            detail::Maker<T>::construct(ptr, std::forward<A>(a)...);
            ++(*_size);
        }

        inline void _destruct(iterator i) noexcept(std::is_nothrow_destructible_v<T>) {
            auto ptr = i._ptr;
            --(*_size);
            if constexpr(!std::is_trivially_destructible_v<T> && std::is_destructible_v<T>) {
                detail::Maker<T>::destroy(ptr);
            }
        }

        inline void _destruct(iterator first, iterator last) noexcept(std::is_nothrow_destructible_v<T>) {
            auto ptr = first._ptr;
            size_t n = last - first;
            *_size -= n;
            if constexpr(!std::is_trivially_destructible_v<T> && std::is_destructible_v<T>) {
                for (size_t i = n; i > 0; --i) {
                    detail::Maker<T>::destroy(ptr + i - 1);
                }
            }
        }

        void _resize(size_t capacity = 0, size_t pos = 0, size_t offset = 0) {
            auto min_capacity = std::max(this->capacity() * 3 / 2, this->capacity() + 1);
            capacity = std::max(capacity, min_capacity);
            auto lock = _ptr;
            auto s = size();
            auto r = begin();
            _allocate(capacity);
            auto l = begin();
            size_t o = 0;
            for (size_t i = 0; i < s; ++i) {
                if (i == pos) {
                    o = offset;
                }
                _construct(l + i + o, std::move(*(r + i)));
            }
        }

        void _reserve(size_t capacity) {
            if (capacity <= this->capacity()) {
                return;
            }
            auto lock = _ptr;
            auto s = size();
            auto r = begin();
            _allocate(capacity);
            auto l = begin();
            for (size_t i = 0; i < s; ++i) {
                _construct(l + i, std::move(*(r + i)));
            }
        }

        void _move(const_iterator it, ptrdiff_t offset) {
            size_t pos = it - begin();
            auto s = size();
            if (!s) {
                return;
            }
            auto ptr = begin();
            if (offset > 0) {
                if (s + offset <= capacity()) {
                    for (size_t i = s; i > pos; --i) {
                        auto src = ptr + i - 1;
                        auto dst = src + offset;
                        if (i - 1 + offset >= s) {
                            _construct(dst, std::move(*src));
                        } else {
                            *dst = std::move(*src);
                        }
                    }
                    _destruct(ptr + pos, ptr + pos + offset);
                } else {
                    _resize(s + offset, pos, offset);
                }
            } else {
                for (size_t i = pos; i < s + offset; ++i) {
                    auto dst = ptr + i;
                    auto src = dst - offset;
                    *dst = std::move(*src);
                }
                _destruct(ptr + s + offset, ptr + s);
            }
        }
    };

    template<typename T>
    class vector<unique_ptr<T>> : public std::vector<unique_ptr<T>> {
    public:
        using std::vector<unique_ptr<T>>::vector;
    };
}
