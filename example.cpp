//------------------------------------------------------------------------------
// SGCL - a real-time Garbage Collector for C++
// Copyright (c) 2022-2024 Sebastian Nibisz
// SPDX-License-Identifier: Zlib
//------------------------------------------------------------------------------

#include "sgcl/sgcl.h"
#include <iostream>

int main() {
    using namespace sgcl;

    // unique pointer, deterministic destruction
    auto unique = make_tracked<int>(1);
    unique_ptr<int> unique2 = make_tracked<int>(2);
    std::unique_ptr<int, unique_deleter> unique3 = make_tracked<int>(3);

    // shared pointer, deterministic destruction
    std::shared_ptr<int> shared = make_tracked<int>(4);

    // root pointer, non-deterministic destruction in GC thread
    root_ptr<int> root = make_tracked<int>(5);

    // tracked pointer, non-deterministic destruction in GC thread
    struct node {
        tracked_ptr<node> next;
        int value;
    };
    root_ptr<node> head = make_tracked<node>();

    // undefined behavior, tracked_ptr must be allocated on the managed heap
    // tracked_ptr<node> head = make_tracked<node>();
    // node head;
    // node* head = new node;

    // move unique to root
    root = std::move(unique);

    // move unique to shared
    shared = std::move(unique);

    // root to tracked
    head->next = head;

    // tracked to root
    head = head->next;

    // not allowed
    // root = shared;
    // shared = root;

    // copy pointer
    auto ref = root;

    // clone object
    auto clone = root.clone();

    // create pointer based on raw pointer
    // only pointers to memory allocated on the managed heap
    root_ptr<node> root_ref(head.get());

    // create alias
    // only pointers to memory allocated on the managed heap, excluding arrays
    root_ptr<int> value(&head->value);

    // create and init array
    root_ptr<int[]> array = make_tracked<int[]>(5, 7);

    // array iteration
    for (auto v: array) {
        std::cout << v << " ";
    }
    std::cout << std::endl;

    // cast pointer
    root_ptr<void> any = root;
    root = static_pointer_cast<int>(any);
    if (any.is<int>() || any.type() == typeid(int)) {
        root = any.as<int>();
    }

    // atomic pointer
    atomic<root_ptr<int>> atomic = root;

    // metadata using
    // metadata structure is defined in the configuration.h file
    struct: metadata {
        void test() override {
            std::cout << "metadata<int>.test()\n";
        }
    } static mdata;
    metadata::set<int>(&mdata);

    any.metadata()->test();

    // terminate collector (optional, not required)
    collector::terminate();
}
