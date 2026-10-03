//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "aliases.h"
#include "expected.h"
#include "detail/handle_word.h"
#include "detail/hash_bytes.h"
#include "detail/string_data.h"
#include "mixin/text.h"
#include "slice.h"
#include "tracked_ptr.h"

#include <algorithm>
#include <charconv>
#include <compare>
#include <functional>
#include <iosfwd>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace sgcl {
    // An immutable string on the managed heap, one word: a pointer to an
    // object holding the length, the characters and a terminator, and the
    // hash once asked for, of exactly that size (detail/string_data.h).
    // What a string is in Java and Go rather than in C++: made once,
    // never modified, shared by copying the word, compared and hashed by
    // its contents, reclaimed by the collector, no destructor (the sweep
    // frees the slot and nothing runs), no reference count. Copying one
    // between managed objects costs a word and the barrier, wherever it
    // is long; a std::string past its small buffer costs an allocation
    // per copy and a free per destruction, in the sweep. Creating one
    // costs a managed allocation (README: Allocation), where a
    // std::string of a few characters costs none: a string is for text
    // that is kept, shared and compared, not for a scratch buffer, which
    // std::string remains. No small-string optimization: the word is the
    // whole of it, and the empty string is null.
    // The interface is the read side of std::string (and all of
    // std::string_view, as mixin::text, core/mixin/text.h, shared with
    // slice<const CharT>): size, data, c_str, [], at, front, back, the
    // iterators, compare, starts_with, ends_with, contains, the finds,
    // substr (a new string), the comparisons and <=> with a string, a
    // std view or a literal, operator+ (a new string), std::hash
    // (computed once, kept in the object), operator<<, conversions to a
    // std view and to std::string; the constructors from a literal, a
    // std view, a std::string, a range, (n, ch); no mutation, no capacity.
    // A piece of the string that holds it is a slice (slice.h, as_slice):
    // what split and fields hand out, and what a string is made of again
    // without a copy when the slice is the whole of one. Past
    // std::string, what the strings of Go and Java have, each a new
    // string (or the same object when nothing changes): split and fields
    // (a range of slices, the class pieces), join, trim, trim_left,
    // trim_right, trim_prefix, trim_suffix, replace, repeat, to_lower,
    // to_upper. The word is a tracked_ptr, so a string lives where one
    // may: on a stack or in a managed object.
    // The length is kept in 32 bits: a string holds up to 4 G characters.
    namespace detail {
        // A string over the word of its object, for the library's own
        // structures that keep a string's object by its address (an
        // atomic<string> loads one, an intern pool finds one through a
        // weak pointer): the word is the object's start, as the maker
        // returned it, nothing else
        struct StringAccess {
            template<class S>
            static S over(const tracked_ptr<const void>& word) noexcept {
                return S(word);
            }

            // A string of `n` characters that `fill(CharT* chars)` writes
            // in place, and one of at most `bound` whose fill returns how
            // many it wrote (StringMaker::make_bounded: one object, each
            // character written once): the library's builders that know
            // their size, or a bound of it
            template<class S, class Fill>
            static S filled(size_t n, Fill&& fill) {
                return S::_filled(n, std::forward<Fill>(fill));
            }

            template<class S, class Fill>
            static S bounded(size_t bound, Fill&& fill) {
                return S::_bounded(bound, std::forward<Fill>(fill));
            }

            // The two steps of bounded, for a writing that waits (a read in
            // a task): the object with room for `bound` characters, then
            // the string of the first `used` of them
            template<class S>
            struct Unfilled {
                unique_ptr<void> slot;
                typename S::value_type* chars = nullptr;
                size_t bound = 0;
            };

            template<class S>
            static Unfilled<S> unfilled(size_t bound) {
                Unfilled<S> u;
                u.bound = bound;
                if (bound) {
                    u.slot = StringMaker::make_unfilled<typename S::value_type>(bound, u.chars);
                }
                return u;
            }

            template<class S>
            static S finish(Unfilled<S>&& u, size_t used) noexcept {
                if (!u.bound) {
                    return S();
                }
                return S(StringMaker::finish<typename S::value_type>(std::move(u.slot), u.bound, used));
            }
        };
    }

    template<class CharT, class Traits = std::char_traits<CharT>>
    class basic_string
    : public mixin::text<basic_string<CharT, Traits>, CharT, Traits> {
        static_assert(sizeof(detail::StringHeader) % sizeof(CharT) == 0, "the header is a whole number of characters");

        using Maker = detail::StringMaker;
        using Word = typename Maker::Word;
        static constexpr size_t HeaderChars = sizeof(detail::StringHeader) / sizeof(CharT);

    public:
        using traits_type = Traits;
        using value_type = CharT;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using reference = const CharT&;
        using const_reference = const CharT&;
        using pointer = const CharT*;
        using const_pointer = const CharT*;
        using iterator = const CharT*;
        using const_iterator = const CharT*;
        using reverse_iterator = std::reverse_iterator<const_iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;
        using view_type = std::basic_string_view<CharT, Traits>;
        using slice_type = slice<const CharT>;   // a piece of this string that holds it

        static constexpr size_type npos = view_type::npos;

        class pieces;   // what split and fields return: a range of slices, below

        // The constructors: from what a std::string is made of, the
        // characters copied once into the string's object. The empty
        // string allocates nothing.
        constexpr basic_string() noexcept = default;

        constexpr basic_string(std::nullptr_t) = delete;

        // From an array of characters (a literal): up to its first NUL or
        // its end, whichever comes first — an array filled to the brim has
        // no NUL and is not read past its end. From a pointer (CharT* or
        // const CharT*, no other pointer converts): up to its NUL. Of the
        // pair, the array's overload is the better match, so an array
        // never decays into the pointer's strlen
        template<size_t N>
        basic_string(const CharT (&s)[N])
        : basic_string(detail::array_text<CharT, Traits>(s)) {
        }

        template<class P>
        requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        basic_string(P s)
        : basic_string(view_type(s)) {
        }

        basic_string(const CharT* s, size_type n)
        : basic_string(view_type(s, n)) {
        }

        basic_string(view_type s)
        : _word(s.empty() ? Word() : Maker::make(s)) {
        }

        // From any type a string_view is made of (a std::string, a
        // std::string_view), as std::string does
        template<class V>
        requires std::is_convertible_v<const V&, view_type> && (!std::is_convertible_v<const V&, const CharT*>) && (!std::is_same_v<std::remove_cvref_t<V>, basic_string>)
        explicit basic_string(const V& v)
        : basic_string(view_type(v)) {
        }

        basic_string(size_type n, CharT c)
        : _word(Maker::template make_filled<CharT>(n, [&](CharT* chars) { std::fill_n(chars, n, c); })) {
        }

        template<std::input_iterator It>
        basic_string(It first, It last)
        : basic_string(std::basic_string<CharT, Traits>(first, last)) {
        }

        // A forward range is counted first and written in place
        template<std::forward_iterator It>
        basic_string(It first, It last)
        : _word(Maker::template make_filled<CharT>(size_t(std::distance(first, last)), [&](CharT* chars) { std::copy(first, last, chars); })) {
        }

        basic_string(std::initializer_list<CharT> il)
        : basic_string(view_type(il.begin(), il.size())) {
        }

        // From a slice: the string's own object when the slice is the
        // whole of a string (no copy), a new string of the characters
        // otherwise
        explicit basic_string(const slice_type& v)
        : _word(_whole_string(v) ? Word(static_pointer_cast<const void>(v.owner())) : (v.empty() ? Word() : Maker::make(v.view()))) {
        }

        // From bytes — a slice of bytes, a vector<byte>, an array<byte, N>,
        // whatever converts to slice<const byte> — as the same bytes taken
        // for characters: what a decryption, a decoding or a file read gave
        // back, when it is text. Nothing checks that they are UTF-8
        // (txt::decode with the strict policy does that). Explicit, since
        // bytes are not always text: string text(opened)
        template<class B>
        requires (sizeof(CharT) == 1) && std::is_convertible_v<const B&, slice<const byte>>
              && (!std::is_convertible_v<const B&, view_type>) && (!std::is_convertible_v<const B&, const CharT*>)
              && (!std::is_same_v<std::remove_cvref_t<B>, basic_string>) && (!std::is_same_v<std::remove_cvref_t<B>, slice_type>)
        explicit basic_string(const B& bytes)
        : basic_string(_bytes_view(slice<const byte>(bytes))) {
        }

        basic_string(const basic_string&) noexcept = default;
        basic_string(basic_string&&) noexcept = default;

        basic_string& operator=(const basic_string&) noexcept = default;
        basic_string& operator=(basic_string&&) noexcept = default;

        template<size_t N>
        basic_string& operator=(const CharT (&s)[N]) {
            return *this = basic_string(s);
        }

        template<class P>
        requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        basic_string& operator=(P s) {
            return *this = basic_string(s);
        }

        basic_string& operator=(view_type s) {
            return *this = basic_string(s);
        }

        template<class V>
        requires std::is_convertible_v<const V&, view_type> && (!std::is_convertible_v<const V&, const CharT*>) && (!std::is_same_v<std::remove_cvref_t<V>, basic_string>)
        basic_string& operator=(const V& v) {
            return *this = basic_string(v);
        }

        // The characters, terminated; the empty string's are a terminator
        const CharT* data() const noexcept {
            return _word ? _chars() : &_empty;
        }

        const CharT* c_str() const noexcept {
            return data();
        }

        size_type size() const noexcept {
            return _word ? _header()->size : 0;
        }

        size_type length() const noexcept {
            return size();
        }

        bool empty() const noexcept {
            return !_word;
        }

        static constexpr size_type max_size() noexcept {
            return UINT32_MAX;
        }

        // The string as a slice that holds the object (slice.h): the
        // whole of it, or the characters [pos, pos + n), shared, nothing
        // copied
        slice_type as_slice() const noexcept {
            return slice_type(_word, data(), data() + size());
        }

        slice_type as_slice(size_type pos, size_type n = npos) const {
            if (pos > size()) {
                throw out_of_range("sgcl::basic_string::as_slice");
            }
            return slice_type(_word, data() + pos, data() + pos + std::min(n, size() - pos));
        }

        operator slice_type() const noexcept {
            return as_slice();
        }

        const CharT& operator[](size_type i) const noexcept {
            assert(i <= size());
            return data()[i];
        }

        const CharT& front() const noexcept {
            assert(!empty());
            return data()[0];
        }

        const CharT& back() const noexcept {
            assert(!empty());
            return data()[size() - 1];
        }

        const_iterator begin() const noexcept { return data(); }
        const_iterator end() const noexcept { return data() + size(); }
        const_iterator cbegin() const noexcept { return begin(); }
        const_iterator cend() const noexcept { return end(); }
        const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
        const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
        const_reverse_iterator crbegin() const noexcept { return rbegin(); }
        const_reverse_iterator crend() const noexcept { return rend(); }

        // A new string of the characters [pos, pos + n): a copy, the
        // whole string when the range is the whole string
        basic_string substr(size_type pos = 0, size_type n = npos) const {
            if (pos > size()) {
                throw out_of_range("sgcl::basic_string::substr");
            }
            if (pos == 0 && n >= size()) {
                return *this;
            }
            return basic_string(this->view().substr(pos, n));
        }

        // The pieces between the occurrences of `sep`, in order: an empty
        // piece where two separators meet or one ends the string; the
        // whole string when `sep` does not occur; every character on its
        // own for an empty `sep` (a code point a piece, as Go's Split with
        // ""); no piece for an empty string. With `max_parts`, at most that
        // many, the last holding the rest of the string; 0 is no limit. A range of views into this string (the
        // class `pieces`, below), computed as it is walked: nothing is
        // copied, `for (std::string_view piece : s.split(','))` allocates
        // nothing, and a container of strings is built from it when the
        // pieces are to be kept: `vector<string> parts(s.split(','))`.
        pieces split(view_type sep, size_type max_parts = 0) const {
            return pieces(*this, sep, max_parts, sep.empty() ? pieces::Characters : pieces::Separator);
        }

        pieces split(CharT sep, size_type max_parts = 0) const noexcept {
            return pieces(*this, sep, max_parts);
        }

        template<size_t N>
        pieces split(const CharT (&sep)[N], size_type max_parts = 0) const {
            return split(detail::array_text<CharT, Traits>(sep), max_parts);
        }

        template<class P>
        requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        pieces split(P sep, size_type max_parts = 0) const {
            return split(view_type(sep), max_parts);
        }

        pieces split(const basic_string& sep, size_type max_parts = 0) const noexcept {
            return pieces(*this, sep, max_parts, sep.empty() ? pieces::Characters : pieces::Separator);
        }

        // A value that is no code point (a surrogate, past U+10FFFF) occurs
        // nowhere: the whole string is one piece
        pieces split(char32_t sep, size_type max_parts = 0) const noexcept requires (!std::same_as<CharT, char32_t>) {
            return utf8::valid(sep) ? split(view_type(_encoded_of(sep)), max_parts) : pieces(*this, basic_string(), 1, pieces::Separator);
        }

        pieces split(int, size_type = 0) const = delete;   // 'ż' is an int: write U'ż'

        // The words: the pieces between runs of white space (Unicode's:
        // space, tab, newline and the rest of the C locale's six, and the
        // no-break, ideographic and other spaces, unicode::is_space), none
        // empty
        pieces fields() const noexcept {
            return pieces(*this, basic_string(), 0, pieces::Fields);
        }

        // One string of the parts (strings, views, literals: anything a
        // view is made of) with `sep` between each two, built once
        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
        static basic_string join(R&& parts, view_type sep) {
            if constexpr(std::ranges::forward_range<R>) {
                // A range walked twice: the lengths summed, then each part
                // and separator written once into a string of that size.
                // The sum may wrap 64 bits only when a part is longer than
                // max_size() (a view: a lazy range may give one long view
                // many times over) or there are more than max_size() parts;
                // below both, max_size() parts of max_size() each fit 64
                // bits. A part of this string type is never longer, so its
                // loop only sums and counts; a view's also ORs the lengths,
                // off the sum's chain. One test after the loop; past any
                // bound the length is counted again exactly (cold), which
                // throws only when the result would pass max_size(). A
                // range that knows its size gives the count itself: with
                // the count tested, the compiler kept the loop's counter
                // (one more instruction a part) instead of the distance
                constexpr bool Bounded = std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, basic_string>;
                size_t total = 0;
                size_t count = 0;
                [[maybe_unused]] size_t wide = 0;
                for (auto&& part : parts) {
                    if constexpr (Bounded) {
                        total += view_type(part).size();
                    } else {
                        size_t n = view_type(part).size();
                        total += n;
                        wide |= n;
                    }
                    ++count;
                }
                if constexpr (std::ranges::sized_range<R>) {
                    count = size_t(std::ranges::size(parts));
                }
                const size_t seps = count > 1 ? count - 1 : 0;
                if ((Bounded ? count : wide | count) > max_size() || total > max_size() || sep.size() > max_size()
                    || seps * sep.size() > max_size() - total) [[unlikely]] {
                    total = _joined_size(parts, sep);
                } else {
                    total += seps * sep.size();
                }
                return _filled(total, [&](CharT* at) {
                    bool first = true;
                    for (auto&& part : parts) {
                        if (!first) {
                            detail::copy_bytes(at, sep.data(), sep.size() * sizeof(CharT));
                            at += sep.size();
                        }
                        view_type v(part);
                        detail::copy_bytes(at, v.data(), v.size() * sizeof(CharT));
                        at += v.size();
                        first = false;
                    }
                });
            } else {
                // a single pass: gathered, as its length is not known
                std::basic_string<CharT, Traits> s;
                bool first = true;
                for (auto&& part : parts) {
                    if (!first) {
                        s.append(sep);
                    }
                    s.append(view_type(part));
                    first = false;
                }
                return basic_string(view_type(s));
            }
        }

        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
        static basic_string join(R&& parts, CharT sep) {
            return join(std::forward<R>(parts), view_type(&sep, 1));
        }

        template<std::ranges::input_range R, size_t N>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
        static basic_string join(R&& parts, const CharT (&sep)[N]) {
            return join(std::forward<R>(parts), detail::array_text<CharT, Traits>(sep));
        }

        template<std::ranges::input_range R, class P>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type>
              && (std::same_as<P, const CharT*> || std::same_as<P, CharT*>)
        static basic_string join(R&& parts, P sep) {
            return join(std::forward<R>(parts), view_type(sep));
        }

        template<std::ranges::input_range R>
        requires std::is_convertible_v<std::ranges::range_reference_t<R>, view_type> && (!std::same_as<CharT, char32_t>)
        static basic_string join(R&& parts, char32_t sep) {
            return join(std::forward<R>(parts), view_type(_encoded_of(sep)));
        }

        template<std::ranges::input_range R>
        static basic_string join(R&&, int) = delete;   // 'ż' is an int: write U'ż'

        // One string of the pieces in order — strings, slices, views,
        // literals, characters — their lengths summed first and each
        // written once into a string of that size: a text made of a few
        // known parts, without the steps of a+b+c (each + a string of its
        // own) or of appending
        template<class... A>
        requires (sizeof...(A) > 0) && ((std::is_convertible_v<const A&, view_type> || std::is_same_v<A, CharT>) && ...)
        static basic_string concat(const A&... pieces) {
            const size_type total = (size_type(0) + ... + _piece(pieces).size());
            return _filled(total, [&](CharT* at) {
                ((detail::copy_bytes(at, _piece(pieces).data(), _piece(pieces).size() * sizeof(CharT)), at += _piece(pieces).size()), ...);
            });
        }

        // Without the characters of `chars` (Unicode white space by
        // default, unicode::is_space; a set of code points as a
        // std::u32string_view, trim(U"«»")) at both ends, at the start, at
        // the end: the same object when there are none
        basic_string trim() const noexcept {
            auto from = this->_find_space(0, false);
            return from == npos ? basic_string() : _part(from, this->_end_without_spaces());
        }

        basic_string trim(view_type chars) const noexcept {
            auto v = this->view();
            auto from = v.find_first_not_of(chars);
            if (from == npos) {
                return basic_string();
            }
            auto to = v.find_last_not_of(chars) + 1;
            return _part(from, to);
        }

        template<size_t N>
        basic_string trim(const CharT (&chars)[N]) const noexcept {
            return trim(detail::array_text<CharT, Traits>(chars));
        }

        basic_string trim(std::u32string_view set) const noexcept requires (!std::same_as<CharT, char32_t>) {
            auto from = this->find_first_not_of(set);
            if (from == npos) {
                return basic_string();
            }
            auto last = this->find_last_not_of(set);
            return _part(from, last + this->_width_at(last));
        }

        basic_string trim_left() const noexcept {
            auto from = this->_find_space(0, false);
            return from == npos ? basic_string() : _part(from, size());
        }

        basic_string trim_left(std::u32string_view set) const noexcept requires (!std::same_as<CharT, char32_t>) {
            auto from = this->find_first_not_of(set);
            return from == npos ? basic_string() : _part(from, size());
        }

        basic_string trim_left(view_type chars) const noexcept {
            auto from = this->view().find_first_not_of(chars);
            return from == npos ? basic_string() : _part(from, size());
        }

        template<size_t N>
        basic_string trim_left(const CharT (&chars)[N]) const noexcept {
            return trim_left(detail::array_text<CharT, Traits>(chars));
        }

        basic_string trim_right() const noexcept {
            return _part(0, this->_end_without_spaces());
        }

        basic_string trim_right(std::u32string_view set) const noexcept requires (!std::same_as<CharT, char32_t>) {
            auto last = this->find_last_not_of(set);
            return last == npos ? basic_string() : _part(0, last + this->_width_at(last));
        }

        basic_string trim_right(view_type chars) const noexcept {
            auto to = this->view().find_last_not_of(chars);
            return to == npos ? basic_string() : _part(0, to + 1);
        }

        template<size_t N>
        basic_string trim_right(const CharT (&chars)[N]) const noexcept {
            return trim_right(detail::array_text<CharT, Traits>(chars));
        }

        // Without `prefix` at the start (`suffix` at the end) when it is
        // there; the same object when it is not
        basic_string trim_prefix(view_type prefix) const noexcept {
            return this->starts_with(prefix) ? _part(prefix.size(), size()) : *this;
        }

        basic_string trim_suffix(view_type suffix) const noexcept {
            return this->ends_with(suffix) ? _part(0, size() - suffix.size()) : *this;
        }

        template<size_t N>
        basic_string trim_prefix(const CharT (&prefix)[N]) const noexcept {
            return trim_prefix(detail::array_text<CharT, Traits>(prefix));
        }

        template<size_t N>
        basic_string trim_suffix(const CharT (&suffix)[N]) const noexcept {
            return trim_suffix(detail::array_text<CharT, Traits>(suffix));
        }

        // With every occurrence of `from` (the first `count` of them, when
        // given; 0 is every one) replaced by `to`, left to right without
        // overlapping; the same object when `from` is empty or does not
        // occur
        basic_string replace(view_type from, view_type to, size_type count = 0) const {
            auto v = this->view();
            auto at = from.empty() ? npos : v.find(from);
            if (at == npos) {
                return *this;
            }
            // A pass that counts the occurrences (as the writing will take
            // them: left to right, not overlapping, the first `count`), then
            // the text written once into a string of the size they make
            size_type hits = 0;
            for (auto k = at; k != npos; k = (count && hits == count) ? npos : v.find(from, k + from.size())) {
                ++hits;
            }
            // Past the maximum when what each occurrence adds, times their
            // number, passes the room left: checked by a division, as the
            // product of a long `to` and many occurrences may pass 64 bits
            // and wrap to a small length
            if (to.size() > from.size() && hits > (max_size() - v.size()) / (to.size() - from.size())) {
                throw length_error("sgcl::basic_string::replace");
            }
            const size_type total = v.size() - hits * from.size() + hits * to.size();
            return _filled(total, [&](CharT* out) {
                size_type pos = 0;
                auto put = [&](const CharT* p, size_type k) {
                    detail::copy_bytes(out, p, k * sizeof(CharT));
                    out += k;
                };
                for (size_type h = 0, k = at; h < hits; ++h) {
                    put(v.data() + pos, k - pos);
                    put(to.data(), to.size());
                    pos = k + from.size();
                    if (h + 1 < hits) {
                        k = v.find(from, pos);
                    }
                }
                put(v.data() + pos, v.size() - pos);
            });
        }

        // An array on either side, or both, read to its end as well
        template<size_t N, size_t M>
        basic_string replace(const CharT (&from)[N], const CharT (&to)[M], size_type count = 0) const {
            return replace(detail::array_text<CharT, Traits>(from), detail::array_text<CharT, Traits>(to), count);
        }

        template<size_t N>
        basic_string replace(const CharT (&from)[N], view_type to, size_type count = 0) const {
            return replace(detail::array_text<CharT, Traits>(from), to, count);
        }

        template<size_t M>
        basic_string replace(view_type from, const CharT (&to)[M], size_type count = 0) const {
            return replace(from, detail::array_text<CharT, Traits>(to), count);
        }

        basic_string replace(CharT from, CharT to, size_type count = 0) const {
            return replace(view_type(&from, 1), view_type(&to, 1), count);
        }

        // A `from` that is no code point occurs nowhere (the same object); a
        // `to` that is none is written as U+FFFD, as utf8::encode writes it
        basic_string replace(char32_t from, char32_t to, size_type count = 0) const requires (!std::same_as<CharT, char32_t>) {
            if (!utf8::valid(from)) {
                return *this;
            }
            return replace(view_type(_encoded_of(from)), view_type(_encoded_of(to)), count);
        }

        basic_string replace(int, int, size_type = 0) const = delete;   // 'ż' is an int: write U'ż'

        // The string `count` times over: empty for 0, the same object for 1
        basic_string repeat(size_type count) const {
            if (count == 0 || empty()) {
                return basic_string();
            }
            if (count == 1) {
                return *this;
            }
            auto v = this->view();
            if (v.size() > max_size() / count) {
                throw length_error("sgcl::basic_string::repeat");
            }
            return _filled(v.size() * count, [&](CharT* chars) {
                for (size_type i = 0; i < count; ++i) {
                    detail::copy_bytes(chars + i * v.size(), v.data(), v.size() * sizeof(CharT));
                }
            });
        }

        // With every letter in lower (upper) case by Unicode's simple
        // case mapping (unicode::to_lower: one code point to one, "ŁÓDŹ"
        // to "łódź"; ß stays ß, no language's rules), the other characters
        // as they are; the same object when no letter changes
        basic_string to_lower() const {
            if constexpr (sizeof(CharT) == 1) {
                if (utf8::all_ascii(this->_bytes())) {
                    return _ascii_cased('A', 'Z', 32);
                }
            }
            return _cased<detail::unicode_tables::ToLower>();
        }

        basic_string to_upper() const {
            if constexpr (sizeof(CharT) == 1) {
                if (utf8::all_ascii(this->_bytes())) {
                    return _ascii_cased('a', 'z', -32);
                }
            }
            return _cased<detail::unicode_tables::ToUpper>();
        }

        void swap(basic_string& o) noexcept {
            std::swap(_word, o._word);
        }

    private:
        // The same object, or the same characters: the lengths first,
        // the hashes when both are known, the characters last (the free
        // operator== below)
        template<class C, class Tr>
        friend bool operator==(const basic_string<C, Tr>&, const basic_string<C, Tr>&) noexcept;

        bool _equals(const basic_string& o) const noexcept {
            if (object() == o.object()) {
                return true;
            }
            if (size() != o.size()) {
                return false;
            }
            if (object() && o.object()) {
                auto h = _header()->hash.load(std::memory_order_relaxed);
                auto oh = o._header()->hash.load(std::memory_order_relaxed);
                if (h && oh && h != oh) {
                    return false;
                }
            }
            return this->view() == o.view();
        }

    public:
        // The hash of the characters, computed the first time and kept in
        // the string's object (0 is "not yet"); the empty string's is a constant
        size_t hash() const noexcept {
            if (!_word) {
                return HashMultiplier;
            }
            auto header = _header();
            auto h = header->hash.load(std::memory_order_relaxed);
            if (!h) {
                h = _fold(detail::hash_bytes(data(), size() * sizeof(CharT)));
                header->hash.store(h, std::memory_order_relaxed);
            }
            return (size_t)h * HashMultiplier;
        }

        // The hash a string of these characters has: what std::hash of the
        // string is for a string_view or a literal, so that a map keyed by
        // strings is searched with either and no string is made for the
        // search (std::hash<basic_string> is transparent, below)
        static size_t hash_of(view_type s) noexcept {
            return s.empty() ? HashMultiplier : (size_t)_fold(detail::hash_bytes(s.data(), s.size() * sizeof(CharT))) * HashMultiplier;
        }

        // The address of the string's object: its identity (two strings
        // made from the same characters are two objects); null when empty
        const void* object() const noexcept {
            return _word.get();
        }

    private:
        detail::StringHeader* _header() const noexcept {
            return const_cast<detail::StringHeader*>(static_cast<const detail::StringHeader*>(_word.get()));
        }

        static constexpr size_t HashMultiplier = 0x9E3779B97F4A7C15ull;

        // The keyed hash of the characters (detail/hash_bytes.h), folded to the 32
        // bits the header keeps; never 0
        static uint32_t _fold(size_t h) noexcept {
            auto r = (uint32_t)(h ^ (h >> 32));
            return r ? r : 1;
        }

        const CharT* _chars() const noexcept {
            return reinterpret_cast<const CharT*>(static_cast<const unsigned char*>(_word.get()) + sizeof(detail::StringHeader));
        }

        // The characters [from, to): the same object for the whole string
        basic_string _part(size_type from, size_type to) const noexcept {
            return (from == 0 && to == size()) ? *this : basic_string(this->view().substr(from, to - from));
        }

        // A Latin letter changes case by one bit, and no letter of ASCII
        // maps to anything but its own pair, so a text that is all ASCII
        // needs no decoding and no table: one pass over the bytes, where
        // the general path below decodes a code point at a time and asks
        // a table about each
        basic_string _ascii_cased(char lo, char hi, int by) const noexcept {
            auto v = this->view();
            // Without a branch, because one here costs twelve times what
            // the work does: the letters of a text do not alternate in
            // any way a processor can predict, and a branch is also what
            // stops the loop being done a word at a time. A pass that only
            // reads says whether anything changes (the same object when
            // not); the second writes the new string in place
            bool changed = false;
            for (CharT c : v) {
                changed |= uint8_t(uint8_t(c) - uint8_t(lo)) <= uint8_t(hi - lo);
            }
            if (!changed) {
                return *this;
            }
            // The bounds, the step and the source in locals of the fill: a
            // store through a character pointer may alias any memory, the
            // lambda's captures included, so read from the captures the
            // compiler reloads them at every byte and does not do the loop
            // a vector at a time (three to four times slower, measured);
            // locals whose address is never taken it keeps in registers
            return _filled(v.size(), [&](CharT* chars) {
                const CharT* from = v.data();
                const size_type n = v.size();
                const uint8_t first = uint8_t(lo), span = uint8_t(hi - lo);
                const int step = by;
                for (size_type i = 0; i < n; ++i) {
                    CharT c = from[i];
                    bool letter = uint8_t(uint8_t(c) - first) <= span;
                    chars[i] = CharT(int(c) + (letter ? step : 0));
                }
            });
        }

        // The unit at i of a 16-bit text, a surrogate, as the map leaves
        // it: the same half of the mapped code point when it is a half of
        // a pair, else itself (a lone surrogate is no code point). Each
        // half is asked on its own, so the loops over units step by one;
        // out of line, so that they keep their shape for the plane
        template<class Map>
        SGCL_NOINLINE static CharT _pair_half(Map map, const CharT* u, size_type n, size_type i) noexcept {
            CharT c = u[i];
            if (char32_t(c) < 0xDC00) {
                if (i + 1 < n && (char32_t(u[i + 1]) & 0xFC00) == 0xDC00) {
                    char32_t m = map(0x10000 + ((char32_t(c) - 0xD800) << 10) + (char32_t(u[i + 1]) - 0xDC00));
                    return CharT(0xD800 + ((m - 0x10000) >> 10));
                }
            } else if (i > 0 && (char32_t(u[i - 1]) & 0xFC00) == 0xD800) {
                char32_t m = map(0x10000 + ((char32_t(u[i - 1]) - 0xD800) << 10) + (char32_t(c) - 0xDC00));
                return CharT(0xDC00 + ((m - 0x10000) & 0x3FF));
            }
            return c;
        }

        // The characters mapped a code point at a time by Table (a UTF-8
        // sequence, a UTF-16 unit or surrogate pair, a unit of a 32-bit
        // string): the same object when none changes, else a new string,
        // whose bytes may be more or fewer in UTF-8
        template<const detail::CaseTable& Table>
        basic_string _cased() const {
            auto map = [](char32_t c) { return detail::unicode_mapped(c, Table); };
            auto v = this->view();
            if constexpr (sizeof(CharT) == 1) {
                auto bytes = this->_bytes();
                size_type first = 0;
                for (; first < bytes.size();) {
                    auto [c, n] = utf8::decode(bytes, first);
                    if (map(c) != c) {
                        break;
                    }
                    first += n;
                }
                if (first == bytes.size()) {
                    return *this;
                }
                // A mapped letter may take more bytes or fewer than its own
                // (ɐ 2 → Ɐ 3, K 3 → k 1): a pass over the rest counts the
                // difference, and the string is written once at its size
                ptrdiff_t grew = 0;
                for (size_type i = first; i < bytes.size();) {
                    auto [c, n] = utf8::decode(bytes, i);
                    auto m = map(c);
                    if (m != c) {
                        grew += ptrdiff_t(utf8::width(m)) - ptrdiff_t(n);
                    }
                    i += n;
                }
                return _filled(size_type(ptrdiff_t(v.size()) + grew), [&](CharT* chars) {
                    detail::copy_bytes(chars, v.data(), first);
                    CharT* w = chars + first;
                    for (size_type i = first; i < bytes.size();) {
                        auto [c, n] = utf8::decode(bytes, i);
                        auto m = map(c);
                        if (m == c) {
                            detail::copy_bytes(w, v.data() + i, n);
                            w += n;
                        } else {
                            w += utf8::encode(m, reinterpret_cast<char*>(w));
                        }
                        i += n;
                    }
                });
            } else if constexpr (sizeof(CharT) == 2) {
                // A unit of the Basic Multilingual Plane is its own code
                // point, mapped as it is; a high surrogate followed by a
                // low one is a letter past U+FFFF (Deseret, Adlam), mapped
                // as one. No simple mapping crosses the end of the plane
                // (the tests ask every code point), so a pair maps to a
                // pair and the string keeps its size. A lone surrogate is
                // no code point and maps to itself: left as it is, as an
                // ill-formed byte of UTF-8.
                //
                // One pass: the scan stops at the first unit that changes,
                // the unchanged start is copied and the mapping goes on
                // from there. Every unit, ASCII included, maps through the
                // plane's two-stage table, without a branch; the table
                // maps no surrogate, so only the test of a surrogate,
                // which plain text never takes, sends a unit to its pair.
                // Each half of a pair is its own step (_pair_half, out of
                // line), so both loops go a unit a turn
                const CharT* u = v.data();
                const size_type n = v.size();
                size_type first = 0;
                for (; first < n; ++first) {
                    char32_t c = char32_t(u[first]);
                    if (detail::unicode_plane_mapped(c, Table) != c) {
                        break;
                    }
                    if ((c & 0xF800) == 0xD800 && _pair_half(map, u, n, first) != CharT(c)) {
                        break;
                    }
                }
                if (first == n) {
                    return *this;
                }
                return _filled(n, [&](CharT* chars) {
                    detail::copy_bytes(chars, u, first * sizeof(CharT));
                    for (size_type i = first; i < n; ++i) {
                        char32_t c = char32_t(u[i]);
                        CharT m = CharT(detail::unicode_plane_mapped(c, Table));
                        chars[i] = (c & 0xF800) == 0xD800 ? _pair_half(map, u, n, i) : m;
                    }
                });
            } else {
                auto changes = [&](CharT c) { return map(char32_t(c)) != char32_t(c); };
                if (std::find_if(v.begin(), v.end(), changes) == v.end()) {
                    return *this;
                }
                return _filled(v.size(), [&](CharT* chars) {
                    for (size_type i = 0; i < v.size(); ++i) {
                        chars[i] = CharT(map(char32_t(v[i])));
                    }
                });
            }
        }

        // A code point as the text it is split on or replaced: its units
        static auto _encoded_of(char32_t c) noexcept requires (!std::same_as<CharT, char32_t>) {
            return typename basic_string::_encoded(c);
        }

        // Whether the slice is the whole of a string: its owner a string
        // object holding exactly its characters
        static bool _whole_string(const slice_type& v) noexcept {
            auto o = v.owner().get();
            if (!o) {
                return false;
            }
            auto chars = reinterpret_cast<const CharT*>(static_cast<const unsigned char*>(o) + sizeof(detail::StringHeader));
            return v.data() == chars && v.size() == static_cast<const detail::StringHeader*>(o)->size && detail::Page::metadata_of(o).is_string;
        }

        inline static constexpr CharT _empty = CharT();

        // A string over the word of its object (detail::StringAccess:
        // atomic.h, intern.h)
        static view_type _bytes_view(const slice<const byte>& b) noexcept {
            return view_type(reinterpret_cast<const CharT*>(b.data()), b.size());
        }

        explicit basic_string(const tracked_ptr<const void>& w) noexcept
        : _word(w) {
        }

        // A piece of concat as a view: a character as one of itself
        template<class A>
        static view_type _piece(const A& a) noexcept {
            if constexpr(std::is_same_v<A, CharT>) {
                return view_type(&a, 1);
            } else if constexpr(std::is_array_v<A>) {
                return detail::array_text<CharT, Traits>(a);   // to its first NUL or its end
            } else {
                return view_type(a);
            }
        }

        // The length of join's result counted exactly, a part or separator
        // at a time against the room left: length_error past max_size()
        template<class R>
        SGCL_NOINLINE static size_type _joined_size(R& parts, view_type sep) {
            size_type total = 0;
            bool first = true;
            for (auto&& part : parts) {
                const size_t n = view_type(part).size();
                if ((!first && sep.size() > max_size() - total) || n > max_size() - total - (first ? 0 : sep.size())) {
                    throw length_error("sgcl::basic_string::join");
                }
                total += (first ? 0 : sep.size()) + n;
                first = false;
            }
            return total;
        }

        // A string written in place (detail::StringAccess::filled, bounded)
        template<class Fill>
        static basic_string _filled(size_type n, Fill&& fill) {
            return basic_string(Maker::template make_filled<CharT>(n, std::forward<Fill>(fill)));
        }

        template<class Fill>
        static basic_string _bounded(size_type bound, Fill&& fill) {
            return basic_string(Maker::template make_bounded<CharT>(bound, std::forward<Fill>(fill)));
        }

        // The handle's word, for the atomics (detail/handle_word.h)
        basic_string(detail::FromWord, const tracked_ptr<const void>& w) noexcept
        : _word(w) {
        }

        Word& _handle_word() noexcept {
            return _word;
        }

        const Word& _handle_word() const noexcept {
            return _word;
        }

        Word _word;

        friend struct detail::HandleWord;
        friend struct detail::StringAccess;
    };

    // The pieces of a string between the occurrences of a separator, the
    // words between white space, or the characters: what split and fields
    // hand back. A forward range of slices into the string, one piece
    // per step, found as the walk goes (one find per step); each slice
    // holds the string's object, so a piece kept anywhere a tracked_ptr
    // may live stays valid on its own. The object holds the
    // string and the separator (a copy of the view given, so that a
    // temporary separator in the head of a range-for cannot dangle: one
    // of up to 16 bytes inline, a longer one in a string of its own; a
    // string given as the separator is held as it is). A container of
    // slices or of strings is built from the range when the pieces are to
    // be kept.
    template<class CharT, class Traits>
    class basic_string<CharT, Traits>::pieces {
    public:
        using value_type = slice<const CharT>;
        using size_type = size_t;

        class iterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = slice<const CharT>;
            using difference_type = ptrdiff_t;
            using pointer = const value_type*;
            using reference = const value_type&;

            iterator() noexcept = default;

            reference operator*() const noexcept {
                return _piece;
            }

            pointer operator->() const noexcept {
                return &_piece;
            }

            iterator& operator++() noexcept {
                _advance();
                return *this;
            }

            iterator operator++(int) noexcept {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            friend bool operator==(const iterator& a, const iterator& b) noexcept {
                return a._done == b._done && (a._done || a._next == b._next);
            }

        private:
            const pieces* _owner = nullptr;
            size_type _next = npos;    // where the next piece starts; npos when the last one is out
            size_type _count = 0;      // pieces given out so far
            bool _done = true;         // past the last piece: the end
            value_type _piece;

            iterator(const pieces* owner, size_type start) noexcept
            : _owner(owner)
            , _next(start)
            , _done(false) {
                _advance();
            }

            // The piece from _next, and _next moved past it and its
            // separator (npos once the last piece is out); the end when
            // there is no piece to give
            void _advance() noexcept {
                if (_next == npos) {
                    _done = true;
                    return;
                }
                auto text = _owner->_text.view();
                switch (_owner->_mode) {
                case Fields: {
                    auto from = _owner->_text._find_space(_next, false);
                    if (from == npos) {
                        _done = true;
                        return;
                    }
                    auto to = _owner->_text._find_space(from, true);
                    _piece = _owner->_text.as_slice(from, to == npos ? npos : to - from);
                    _next = to;
                    return;
                }
                case Characters:
                    if (_next >= text.size()) {
                        _done = true;
                        return;
                    }
                    if (_last()) {
                        _piece = _owner->_text.as_slice(_next);
                        _next = npos;
                    } else {
                        auto width = _owner->_text._width_at(_next);   // a code point: its bytes, a surrogate pair
                        _piece = _owner->_text.as_slice(_next, width);
                        _next += width;
                    }
                    ++_count;
                    return;
                case Separator: {
                    auto sep = _owner->_separator();
                    auto at = _last() ? npos : text.find(sep, _next);
                    if (at == npos) {
                        _piece = _owner->_text.as_slice(_next);
                        _next = npos;
                    } else {
                        _piece = _owner->_text.as_slice(_next, at - _next);
                        _next = at + sep.size();
                    }
                    ++_count;
                    return;
                }
                }
            }

            // Whether the piece to give out is the last one allowed
            bool _last() const noexcept {
                return _owner->_max_parts && _count + 1 == _owner->_max_parts;
            }

            friend class pieces;
        };

        using const_iterator = iterator;

        iterator begin() const noexcept {
            return _text.empty() ? end() : iterator(this, 0);
        }

        iterator end() const noexcept {
            return iterator();
        }

        bool empty() const noexcept {
            return begin() == end();
        }

        // The string the pieces are of
        const basic_string& text() const noexcept {
            return _text;
        }

    private:
        // How the pieces are found, set by the function that made the range:
        // split by a separator, by characters (an empty separator), fields
        enum Mode { Separator, Characters, Fields };

        // The characters of a separator kept inline: 16 bytes of them, the
        // separators one writes (", ", "::", "\r\n", a character's
        // encoding) without an object of their own per split
        static constexpr size_type InlineSeparator = 16 / sizeof(CharT);

        basic_string _text;
        basic_string _sep;
        CharT _sep_chars[InlineSeparator] = {};
        unsigned char _sep_size = 0;   // the characters in _sep_chars; 0: the separator is _sep
        size_type _max_parts;
        Mode _mode;

        pieces(const basic_string& text, basic_string sep, size_type max_parts, Mode mode) noexcept
        : _text(text)
        , _sep(std::move(sep))
        , _max_parts(max_parts)
        , _mode(mode) {
        }

        pieces(const basic_string& text, view_type sep, size_type max_parts, Mode mode)
        : _text(text)
        , _max_parts(max_parts)
        , _mode(mode) {
            if (sep.size() <= InlineSeparator) {
                Traits::copy(_sep_chars, sep.data(), sep.size());
                _sep_size = (unsigned char)sep.size();
            } else {
                _sep = basic_string(sep);
            }
        }

        pieces(const basic_string& text, CharT sep, size_type max_parts) noexcept
        : _text(text)
        , _sep_chars{sep}
        , _sep_size(1)
        , _max_parts(max_parts)
        , _mode(Separator) {
        }

        // The separator as a view: the characters kept inline, or the string
        view_type _separator() const noexcept {
            return _sep_size ? view_type(_sep_chars, _sep_size) : _sep.view();
        }

        friend class basic_string;
    };

    using string = basic_string<char>;
    using wstring = basic_string<wchar_t>;
    using u8string = basic_string<char8_t>;
    using u16string = basic_string<char16_t>;
    using u32string = basic_string<char32_t>;
    using string_slice = slice<const char>;   // a piece of a string that holds it (slice.h); the same for the other character types by slice<const CharT>

    template<class CharT, class Traits>
    void swap(basic_string<CharT, Traits>& l, basic_string<CharT, Traits>& r) noexcept {
        l.swap(r);
    }

    template<class CharT, class Traits>
    bool operator==(const basic_string<CharT, Traits>& a, const basic_string<CharT, Traits>& b) noexcept {
        return a._equals(b);
    }

    template<class CharT, class Traits>
    std::strong_ordering operator<=>(const basic_string<CharT, Traits>& a, const basic_string<CharT, Traits>& b) noexcept {
        return std::basic_string_view<CharT, Traits>(a) <=> std::basic_string_view<CharT, Traits>(b);
    }

    // A string against a slice of characters: by the characters (the
    // conversions of each to a std view would tie otherwise)
    template<class CharT, class Traits>
    bool operator==(const basic_string<CharT, Traits>& a, const slice<const CharT>& b) noexcept {
        return a.view() == b.view();
    }

    template<class CharT, class Traits>
    std::strong_ordering operator<=>(const basic_string<CharT, Traits>& a, const slice<const CharT>& b) noexcept {
        return a.view() <=> b.view();
    }

    // Concatenation: a new string of the two, built once
    namespace detail {
        template<class S, class CharT, class Traits>
        S string_concat(std::basic_string_view<CharT, Traits> a, std::basic_string_view<CharT, Traits> b) {
            return StringAccess::filled<S>(a.size() + b.size(), [&](CharT* chars) {
                copy_bytes(chars, a.data(), a.size() * sizeof(CharT));
                copy_bytes(chars + a.size(), b.data(), b.size() * sizeof(CharT));
            });
        }
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, std::type_identity_t<std::basic_string_view<CharT, Traits>> b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), b);
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(std::type_identity_t<std::basic_string_view<CharT, Traits>> a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(a, std::basic_string_view<CharT, Traits>(b));
    }

    // An array (a literal) up to its first NUL or its end; a pointer
    // (CharT* or const CharT*) up to its NUL
    template<class CharT, class Traits, size_t N>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, const CharT (&b)[N]) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), detail::array_text<CharT, Traits>(b));
    }

    template<class CharT, class Traits, class P>
    requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, P b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits, size_t N>
    basic_string<CharT, Traits> operator+(const CharT (&a)[N], const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(detail::array_text<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits, class P>
    requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
    basic_string<CharT, Traits> operator+(P a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(const basic_string<CharT, Traits>& a, CharT b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(a), std::basic_string_view<CharT, Traits>(&b, 1));
    }

    template<class CharT, class Traits>
    basic_string<CharT, Traits> operator+(CharT a, const basic_string<CharT, Traits>& b) {
        return detail::string_concat<basic_string<CharT, Traits>>(std::basic_string_view<CharT, Traits>(&a, 1), std::basic_string_view<CharT, Traits>(b));
    }

    template<class CharT, class Traits>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const basic_string<CharT, Traits>& s) {
        return os << std::basic_string_view<CharT, Traits>(s);
    }

    template<class CharT, class Traits>
    requires detail::IsCharacter<CharT>
    std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os, const slice<const CharT>& v) {
        return os << v.view();
    }

    // A number as a string: to_string(42), to_string(2.5); "true"/"false"
    // for a bool; a char as a string of one
    template<class T>
    requires std::is_arithmetic_v<T> && (!std::is_same_v<T, bool>) && (!std::is_same_v<T, char>)
    string to_string(T v) noexcept {
        return string(std::to_string(v));
    }

    inline string to_string(bool v) noexcept {
        return v ? "true" : "false";
    }

    inline string to_string(char c) noexcept {
        return string(1, c);
    }

    // Why parse<T> read no number: the reason and the byte it stopped on
    // (0 for an empty text or one that does not begin as a number, the
    // first byte after the digits for a text with more after them or a
    // number out of the type's range). The
    // code is what std::from_chars reports: invalid_argument, or
    // result_out_of_range for a number that does not fit the type.
    class number_error {
    public:
        enum class reason : uint8_t {
            empty,          // no text
            not_a_number,   // the text does not begin as a number of the type
            trailing,       // a number, and more after it
            out_of_range    // a number the type cannot hold
        };

        constexpr number_error(reason r, size_t offset) noexcept
        : _reason(r)
        , _offset(offset) {
        }

        constexpr std::errc code() const noexcept {
            return _reason == reason::out_of_range ? std::errc::result_out_of_range : std::errc::invalid_argument;
        }

        constexpr reason why() const noexcept {
            return _reason;
        }

        constexpr size_t offset() const noexcept {
            return _offset;
        }

        string message() const noexcept {
            switch (_reason) {
                case reason::empty: return "an empty text";
                case reason::not_a_number: return "not a number";
                case reason::trailing: return "more after the number";
                case reason::out_of_range: return "a number out of the type's range";
            }
            return "not a number";
        }

        friend bool operator==(const number_error&, const number_error&) noexcept = default;

    private:
        reason _reason;
        size_t _offset;
    };

    namespace detail {
        inline number_error number_failure(std::string_view text, const char* end, std::errc ec) noexcept {
            if (text.empty()) {
                return number_error(number_error::reason::empty, 0);
            }
            if (ec == std::errc::result_out_of_range) {
                return number_error(number_error::reason::out_of_range, size_t(end - text.data()));   // from_chars gives where the number ends
            }
            if (ec != std::errc()) {
                return number_error(number_error::reason::not_a_number, 0);
            }
            return number_error(number_error::reason::trailing, size_t(end - text.data()));
        }
    }

    // A number from its text, the reverse of to_string: parse<int>("42"),
    // parse<double>("2.5"), parse<bool>("true"); the error when the text
    // is not exactly one number (no white space, no sign for an unsigned
    // type, nothing after the digits) or it does not fit the type. What
    // C#'s TryParse, Go's strconv and Java's parseInt do, as an expected:
    // std::from_chars under it, so no locale and no allocation. A base
    // other than 10 for the integers: parse<int>("ff", 16), from 2 to 36;
    // any other reads no number (not_a_number).
    template<class T>
    requires std::is_integral_v<T> && (!std::is_same_v<T, bool>)
    expected<T, number_error> parse(std::string_view text, int base = 10) noexcept {
        if (base < 2 || base > 36) {
            // no number is written in it: from_chars would read past its
            // tables (a precondition of the standard's, undefined behaviour)
            return unexpected(number_error(text.empty() ? number_error::reason::empty : number_error::reason::not_a_number, 0));
        }
        T value;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
        if (ec != std::errc() || end != text.data() + text.size() || text.empty()) {
            return unexpected(detail::number_failure(text, end, ec));
        }
        return value;
    }

    template<class T>
    requires std::is_floating_point_v<T>
    expected<T, number_error> parse(std::string_view text) noexcept {
        T value;
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc() || end != text.data() + text.size() || text.empty()) {
            return unexpected(detail::number_failure(text, end, ec));
        }
        return value;
    }

    template<class T>
    requires std::is_same_v<T, bool>
    expected<T, number_error> parse(std::string_view text) noexcept {
        if (text == "true") {
            return true;
        }
        if (text == "false") {
            return false;
        }
        return unexpected(number_error(text.empty() ? number_error::reason::empty : number_error::reason::not_a_number, 0));
    }
}

