//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "mixin/mixin.h"
#include "tracked_ptr.h"

#include <array>
#include <cassert>
#include <compare>
#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sgcl {
    namespace detail {
        // A character type: slice<const CharT> gets the text interface
        template<class T>
        inline constexpr bool IsCharacter = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

        struct NoText {};

        // Chosen by a specialization, not by conditional_t, so that
        // mixin::text<Derived, byte> is never named: its default char_traits
        // argument would instantiate the deprecated char_traits<byte>
        template<class T, class Derived, bool = IsCharacter<std::remove_const_t<T>>>
        struct SliceBaseOf { using type = NoText; };
        template<class T, class Derived>
        struct SliceBaseOf<T, Derived, true> { using type = mixin::text<Derived, std::remove_const_t<T>>; };

        template<class T, class Derived>
        using SliceBase = typename SliceBaseOf<T, Derived>::type;

        // mixin::sequence only where the elements can be written: slice<T>,
        // not slice<const T> (the constness of T is the slice's, known here)
        template<class T, class Derived>
        using SliceWriteBase = MixinIf<!std::is_const_v<T>, mixin::sequence, Derived>;

        // The std view of the characters of a text slice; for a slice of
        // anything else a type that no argument converts to
        struct NoView { NoView() = delete; };

        template<class T, bool = IsCharacter<std::remove_const_t<T>>>
        struct TextViewOf { using type = NoView; };
        template<class T>
        struct TextViewOf<T, true> { using type = std::basic_string_view<std::remove_const_t<T>>; };

        template<class T>
        using TextView = typename TextViewOf<T>::type;

        // The std string of the characters, for the same reason
        template<class T, bool = IsCharacter<std::remove_const_t<T>>>
        struct TextStringOf { using type = NoView; };
        template<class T>
        struct TextStringOf<T, true> { using type = std::basic_string<std::remove_const_t<T>>; };

        template<class T>
        using TextString = typename TextStringOf<T>::type;
    }

    // A slice: the elements [begin, end) of some contiguous storage, and
    // the managed object they lie in, kept alive by the slice for as
    // long as the slice exists — what a slice is in Go (a piece of the
    // array that shares it and holds it), and what std::span and
    // std::string_view are not (a range with no duty to keep its memory).
    // Three words: the owner, a tracked_ptr to the object (the string,
    // the buffer of a vector, the block of a reader), and two raw
    // pointers into it. A slice of unmanaged memory (a stack array, a
    // std::vector, a std::span) has no owner: the word is null and the
    // slice promises what a span does, the memory valid for the call.
    // Which of the two a slice is follows from where the memory comes
    // from, not from a choice: a string, a vector, a reader's block hand
    // out slices with the owner set; a raw pointer or a std container
    // give one without. The owner is given explicitly by whoever knows
    // it (the constructor with the owner): a pointer into an object
    // finds the object only within its first page, so nothing is guessed
    // from an address.
    //
    // A slice without an owner costs what a span costs: three word
    // stores, no barrier, no registration of the thread (a null roots
    // nothing). A slice with an owner is a tracked_ptr's copy: the
    // barrier and, the first time on a thread, the registration of its
    // stack. The rule the two paths keep: a non-null owner never lands
    // on a stack the collector does not know (the constructor from an
    // owner, the copy and the assignment from an owned slice register).
    // A slice lives where a tracked_ptr may: on a stack or in a managed
    // object. The elements are T: slice<const char> is text (with the
    // operations of std::string_view, mixin::text), slice<std::byte> a buffer
    // to read into, slice<const std::byte> data to write. A slice is a
    // range of the library (mixin::enumerable and the rest, mixin/): what a
    // vector can be asked, a slice of it can be asked too, and sorted in
    // place when its elements are not const.
    template<class T>
    class slice
    : public detail::SliceBase<T, slice<T>>
    , public mixin::bidirectional<slice<T>>
    , public mixin::comparable<slice<T>>
    , public mixin::contiguous<slice<T>>
    , public mixin::enumerable<slice<T>>
    , public mixin::equatable<slice<T>>
    , public mixin::ordered<slice<T>>
    , public mixin::random_access<slice<T>>
    , public detail::SliceWriteBase<T, slice<T>> {
    public:
        using element_type = T;
        using value_type = std::remove_cv_t<T>;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using iterator = T*;
        using const_iterator = const T*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        static constexpr size_type npos = size_type(-1);

        // Empty, no owner
        slice() noexcept
        : _object(nullptr, detail::unregistered) {
        }

        // The elements [first, last) or [first, first + n) of unmanaged
        // memory: no owner
        slice(T* first, T* last) noexcept
        : _object(nullptr, detail::unregistered)
        , _begin(first)
        , _end(last) {
        }

        slice(T* first, size_type n) noexcept
        : slice(first, first + n) {
        }

        // The elements [first, last) of the managed object `owner`, which
        // the slice holds
        slice(const tracked_ptr<const void>& owner, T* first, T* last) noexcept
        : _object(owner)
        , _begin(first)
        , _end(last) {
            assert(_within_owner() && "the elements of a slice lie in its owner");
        }

        slice(const tracked_ptr<const void>& owner, T* first, size_type n) noexcept
        : slice(owner, first, first + n) {
        }

        // From the std containers and views: no owner
        slice(std::span<T> s) noexcept
        : slice(s.data(), s.data() + s.size()) {
        }

        template<size_t N>
        slice(T (&a)[N]) noexcept
        : slice(a, a + N) {
        }

        template<class U, size_t N>
        requires std::is_convertible_v<U (*)[], T (*)[]>
        slice(std::array<U, N>& a) noexcept
        : slice(a.data(), a.data() + N) {
        }

        template<class U, size_t N>
        requires std::is_convertible_v<const U (*)[], T (*)[]>
        slice(const std::array<U, N>& a) noexcept
        : slice(a.data(), a.data() + N) {
        }

        template<class U, class A>
        requires std::is_convertible_v<U (*)[], T (*)[]>
        slice(std::vector<U, A>& v) noexcept
        : slice(v.data(), v.data() + v.size()) {
        }

        template<class U, class A>
        requires std::is_convertible_v<const U (*)[], T (*)[]>
        slice(const std::vector<U, A>& v) noexcept
        : slice(v.data(), v.data() + v.size()) {
        }

        template<class Traits>
        requires std::is_convertible_v<const std::remove_const_t<T> (*)[], T (*)[]>
        slice(std::basic_string_view<std::remove_const_t<T>, Traits> s) noexcept
        : slice(s.data(), s.data() + s.size()) {
        }

        // A slice of T from a slice of a type that converts (T* from U*: a
        // const from a mutable), the owner carried over
        template<class U>
        requires (!std::is_same_v<U, T>) && std::is_convertible_v<U (*)[], T (*)[]>
        slice(const slice<U>& o) noexcept
        : slice(o.owner(), o.data(), o.data() + o.size()) {
        }

        // The copy: a null owner without the registration, an owner
        // through tracked_ptr's copy (the registration, the barrier)
        slice(const slice& o) noexcept
        : _object(o._object ? tracked_ptr<const void>(o._object) : tracked_ptr<const void>(nullptr, detail::unregistered))
        , _begin(o._begin)
        , _end(o._end) {
        }

        slice(slice&& o) noexcept
        : slice(static_cast<const slice&>(o)) {
        }

        // The assignment: a non-null owner arriving on this stack has the
        // thread registered first, as a constructor would
        slice& operator=(const slice& o) noexcept {
            if (o._object) {
                detail::ensure_thread_registered();
            }
            _object = o._object;
            _begin = o._begin;
            _end = o._end;
            return *this;
        }

        slice& operator=(slice&& o) noexcept {
            return *this = static_cast<const slice&>(o);
        }

        // The two raw words nulled: inside a managed object the pointer
        // map takes any word that only ever held addresses for a pointer
        // and traces it, so a dead slice's begin and end, left as they
        // were in a container's slot, would keep the object they point
        // into alive (an interior address within its first page)
        ~slice() noexcept {
            *(T* volatile*)&_begin = nullptr;
            *(T* volatile*)&_end = nullptr;
        }

        // The object the elements lie in, null for unmanaged memory
        const tracked_ptr<const void>& owner() const noexcept {
            return _object;
        }

        bool owned() const noexcept {
            return _object != nullptr;
        }

        T* data() const noexcept {
            return _begin;
        }

        size_type size() const noexcept {
            return size_type(_end - _begin);
        }

        size_type size_bytes() const noexcept {
            return size() * sizeof(T);
        }

        // Itself: what every container with a buffer answers, so that
        // generic code asks one question
        slice as_slice() const noexcept {
            return *this;
        }

        bool empty() const noexcept {
            return _begin == _end;
        }

        T& operator[](size_type i) const noexcept {
            assert(i < size());
            return _begin[i];
        }

        T& front() const noexcept {
            assert(!empty());
            return *_begin;
        }

        T& back() const noexcept {
            assert(!empty());
            return _end[-1];
        }

        iterator begin() const noexcept { return _begin; }
        iterator end() const noexcept { return _end; }
        const_iterator cbegin() const noexcept { return _begin; }
        const_iterator cend() const noexcept { return _end; }
        reverse_iterator rbegin() const noexcept { return reverse_iterator(_end); }
        reverse_iterator rend() const noexcept { return reverse_iterator(_begin); }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        // The elements [pos, pos + n) as a slice of the same owner;
        // std::span's names for the same
        slice subslice(size_type pos, size_type n = npos) const {
            if (pos > size()) {
                throw std::out_of_range("sgcl::slice::subslice");
            }
            return slice(_object, _begin + pos, _begin + pos + std::min(n, size() - pos), Unchecked{});
        }

        slice subspan(size_type pos, size_type n = npos) const {
            return subslice(pos, n);
        }

        slice first(size_type n) const noexcept {
            assert(n <= size());
            return slice(_object, _begin, _begin + n, Unchecked{});
        }

        slice last(size_type n) const noexcept {
            assert(n <= size());
            return slice(_object, _end - n, _end, Unchecked{});
        }

        // The slice narrowed in place, as std::string_view's
        void remove_prefix(size_type n) noexcept {
            assert(n <= size());
            _begin += n;
        }

        void remove_suffix(size_type n) noexcept {
            assert(n <= size());
            _end -= n;
        }

        // For a std interface: the range without the owner
        operator std::span<T>() const noexcept {
            return std::span<T>(_begin, _end);
        }

        void swap(slice& o) noexcept {
            slice t = *this;
            *this = o;
            o = t;
        }

        // Text (slice<const CharT>): the slice without the characters of
        // `chars` (white space by default) at both ends, at the start, at
        // the end; without a prefix or a suffix when it is there — each
        // a slice of the same owner, nothing copied
        slice trim() const noexcept requires detail::IsCharacter<value_type> {
            auto from = this->_find_space(0, false);
            if (from == npos) {
                return slice(_object, _begin, _begin, Unchecked{});
            }
            return slice(_object, _begin + from, _begin + this->_end_without_spaces(), Unchecked{});
        }

        slice trim(detail::TextView<T> chars) const noexcept requires detail::IsCharacter<value_type> {
            auto v = this->view();
            auto from = v.find_first_not_of(chars);
            if (from == npos) {
                return slice(_object, _begin, _begin, Unchecked{});
            }
            auto to = v.find_last_not_of(chars) + 1;
            return slice(_object, _begin + from, _begin + to, Unchecked{});
        }

        slice trim_left() const noexcept requires detail::IsCharacter<value_type> {
            auto from = this->_find_space(0, false);
            return from == npos ? slice(_object, _begin, _begin, Unchecked{}) : slice(_object, _begin + from, _end, Unchecked{});
        }

        slice trim_left(detail::TextView<T> chars) const noexcept requires detail::IsCharacter<value_type> {
            auto from = this->view().find_first_not_of(chars);
            return from == npos ? slice(_object, _begin, _begin, Unchecked{}) : slice(_object, _begin + from, _end, Unchecked{});
        }

        slice trim_right() const noexcept requires detail::IsCharacter<value_type> {
            return slice(_object, _begin, _begin + this->_end_without_spaces(), Unchecked{});
        }

        slice trim_right(detail::TextView<T> chars) const noexcept requires detail::IsCharacter<value_type> {
            auto to = this->view().find_last_not_of(chars);
            return to == npos ? slice(_object, _begin, _begin, Unchecked{}) : slice(_object, _begin, _begin + to + 1, Unchecked{});
        }

        // The characters to trim as code points: trim(U"«»")
        slice trim(std::u32string_view set) const noexcept requires (detail::IsCharacter<value_type> && sizeof(value_type) == 1) {
            auto from = this->find_first_not_of(set);
            if (from == npos) {
                return slice(_object, _begin, _begin, Unchecked{});
            }
            auto last = this->find_last_not_of(set);
            return slice(_object, _begin + from, _begin + last + this->decode(last).second, Unchecked{});
        }

        slice trim_left(std::u32string_view set) const noexcept requires (detail::IsCharacter<value_type> && sizeof(value_type) == 1) {
            auto from = this->find_first_not_of(set);
            return from == npos ? slice(_object, _begin, _begin, Unchecked{}) : slice(_object, _begin + from, _end, Unchecked{});
        }

        slice trim_right(std::u32string_view set) const noexcept requires (detail::IsCharacter<value_type> && sizeof(value_type) == 1) {
            auto last = this->find_last_not_of(set);
            return last == npos ? slice(_object, _begin, _begin, Unchecked{}) : slice(_object, _begin, _begin + last + this->decode(last).second, Unchecked{});
        }

        slice trim_prefix(detail::TextView<T> prefix) const noexcept requires detail::IsCharacter<value_type> {
            return this->starts_with(prefix) ? slice(_object, _begin + prefix.size(), _end, Unchecked{}) : *this;
        }

        slice trim_suffix(detail::TextView<T> suffix) const noexcept requires detail::IsCharacter<value_type> {
            return this->ends_with(suffix) ? slice(_object, _begin, _end - suffix.size(), Unchecked{}) : *this;
        }

        // substr: std::string_view's name for subslice, on text
        slice substr(size_type pos = 0, size_type n = npos) const requires detail::IsCharacter<value_type> {
            return subslice(pos, n);
        }

        // contains: a name in two bases (mixin::text's, of a substring or a
        // character; mixin::enumerable's, of an element) is ambiguous, so the
        // slice says which — the text's for text, the element's otherwise
        bool contains(detail::TextView<T> s) const noexcept requires detail::IsCharacter<value_type> {
            return detail::SliceBase<T, slice>::contains(s);
        }

        bool contains(value_type c) const noexcept requires detail::IsCharacter<value_type> {
            return detail::SliceBase<T, slice>::contains(c);
        }

        bool contains(char32_t c) const noexcept requires (detail::IsCharacter<value_type> && sizeof(value_type) == 1) {
            return detail::SliceBase<T, slice>::contains(c);
        }

        bool contains(std::same_as<int> auto) const requires detail::IsCharacter<value_type> = delete;   // 'ż' is an int: write U'ż' (a template: slice<int> has contains(int) already)

        bool contains(const auto& value) const requires (!detail::IsCharacter<value_type>) && detail::EquatableElements<slice> {
            return mixin::enumerable<slice>::contains(value);
        }

    private:
        struct Unchecked {};

        slice(const tracked_ptr<const void>& owner, T* first, T* last, Unchecked) noexcept
        : _object(owner ? tracked_ptr<const void>(owner) : tracked_ptr<const void>(nullptr, detail::unregistered))
        , _begin(first)
        , _end(last) {
        }

        // Whether [begin, end) lies in the owner (debug): the owner's
        // object from its page, its size from the page's metadata
        bool _within_owner() const noexcept {
            if (!_object) {
                return true;
            }
            auto page = detail::Page::page_of(_object.get());
            auto base = static_cast<const char*>(page->pointer_of(page->index_of(_object.get())));
            auto end = page->is_array ? reinterpret_cast<const char*>(page->data) + page->data_size() : base + page->object_size;   // a buffer or a large object: the rest of its pages
            auto first = reinterpret_cast<const char*>(_begin);
            auto last = reinterpret_cast<const char*>(_end);
            return first >= base && last <= end && first <= last;
        }

        tracked_ptr<const void> _object;
        T* _begin = nullptr;
        T* _end = nullptr;
    };

    template<class T>
    slice(T*, T*) -> slice<T>;

    template<class T>
    slice(T*, size_t) -> slice<T>;

    template<class T, size_t N>
    slice(T (&)[N]) -> slice<T>;

    template<class T, size_t N>
    slice(std::array<T, N>&) -> slice<T>;

    template<class T, size_t N>
    slice(const std::array<T, N>&) -> slice<const T>;

    template<class T, class A>
    slice(std::vector<T, A>&) -> slice<T>;

    template<class T, class A>
    slice(const std::vector<T, A>&) -> slice<const T>;

    template<class T>
    slice(std::span<T>) -> slice<T>;

    // The bytes of a slice, as std::as_bytes
    template<class T>
    slice<const std::byte> as_bytes(const slice<T>& s) noexcept {
        return slice<const std::byte>(s.owner(), reinterpret_cast<const std::byte*>(s.data()), s.size_bytes());
    }

    template<class T>
    requires (!std::is_const_v<T>)
    slice<std::byte> as_writable_bytes(const slice<T>& s) noexcept {
        return slice<std::byte>(s.owner(), reinterpret_cast<std::byte*>(s.data()), s.size_bytes());
    }

    // runes: the code points of a UTF-8 text, decoded as they are walked —
    // what runes() of a string or a text slice returns, Go's
    // `for i, r := range s`. A forward range of char32_t over a slice of
    // the text, which holds its object for as long as the range lives
    // (`for (char32_t c : string("żółw").runes())` is safe); an invalid
    // byte is one code point, utf8::replacement. The iterator knows the
    // byte position of its code point (pos()) and its width (width()),
    // for the code that goes back to the bytes. A range of the library:
    // mixin::enumerable, so `s.runes().contains(U'ż')`,
    // `s.runes().count_of(unicode::is_upper)`, `s.runes().find_if(...)`.
    class runes
    : public mixin::enumerable<runes> {
    public:
        using value_type = char32_t;
        using size_type = size_t;

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = char32_t;
            using difference_type = ptrdiff_t;
            using reference = char32_t;
            using pointer = void;

            iterator() noexcept = default;

            char32_t operator*() const noexcept {
                return _c;
            }

            iterator& operator++() noexcept {
                _pos += _n;
                _decode();
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator t = *this;
                ++*this;
                return t;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._pos == b._pos;
            }

            // The byte position of the code point in the text, and its width in bytes
            size_t pos() const noexcept {
                return _pos;
            }

            size_t width() const noexcept {
                return _n;
            }

        private:
            friend class runes;

            iterator(std::string_view text, size_t pos) noexcept
            : _text(text)
            , _pos(pos) {
                _decode();
            }

            void _decode() noexcept {
                auto [c, n] = utf8::decode(_text, _pos);
                _c = c;
                _n = n;
            }

            std::string_view _text;
            size_t _pos = 0;
            size_t _n = 0;
            char32_t _c = 0;
        };

        using const_iterator = iterator;

        runes() noexcept = default;

        explicit runes(slice<const char> text) noexcept
        : _text(text) {
        }

        iterator begin() const noexcept {
            return iterator(_text.view(), 0);
        }

        iterator end() const noexcept {
            return iterator(_text.view(), _text.size());
        }

        bool empty() const noexcept {
            return _text.empty();
        }

        // The code points: walked and counted, not stored
        size_type count() const noexcept {
            return utf8::count(_text.view());
        }

        // The text the range walks
        const slice<const char>& text() const noexcept {
            return _text;
        }

    private:
        slice<const char> _text;
    };
}

// runes(): declared in the text mixin, defined here where runes and slice
// are complete; a string's as_slice() and a slice's as_slice() hand the
// text over with its object
template<class Derived, class CharT, class Traits>
sgcl::runes sgcl::mixin::text<Derived, CharT, Traits>::runes() const noexcept requires (sizeof(CharT) == 1) {
    auto s = _self().as_slice();
    return sgcl::runes(sgcl::slice<const char>(s.owner(), reinterpret_cast<const char*>(s.data()), s.size()));
}

// The hash of a text slice: the hash a string of the same characters
// has (string.h: std::hash<basic_string>, transparent), defined there
