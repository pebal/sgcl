//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "render.h"
#include "../../core/detail/os.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

// Attributes copied out of the call into memory of their own: what a
// logger's with() keeps, and a record's clone(). Everything a value
// pointed at is copied — a key, a text, the attributes of a group — and
// what is read through the program's operations is made data once: a
// type described by its fields becomes a group of them, a value that
// writes itself a string, any other value of the program (Any) its two
// texts, JSON and %+v (Owned). The copy points at nothing it does not
// own, so it may outlive the call and the objects the call named.
namespace sgcl::slog::detail {
    // Plain memory handed out in order and freed at once. The managed
    // object that owns it frees it in its destructor.
    class Arena {
    public:
        Arena() noexcept = default;
        Arena(const Arena&) = delete;
        Arena& operator=(const Arena&) = delete;

        ~Arena() {
            while (_head) {
                Block* next = _head->next;
                std::free(_head);
                _head = next;
            }
        }

        void* alloc(size_t n, size_t align = alignof(std::max_align_t)) noexcept {
            uintptr_t at = (uintptr_t(_at) + align - 1) & ~(uintptr_t(align) - 1);
            if (!_at || at + n > uintptr_t(_end)) {
                _block(n + align);
                at = (uintptr_t(_at) + align - 1) & ~(uintptr_t(align) - 1);
            }
            _at = reinterpret_cast<char*>(at + n);
            return reinterpret_cast<void*>(at);
        }

        const char* copy(const char* p, size_t n) noexcept {
            if (!n) {
                return "";
            }
            char* q = static_cast<char*>(alloc(n, 1));
            sgcl::detail::copy_bytes(q, p, n);
            return q;
        }

        Attr* attrs(size_t n) noexcept {
            if (!n) {
                return nullptr;
            }
            Attr* a = static_cast<Attr*>(alloc(n * sizeof(Attr), alignof(Attr)));
            for (size_t i = 0; i < n; ++i) {
                new (a + i) Attr();
            }
            return a;
        }

    private:
        struct Block {
            Block* next;
        };

        // A block the system would not give ends the program
        // (os::memory_refused), so alloc cannot throw
        void _block(size_t need) noexcept {
            size_t size = need + sizeof(Block) > 4096 ? need + sizeof(Block) : 4096;
            Block* b = static_cast<Block*>(std::malloc(size));
            if (!b) [[unlikely]] {
                sgcl::detail::os::memory_refused("a block of copied log attributes", size);
            }
            b->next = _head;
            _head = b;
            _at = reinterpret_cast<char*>(b + 1);
            _end = reinterpret_cast<char*>(b) + size;
        }

        Block* _head = nullptr;
        char* _at = nullptr;
        char* _end = nullptr;
    };

    inline void copy_attr(Arena& ar, Lines& w, const Attr& from, Attr& to, const Tail& t, int depth);

    // The length of a text cut to at most `most` bytes, back to the start
    // of a code point when the cut falls inside one: the text form of a
    // value of the program kept in a copy has its length in 32 bits
    // (Value::count), so a longer one is cut there rather than its length
    // wrapped
    inline size_t cut_text(const char* p, size_t n, size_t most) noexcept {
        if (n <= most) {
            return n;
        }
        size_t k = most;
        for (int back = 0; back < 3 && k > 0 && (uint8_t(p[k]) & 0xC0) == 0x80; ++back) {
            --k;
        }
        return k;
    }

    // The attributes of a group at depth, the call's in place of a
    // Splice, into an array of their own; a described type past the limit
    // left out, as the handlers leave it out. The depth is weighed once
    // per group; the array is counted for every child, so the few slots
    // of children left out stay unused. Out of line: inlined, the group's
    // loops made copy_value save every register for each plain value it
    // copies (with() of two values 5% slower)
    SGCL_NOINLINE inline void copy_children(Arena& ar, Lines& w, const Value& from, Value& to, const Tail& t, int depth) {
        const int d = child_depth(from, depth);
        const bool cut = d > MaxDepth;   // past the limit: the described types among the children left out
        size_t n = 0;
        for_each_child(from, t, [&](const Attr&) {
            ++n;
        });
        if (n > UINT32_MAX) {   // the count is 32 bits: the attributes past it are left out
            n = UINT32_MAX;
        }
        Attr* a = ar.attrs(n);
        size_t i = 0;
        for_each_child(from, t, [&](const Attr& c) {
            if (i < n && !(cut && c.value.kind == Kind::Record)) {
                copy_attr(ar, w, c, a[i], t, d);
                ++i;
            }
        });
        to.kind = Kind::Group;
        to.g = a;
        to.count = uint32_t(i);
    }

    inline void copy_value(Arena& ar, Lines& w, const Value& from, Value& to, const Tail& t, int depth) {
        switch (from.kind) {
            case Kind::String: {
                to.kind = Kind::String;
                to.s.p = ar.copy(from.s.p, from.s.n);
                to.s.n = from.s.n;
                return;
            }
            case Kind::Group:
            case Kind::Record:
                copy_children(ar, w, from, to, t, depth);
                return;
            case Kind::Formatted: {
                auto s = formatted(w.tmp, from);
                to.kind = Kind::String;
                to.s.p = ar.copy(s.data(), s.size());
                to.s.n = s.size();
                return;
            }
            case Kind::Any: {
                // both texts, one after the other: JSON, then %+v
                Buf& b = w.tmp;
                b.clear();
                JsonFail fail;
                if (!json_any(b, from.obj.p, from.obj.ops, nullptr, nullptr, 0, fail)) {
                    b.clear();
                    json_error(b, fail);
                }
                size_t json_n = b.size();
                go_text(b, from.obj.p, from.obj.ops, nullptr, nullptr, 0);
                const size_t text_n = cut_text(b.data() + json_n, b.size() - json_n, UINT32_MAX);
                to.kind = Kind::Owned;
                to.s.p = ar.copy(b.data(), json_n + text_n);
                to.s.n = json_n;
                to.count = uint32_t(text_n);
                return;
            }
            case Kind::Owned: {
                to = from;
                to.s.p = ar.copy(from.s.p, from.s.n + from.count);
                return;
            }
            case Kind::Splice:
                to.kind = Kind::Null;
                return;
            default:
                to = from;
                return;
        }
    }

    inline void copy_attr(Arena& ar, Lines& w, const Attr& from, Attr& to, const Tail& t, int depth) {
        to.key = ar.copy(from.key, from.key_n);
        to.key_n = from.key_n;
        copy_value(ar, w, from.value, to.value, t, depth);
    }

    // A list of attributes, the Splices kept as they are (a logger's own
    // are copied before any call gives its)
    inline Attr* copy_list(Arena& ar, Lines& w, const Attr* a, size_t n) {
        Attr* out = ar.attrs(n);
        for (size_t i = 0; i < n; ++i) {
            if (a[i].value.kind == Kind::Splice) {
                out[i] = a[i];
            } else {
                copy_attr(ar, w, a[i], out[i], Tail{}, 0);
            }
        }
        return out;
    }
}