// A map keyed by strings is searched with a string_view or a literal as
// with a string, and no string is made for the search: the hash, the
// equality and the order of a string are transparent (the containers
// take a key of any type they accept, given that), and the hash of a
// view is the hash the string keeps.
namespace std {
    template<class CharT, class Traits>
    struct hash<sgcl::basic_string<CharT, Traits>> {
        using is_transparent = void;

        size_t operator()(const sgcl::basic_string<CharT, Traits>& s) const noexcept {
            return s.hash();
        }

        size_t operator()(std::basic_string_view<CharT, Traits> s) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(s);
        }

        template<size_t N>
        size_t operator()(const CharT (&s)[N]) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(sgcl::detail::array_text<CharT, Traits>(s));
        }

        template<class P>
        requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        size_t operator()(P s) const noexcept {
            return sgcl::basic_string<CharT, Traits>::hash_of(s);
        }
    };

    template<class CharT>
    requires sgcl::detail::IsCharacter<CharT>
    struct hash<sgcl::slice<const CharT>> {
        using is_transparent = void;

        size_t operator()(const sgcl::slice<const CharT>& v) const noexcept {
            return sgcl::basic_string<CharT>::hash_of(v.view());
        }

        size_t operator()(const sgcl::basic_string<CharT>& s) const noexcept {
            return s.hash();
        }

        size_t operator()(std::basic_string_view<CharT> s) const noexcept {
            return sgcl::basic_string<CharT>::hash_of(s);
        }

        template<size_t N>
        size_t operator()(const CharT (&s)[N]) const noexcept {
            return sgcl::basic_string<CharT>::hash_of(sgcl::detail::array_text<CharT, std::char_traits<CharT>>(s));
        }

        template<class P>
        requires std::same_as<P, const CharT*> || std::same_as<P, CharT*>
        size_t operator()(P s) const noexcept {
            return sgcl::basic_string<CharT>::hash_of(s);
        }
    };

    template<class CharT>
    requires sgcl::detail::IsCharacter<CharT>
    struct equal_to<sgcl::slice<const CharT>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a == b;
        }
    };

    template<class CharT>
    requires sgcl::detail::IsCharacter<CharT>
    struct less<sgcl::slice<const CharT>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a < b;
        }
    };

    template<class CharT, class Traits>
    struct equal_to<sgcl::basic_string<CharT, Traits>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a == b;
        }
    };

    template<class CharT, class Traits>
    struct less<sgcl::basic_string<CharT, Traits>> {
        using is_transparent = void;

        template<class A, class B>
        bool operator()(const A& a, const B& b) const noexcept {
            return a < b;
        }
    };
}
