//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "sgcl/sgcl.h"
#include "tests/expected_access.h"

#include <gtest/gtest.h>

using namespace sgcl;

// The stack is scanned conservatively, so a raw pointer or an iterator left
// in the test's own frame keeps its target alive. Code that must not leave
// such words behind runs in a frame of its own; collector::clear_stack() then
// overwrites it before a count. A pointer kept across a count is hidden.
template<class F>
SGCL_NOINLINE void off_frame(F&& f) {
    f();
}

inline uintptr_t hide(const void* p) noexcept {
    return ~(uintptr_t)p;
}

inline char* unhide(uintptr_t h) noexcept {
    return (char*)~h;
}

// The managed bytes n calls of f allocate: the pages the allocators take
// from the heap while the collector stands parked, so that nothing is
// swept and no slot is handed out twice. n calls go first to fill the free
// slots earlier sweeps left. A count of pages: n must make what one call
// allocates add up to several of them.
template<class F>
SGCL_NOINLINE size_t managed_bytes_of(size_t n, F&& f) {
    using sgcl::detail::MemoryCounters;
    collector::force_collect(true);
    collector::stepper parked(true);
    parked.advance_to(collector::stepper::phase::start);
    for (size_t i = 0; i < n; ++i) {
        f();
    }
    auto before = MemoryCounters::alloc_since_cycle();
    for (size_t i = 0; i < n; ++i) {
        f();
    }
    auto pages = MemoryCounters::alloc_since_cycle() - before;
    parked.finish_cycle();
    return pages * config::page_size;
}

// The live objects and buffers, after a full cycle, whose type's name
// holds `part` (a type the test cannot name: a private or local struct);
// a buffer counts by its element type
inline size_t live_objects_named(const char* part) {
    size_t n = 0;
    for (auto& s : collector::get_type_statistics()) {
        if (std::string(s.type->name()).find(part) != std::string::npos) {
            n += s.live_objects;
        }
    }
    return n;
}

// The slot bytes of the live buffers of elements T after a full cycle (a
// buffer past a page counts its pages whole)
template<class T>
size_t live_buffer_bytes() {
    size_t bytes = 0;
    for (auto& s : collector::get_type_statistics()) {
        if (*s.type == typeid(T[]) && s.buffers) {
            bytes += s.live_bytes;
        }
    }
    return bytes;
}

// A sorted container's tree whole: the red-black invariants, the links,
// the order and the size (rb_tree.h: _check, private)
namespace sgcl::detail {
    struct RbTreeCheck {
        static bool of(const auto& tree) {
            return tree._check();
        }
    };
}

inline bool tree_is_valid(const auto& tree) {
    return sgcl::detail::RbTreeCheck::of(tree);
}

struct Bar {
    virtual ~Bar() = default;
    virtual int get_value() const { return 0; };
    virtual void set_value(int) {};
};

struct Baz : Bar {
    int value;

    Baz() {}

    Baz(int val)
    : value(val) {
    }

    ~Baz() {
        value = 0;
    }

    int get_value() const override {
        return value;
    }

    void set_value(int val) override {
        value = val;
    }
};

struct Faz {
    int value;

    virtual ~Faz() {
        value = 0;
    }

    void set_value(int val) {
        value = val;
    }
};

struct Far {
    int value;

    virtual ~Far() {
        value = 0;
    }

    void set_value(int val) {
        value = val;
    }
};

struct Foo : Far, Bar, Faz {
    int value;
    tracked_ptr<Baz> ptr;

    Foo() {
    }

    Foo(int val) {
        Foo::set_value(val);
    }

    ~Foo() {
        value = 0;
        ptr = nullptr;
    }

    int get_value() const override {
        return value;
    }

    void set_value(int val) override {
        value = val;
        ptr = make_tracked<Baz>(val);
        Far::set_value(val + 1);
        Faz::set_value(val + 2);
    }
};

struct Int {
    Int() noexcept
    : _value(0) {
        ++counter;
    }

    Int(int value) noexcept
    : _value(value) {
        ++counter;
    }

    Int(const Int& other) noexcept
    : _value(other._value) {
        ++counter;
    }

    ~Int() {
        --counter;
    }

    Int operator=(Int other) noexcept {
        _value = other._value;
        return *this;
    }

    operator int() const noexcept {
        return _value;
    }

    // Against an Int and against an int: the pair of a type that converts
    // to int needs the same-type overloads too, or C++20's reversed
    // candidates make `a == b` ambiguous (and a concept sees it as no ==)
    bool operator==(const Int& other) const noexcept {
        return _value == other._value;
    }

    std::strong_ordering operator<=>(const Int& other) const noexcept {
        return _value <=> other._value;
    }

    bool operator==(int value) const noexcept {
        return _value == value;
    }

    std::strong_ordering operator<=>(int value) const noexcept {
        return _value <=> value;
    }

    inline static size_t counter = 0;

private:
    int _value;
};
