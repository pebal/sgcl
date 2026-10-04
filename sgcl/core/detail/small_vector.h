//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "bytes.h"
#include "os.h"

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>

namespace sgcl::detail {
    // The default memory of a SmallVector past its inline part: plain
    // memory from ::operator new, nothing zeroed
    struct PlainPolicy {
        SGCL_INLINE_HOT static void* allocate(size_t bytes) {
            return ::operator new(bytes);
        }

        SGCL_INLINE_HOT static void deallocate(void* p, size_t bytes) noexcept {
            ::operator delete(p, bytes);
        }

        // Bytes let go of (a truncation, a move's source, the inline part
        // a growth leaves): nothing to do, and nothing compiled
        SGCL_INLINE_HOT static void wipe(void*, size_t) noexcept {
        }
    };

    // A vector whose first N elements live in the object, the whole of it
    // moved to one block of the policy's memory when it outgrows them:
    // contiguous in both states, data() one pointer. For scratch buffers
    // that die inside a call and nearly always fit (the segments of a
    // path) and for bytes that must never reach managed memory
    // (crypto::secret_bytes, with a policy that zeroes everything it lets
    // go of).
    //
    // Not for the collator's buffers (txt/collate.h keeps its own, the
    // inline part and a std::vector of the rest behind it): there every
    // element of a comparison is written and read in a loop of a few
    // dozen nanoseconds, and the test of the block in data() and
    // push_back made a comparison of two words 9% dearer (measured
    // 2026-09-28, A/B with the growth already out of line), where the
    // split buffer indexes the inline part by i < N alone.
    //
    // Nothing it holds is a pointer the collector must know about: T is
    // trivially copyable and destructible, which a tracked_ptr is not, nor
    // anything that holds one. The block is plain memory, freed by the
    // destructor; the collector never scans it.
    //
    // The inline part is left uninitialised, so it costs its room and
    // nothing else, and a copy takes what has been written and not the
    // whole array (the collator's buffer learned why: the compiler's own
    // copy of a pattern's elements carried two kilobytes of indeterminate
    // bytes for a handful, and where such a copy lands inside a managed
    // object the collector reads those bytes as words, a stale address
    // among them a root that keeps something dead alive).
    //
    // One pointer to the elements, the inline part's while they fit: data()
    // is one load and push_back one compare.
    //
    // The policy is three hooks: allocate(bytes), deallocate(p, bytes) and
    // wipe(p, bytes), the last called on every byte of elements the vector
    // lets go of without freeing — a truncation (resize down, clear,
    // erase_front's vacated end), the inline elements of a move's source,
    // the inline elements a growth leaves, the destructor's inline
    // elements; a block goes back through deallocate, which the wiping
    // policy zeroes too (crypto/secure_zero.h: WipingPolicy).
    template<class T, size_t N, class Policy = PlainPolicy>
    class SmallVector {
        static_assert(N > 0, "SmallVector of no inline elements");
        static_assert(std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>,
                      "SmallVector holds no tracked words: the inline part and the block are never scanned");
        static_assert(alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__, "SmallVector's block is ::operator new's");

    public:
        static constexpr size_t inline_capacity = N;

        // Empty, the inline part untouched (a constructor of its own, so
        // that a value-initialisation does not zero it either)
        SGCL_INLINE_HOT SmallVector() noexcept {
        }

        // n value-initialised elements (zero bytes)
        SGCL_INLINE_HOT explicit SmallVector(size_t n) {
            resize(n);
        }

        SGCL_INLINE_HOT SmallVector(const SmallVector& other) {
            _copy(other);
        }

        // Another's block taken as it is, or its inline elements copied and
        // wiped there; it is left empty
        SGCL_INLINE_HOT SmallVector(SmallVector&& other) noexcept {
            _take(other);
        }

        SGCL_INLINE_HOT SmallVector& operator=(const SmallVector& other) {
            if (this != &other) {
                _copy(other);
            }
            return *this;
        }

        SGCL_INLINE_HOT SmallVector& operator=(SmallVector&& other) noexcept {
            if (this != &other) {
                _release();
                _take(other);
            }
            return *this;
        }

        SGCL_INLINE_HOT ~SmallVector() {
            _release();
        }

        SGCL_INLINE_HOT size_t size() const noexcept {
            return _size;
        }

        SGCL_INLINE_HOT bool empty() const noexcept {
            return _size == 0;
        }

        SGCL_INLINE_HOT size_t capacity() const noexcept {
            return _capacity;
        }

        SGCL_INLINE_HOT T* data() noexcept {
            return _heap;
        }

        SGCL_INLINE_HOT const T* data() const noexcept {
            return _heap;
        }

        SGCL_INLINE_HOT T& operator[](size_t i) noexcept {
            assert(i < _size);
            return data()[i];
        }

        SGCL_INLINE_HOT const T& operator[](size_t i) const noexcept {
            assert(i < _size);
            return data()[i];
        }

        SGCL_INLINE_HOT T* begin() noexcept {
            return data();
        }

