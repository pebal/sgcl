//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../vector.h"
#include "../../core/aliases.h"
#include "../../core/mixin/mixin.h"
#include "../../core/make_tracked.h"
#include "../../core/tracked_ptr.h"
#include "../../core/unique_ptr.h"

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace sgcl::im {
    namespace detail {
        // A cell of list: the element and the rest of the list. Never
        // modified once a list holds it: a push_front is a new cell in
        // front of the old first one, and every cell behind is shared
        // with every list that reaches it. A type of its own (its pool),
        // made by make() only, so that a cell is always a managed object.
        template<class T>
        struct ListCell {
            T value;
            tracked_ptr<ListCell> next;

            template<class... A>
            static unique_ptr<ListCell> make(const tracked_ptr<ListCell>& next, A&&... a) {
                return make_tracked<ListCell>(next, std::forward<A>(a)...);
            }

        private:
            friend class sgcl::detail::MakerBase;

            template<class... A>
            explicit ListCell(const tracked_ptr<ListCell>& n, A&&... a)
            : value(std::forward<A>(a)...)
            , next(n) {
            }
        };
    }

    // The immutable list (the list of Lisp, ML and Elm, Go's container/
    // list being something else): a sequence read from the front, every
    // change of which returns a new list and leaves the old one as it
    // was. A chain of cells, each holding an element and the rest; a
    // push_front is one new cell in front of the old chain, which the
    // new list shares with the old, so the old list stays a list of its
    // own and the two share everything but the first cell: O(1), one
    // allocation, whatever the length. pop_front is the rest of the
    // chain, no allocation at all; front and empty read the first cell.
    // What is not O(1) is not offered: no index, no push_back, no size
    // kept per cell — size() is a word of the list, kept as cells are
    // added and dropped. The tail of a list is a list; a list built from
    // a range holds the elements in the range's order (the cells made
    // from the back).
    //
    // The list is two words: the count and a tracked_ptr to the first
    // cell. It lives where a tracked_ptr may, on a stack or inside a
    // managed object, and a copy of it is a copy of those words. The
    // cells are managed objects that no version owns: a cell reached by
    // ten lists is one cell, collected once the last of them is dropped.
    // Elements are const through the list; an element holding tracked
    // pointers is traced where it lives, in its cell. A list of a
    // million cells is dropped in one sweep, not by a chain of
    // destructors: the cells die together, in no particular order.
    template<class T>
    class list   // read as any range; == its own (a version and its copy by the chain)
    : public mixin::enumerable<list<T>>
    , public mixin::comparable<list<T>>
    , public mixin::ordered<list<T>> {
        using Cell = detail::ListCell<T>;

    public:
        using value_type = T;
        using reference = const T&;
        using const_reference = const T&;
        using pointer = const T*;
        using const_pointer = const T*;
        using size_type = size_t;
        using difference_type = ptrdiff_t;

        // The iterator: a raw pointer to a cell, valid while the list
        // that gave it holds the chain (a cell reached is alive while its
        // list is; a copy of the iterator kept past the list roots
        // nothing). Forward only.
        class const_iterator {
        public:
            using iterator_concept = std::forward_iterator_tag;
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using reference = const T&;
            using pointer = const T*;
            using difference_type = ptrdiff_t;

            const_iterator() noexcept = default;

            reference operator*() const noexcept {
                return _cell->value;
            }

            pointer operator->() const noexcept {
                return &_cell->value;
            }

            const_iterator& operator++() noexcept {
                _cell = _cell->next.get();
                return *this;
            }

            const_iterator operator++(int) noexcept {
                auto t = *this;
                ++*this;
                return t;
            }

            friend bool operator==(const const_iterator& a, const const_iterator& b) noexcept {
                return a._cell == b._cell;
            }

        private:
            friend class list;

            explicit const_iterator(const Cell* cell) noexcept
            : _cell(cell) {
            }

            const Cell* _cell = nullptr;
        };

        using iterator = const_iterator;

        // Empty
        list() noexcept = default;

        // The elements of a range, in its order: the cells made from the
        // back, one allocation each
        template<std::input_iterator InputIt>
        list(InputIt first, InputIt last) {
            if constexpr(std::bidirectional_iterator<InputIt>) {
                for (auto it = last; it != first;) {
                    --it;
                    _push(*it);
                }
            } else {
                sgcl::vector<T> items(first, last);
                for (auto it = items.rbegin(); it != items.rend(); ++it) {
                    _push(std::move(*it));
                }
            }
        }

        list(std::initializer_list<T> ilist)
        : list(ilist.begin(), ilist.end()) {
        }

        // From a range of what the elements are made of (as the mutable
        // containers take one): a list is copied by its own constructor
        template<std::ranges::input_range R>
        requires (!std::is_same_v<std::remove_cvref_t<R>, list>) && std::is_constructible_v<T, std::ranges::range_reference_t<R>>
        explicit list(R&& r)
        : list(std::ranges::begin(r), std::ranges::end(r)) {
        }

        list(const list&) noexcept = default;
        list(list&&) noexcept = default;
        list& operator=(const list&) noexcept = default;
        list& operator=(list&&) noexcept = default;

        const_iterator begin() const noexcept {
            return const_iterator(_head.get());
        }

        const_iterator end() const noexcept {
            return const_iterator();
        }

        const_iterator cbegin() const noexcept {
            return begin();
        }

        const_iterator cend() const noexcept {
            return end();
        }

        size_type size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return _size == 0;
        }

        // The first element: undefined on an empty list, as front() of any sequence
        const_reference front() const noexcept {
            assert(!empty());
            return _head->value;
        }

        // The list with value in front: a new cell, the rest shared
        list push_front(const T& value) const {
            return list(_size + 1, Cell::make(_head, value));
        }

        list push_front(T&& value) const {
            return list(_size + 1, Cell::make(_head, std::move(value)));
        }

        template<class... A>
        list emplace_front(A&&... a) const {
            return list(_size + 1, Cell::make(_head, std::forward<A>(a)...));
        }

        // The list without its first element: the rest of the chain, no
        // allocation; undefined on an empty list
        list pop_front() const noexcept {
            assert(!empty());
            return list(_size - 1, _head->next);
        }

        // The elements in the reverse order: a new chain of every cell
        list reverse() const {
            list r;
            for (auto& v : *this) {
                r._push(v);
            }
            return r;
        }

        // Comparison by the elements, in order; a version and its copy
        // are equal by their chain
        friend bool operator==(const list& a, const list& b) {
            if (a._head == b._head) {
                return true;
            }
            return a._size == b._size && std::equal(a.begin(), a.end(), b.begin());
        }

        friend bool operator!=(const list& a, const list& b) {
            return !(a == b);
        }

    private:
        list(size_t size, tracked_ptr<Cell> head) noexcept
        : _size(size)
        , _head(std::move(head)) {
        }

        // A cell in front, on a list nobody else holds (one being built)
        template<class U>
        void _push(U&& value) {
            _head = Cell::make(_head, std::forward<U>(value));
            ++_size;
        }

        size_t _size = 0;
        tracked_ptr<Cell> _head;
    };

    template<std::input_iterator It>
    list(It, It) -> list<std::iter_value_t<It>>;

}
