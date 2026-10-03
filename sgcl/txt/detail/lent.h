//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "../../core/aliases.h"
#include "../../core/detail/bytes.h"
#include "../../core/detail/os.h"
#include "../../core/slice.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <type_traits>
#include <utility>

// The plain memory one call of this module works in, lent by the thread.
//
// The scratch of a call — the code points of a text being normalized, the
// mapped copy of a text a search walks, the arrays of the bidirectional
// algorithm — holds no pointer and never leaves the call, so it belongs in
// plain memory and not on the managed heap, where it was the garbage of
// every call until the next cycle (the audit of 2026-09-26: 262 KB for one
// search through ten kilobytes). But plain memory from the allocator is a
// malloc and a free per array per call, and a large one is fresh pages
// from the system every time, where the managed pools cost a few
// nanoseconds on the calling thread and leave the rest to the collector's:
// the levels of a short line went from 1.28 to 1.84 us with the paragraph's
// arrays as fresh std::vectors. So a thread keeps a few of each kind and
// lends them out. The call after the first allocates nothing; a call made
// while all of the thread's are out (a scratch inside a scratch) gets one of
// its own.
//
// What goes back is emptied. A slot that holds more than LentKeep is let go
// after the call, and so is one that would take what the thread keeps past
// LentThreadKeep, so that a thread which once searched a text of some
// megabytes does not hold their room for the rest of its life: at most 2 MB
// a thread, whatever it has done.
namespace sgcl::txt::detail {
    inline constexpr size_t LentKeep = size_t(256) << 10;
    inline constexpr size_t LentThreadKeep = size_t(2) << 20;
    inline constexpr size_t LentSlots = 8;

    // What scratch_vector's elements must be is what keeps the collector's
    // words out of it: its memory is plain and nothing scans it, so a
    // tracked_ptr there would not keep its target (README, rule 1). None
    // of the library's types that hold one is trivially copyable — a store
    // to a tracked word is a barrier — so the requirement below is the
    // rule, and these say so where it would be broken first.
    static_assert(!std::is_trivially_copyable_v<tracked_ptr<int>>, "a tracked word has a barrier");
    static_assert(!std::is_trivially_copyable_v<string>, "a string holds a tracked word");
    static_assert(!std::is_trivially_copyable_v<slice<const char>>, "a slice holds its owner");

    // The array of the scratch: plain memory, elements that are bytes to
    // copy. Not a std::vector, because the loops of this module fill their
    // arrays one element at a time, and std::vector's push_back measured
    // 2.0 ns an element where this one, a count and a capacity beside a
    // pointer as the library's vector keeps them, costs 0.6 — which over
    // the mapping of a text was a search a quarter slower than on the
    // managed heap.
    template<class T>
    class scratch_vector {
        static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>,
                      "scratch_vector holds elements that are bytes to copy, never a tracked word: "
                      "its memory is not scanned");

    public:
        using value_type = T;
        using size_type = size_t;
        using iterator = T*;
        using const_iterator = const T*;

        scratch_vector() noexcept = default;

        scratch_vector(const scratch_vector& other) noexcept {
            assign(other.begin(), other.end());
        }

        scratch_vector(scratch_vector&& other) noexcept
        : _data(std::exchange(other._data, nullptr))
        , _size(std::exchange(other._size, 0))
        , _capacity(std::exchange(other._capacity, 0)) {
        }

        scratch_vector& operator=(const scratch_vector& other) noexcept {
            if (this != &other) {
                assign(other.begin(), other.end());
            }
            return *this;
        }

        scratch_vector& operator=(scratch_vector&& other) noexcept {
            if (this != &other) {
                std::free(_data);
                _data = std::exchange(other._data, nullptr);
                _size = std::exchange(other._size, 0);
                _capacity = std::exchange(other._capacity, 0);
            }
            return *this;
        }

        ~scratch_vector() {
            std::free(_data);
        }

        size_t size() const noexcept {
            return _size;
        }

        bool empty() const noexcept {
            return _size == 0;
        }

        size_t capacity() const noexcept {
            return _capacity;
        }

        T* data() noexcept {
            return _data;
        }

        const T* data() const noexcept {
            return _data;
        }

        T* begin() noexcept {
            return _data;
        }

        T* end() noexcept {
            return _data + _size;
        }

        const T* begin() const noexcept {
            return _data;
        }

        const T* end() const noexcept {
            return _data + _size;
        }

        T& operator[](size_t i) noexcept {
            return _data[i];
        }

        const T& operator[](size_t i) const noexcept {
            return _data[i];
        }

        T& front() noexcept {
            return _data[0];
        }

        const T& front() const noexcept {
            return _data[0];
        }

        T& back() noexcept {
            return _data[_size - 1];
        }

        const T& back() const noexcept {
            return _data[_size - 1];
        }

        void clear() noexcept {
            _size = 0;
        }

        // The members that grow cannot throw: a failed realloc ends the
        // program (os::memory_refused)
        void reserve(size_t n) noexcept {
            if (n > _capacity) {
                _grow_to(n);
            }
        }