        SGCL_INLINE_HOT T* end() noexcept {
            return data() + _size;
        }

        SGCL_INLINE_HOT const T* begin() const noexcept {
            return data();
        }

        SGCL_INLINE_HOT const T* end() const noexcept {
            return data() + _size;
        }

        // Past the capacity a block of twice it (twice N the first time).
        // An element that fits costs one compare, the growth out of line
        SGCL_INLINE_HOT void push_back(const T& x) {
            if (_size < _capacity) [[likely]] {
                _heap[_size++] = x;
                return;
            }
            _push_grown(x);
        }

        // n elements from p, which is not inside this vector; past the
        // capacity a block of the larger of twice it and what is needed
        SGCL_INLINE_HOT void append(const T* p, size_t n) {
            if (n > capacity() - _size) {
                size_t need = _size + n;
                size_t twice = 2 * capacity();
                _reallocate(need > twice ? need : twice, _size);
            }
            if (n) {
                detail::copy_bytes(data() + _size, p, n * sizeof(T));
            }
            _size += n;
        }

        // n elements: the first min(n, size()) kept, the rest
        // value-initialised; a shrink wipes the elements it drops and keeps
        // the room; past the capacity a block of exactly n
        void resize(size_t n) {
            if (n > capacity()) {
                _reallocate(n, _size);
            }
            T* d = data();
            if (n < _size) {
                Policy::wipe(d + n, (_size - n) * sizeof(T));
            } else {
                for (size_t i = _size; i < n; ++i) {
                    ::new (static_cast<void*>(d + i)) T();
                }
            }
            _size = n;
        }

        // Room for n elements: a block of exactly n when past the capacity
        SGCL_INLINE_HOT void reserve(size_t n) {
            if (n > capacity()) {
                _reallocate(n, _size);
            }
        }

        // Empty, the elements wiped, the block kept
        SGCL_INLINE_HOT void clear() noexcept {
            Policy::wipe(data(), _size * sizeof(T));
            _size = 0;
        }

        // The first n elements dropped, the rest moved down
        SGCL_INLINE_HOT void erase_front(size_t n) noexcept {
            assert(n <= _size);
            T* d = data();
            detail::move_bytes(d, d + n, (_size - n) * sizeof(T));   // the two runs overlap when n < size() - n
            Policy::wipe(d + (_size - n), n * sizeof(T));
            _size -= n;
        }

    private:
        // push_back past the capacity: the block of twice it, then x
        SGCL_NOINLINE void _push_grown(const T& x) {
            const T copy = x;   // x may be one of the elements the growth moves
            _reallocate(2 * capacity(), _size);
            _heap[_size++] = copy;
        }

        T _inline[N];            // left uninitialised on purpose: _size says what has been written
        T* _heap = _inline;      // points into this object while inline; the move constructor re-targets it
        size_t _capacity = N;
        size_t _size = 0;

        SGCL_INLINE_HOT bool _on_heap() const noexcept {
            return _heap != _inline;
        }

        // A block of cap elements, the first keep elements copied into it;
        // what held them before let go of (the block deallocated, the
        // inline elements wiped)
        void _reallocate(size_t cap, size_t keep) {
            T* block = static_cast<T*>(Policy::allocate(cap * sizeof(T)));
            if (keep) {
                detail::copy_bytes(block, _heap, keep * sizeof(T));
            }
            if (_on_heap()) {
                Policy::deallocate(_heap, _capacity * sizeof(T));
            } else {
                Policy::wipe(_inline, _size * sizeof(T));
            }
            _heap = block;
            _capacity = cap;
        }

        // Another's elements, as many as it has written: in place when
        // they fit, else in a block of exactly their number
        SGCL_INLINE_HOT void _copy(const SmallVector& other) {
            if (other._size > capacity()) {
                _reallocate(other._size, 0);
            } else if (_size > other._size) {
                Policy::wipe(data() + other._size, (_size - other._size) * sizeof(T));
            }
            if (other._size) {
                detail::copy_bytes(data(), other.data(), other._size * sizeof(T));
            }
            _size = other._size;
        }

        // Another's block taken as it is, or its inline elements copied
        // and wiped there (this one inline then: _heap at its own _inline)
        void _take(SmallVector& other) noexcept {
            if (other._on_heap()) {
                _heap = other._heap;
                _capacity = other._capacity;
                other._heap = other._inline;
                other._capacity = N;
            } else {
                if (other._size) {
                    detail::copy_bytes(_inline, other._inline, other._size * sizeof(T));
                    Policy::wipe(other._inline, other._size * sizeof(T));
                }
                _heap = _inline;
                _capacity = N;
            }
            _size = other._size;
            other._size = 0;
        }

        // What it holds let go of: the block deallocated, else the inline
        // elements wiped; empty after
        SGCL_INLINE_HOT void _release() noexcept {
            if (_on_heap()) {
                Policy::deallocate(_heap, _capacity * sizeof(T));
                _heap = _inline;
                _capacity = N;
            } else {
                Policy::wipe(_inline, _size * sizeof(T));
            }
            _size = 0;
        }
    };
}
