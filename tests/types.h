//------------------------------------------------------------------------------
// SGCL: Smart Garbage Collection Library
// Copyright (c) 2022-2025 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "sgcl/sgcl.h"

#include <gtest/gtest.h>

using namespace sgcl;

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

    bool operator==(int value) const noexcept {
        return (_value <=> value) == 0;
    }

    std::strong_ordering operator<=>(int value) const noexcept {
        return (_value <=> value);
    }

    inline static size_t counter = 0;

private:
    int _value;
};