        void resize(size_t n) noexcept {
            reserve(n);
            if (n > _size) {
                // libc's and not fill_bytes: a zero fill of any length
                // (a scratch array holds up to megabytes), where libc
                // clears whole cache lines and fill_bytes measured up to
                // twice as slow (DESIGN 393)
                std::memset(static_cast<void*>(_data + _size), 0, (n - _size) * sizeof(T));
            }
            _size = n;
        }

        void assign(size_t n, const T& value) noexcept {
            _size = 0;
            reserve(n);
            for (size_t i = 0; i < n; ++i) {
                _data[i] = value;
            }
            _size = n;
        }

        template<class It>
        void assign(It first, It last) noexcept {
            _size = 0;
            append(first, last);
        }

        void push_back(const T& value) noexcept {
            if (_size == _capacity) [[unlikely]] {
                _grow_to(_size + 1);
            }
            _data[_size++] = value;
        }

        template<class... A>
        T& emplace_back(A&&... a) noexcept(noexcept(T{std::forward<A>(a)...})) {
            push_back(T{std::forward<A>(a)...});
            return back();
        }

        void pop_back() noexcept {
            --_size;
        }

        // The elements [first, last) at the end
        template<class It>
        void append(It first, It last) noexcept {
            size_t n = size_t(last - first);
            reserve(_size + n);
            for (size_t i = 0; i < n; ++i) {
                _data[_size + i] = first[i];
            }
            _size += n;
        }

        // One element in front of `at`, the rest moved up
        void insert(const T* at, const T& value) noexcept {
            size_t i = size_t(at - _data);
            T copy = value;
            push_back(copy);
            // the two runs overlap by all but one element
            sgcl::detail::move_bytes(_data + i + 1, _data + i, (_size - 1 - i) * sizeof(T));
            _data[i] = copy;
        }

        friend bool operator==(const scratch_vector& a, const scratch_vector& b) noexcept {
            return a._size == b._size && (a._size == 0 || std::memcmp(a._data, b._data, a._size * sizeof(T)) == 0);
        }

    private:
        SGCL_NOINLINE void _grow_to(size_t n) noexcept {
            size_t cap = _capacity * 2;
            if (cap < n) {
                cap = n;
            }
            if (cap < 16) {
                cap = 16;
            }
            void* p = std::realloc(_data, cap * sizeof(T));
            if (!p) [[unlikely]] {
                sgcl::detail::os::memory_refused("a scratch array", cap * sizeof(T));
            }
            _data = static_cast<T*>(p);
            _capacity = cap;
        }

        T* _data = nullptr;
        size_t _size = 0;
        size_t _capacity = 0;
    };

    template<class T>
    struct is_scratch_vector : std::false_type {};

    template<class T>
    struct is_scratch_vector<scratch_vector<T>> : std::true_type {};

    // Every array of a scratch, handed to f: a scratch_vector is its own
    // one, a struct of them has a lent_each of its own, found by argument
    // dependent lookup
    template<class T, class F>
    void lent_arrays(T& value, F&& f) noexcept {
        if constexpr (is_scratch_vector<T>::value) {
            f(value);
        } else {
            lent_each(value, f);
        }
    }

    // The bytes the slots of every kind keep on this thread while they
    // are not lent
    inline size_t& lent_kept_bytes() noexcept {
        static thread_local size_t bytes = 0;
        return bytes;
    }

    // A T of the thread's for as long as this lives. T is a scratch_vector
    // or a struct of them (lent_arrays).
    template<class T>
    class lent {
    public:
        lent() noexcept {
            for (auto& s : _slots()) {
                if (!s.busy) {
                    s.busy = true;
                    lent_kept_bytes() -= s.kept;
                    s.kept = 0;
                    _held = &s;
                    _value = &s.value;
                    return;
                }
            }
            _value = &_own.emplace();
        }

        lent(const lent&) = delete;
        lent& operator=(const lent&) = delete;

        // What goes back is emptied, and kept only while the slot and
        // the thread stay within their bounds
        ~lent() {
            if (!_held) {
                return;
            }
            size_t bytes = 0;
            lent_arrays(_held->value, [&](auto& v) {
                bytes += v.capacity() * sizeof(*v.data());
            });
            auto& kept = lent_kept_bytes();
            if (bytes > LentKeep || kept + bytes > LentThreadKeep) {
                _held->value = T{};
            } else {
                lent_arrays(_held->value, [](auto& v) {
                    v.clear();
                });
                _held->kept = bytes;
                kept += bytes;
            }
            _held->busy = false;
        }

        T& operator*() const noexcept {
            return *_value;
        }

        T* operator->() const noexcept {
            return _value;
        }

    private:
        struct slot {
            T value {};
            size_t kept = 0;     // the bytes it holds while it is not lent
            bool busy = false;
        };

        static std::array<slot, LentSlots>& _slots() noexcept {
            static thread_local std::array<slot, LentSlots> slots;
            return slots;
        }

        slot* _held = nullptr;
        T* _value = nullptr;
        optional<T> _own;
    };
}
