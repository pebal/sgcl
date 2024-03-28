# SGCL
## About the SGCL
SGCL is a real-time garbage collector for C++. Provides fully tracked smart pointers, similar in use to shared_ptr.
## Features
- Thread safe
- Does not use reference counters
- Easy to use like shared_ptr
- Less memory overhead than shared_ptr
- Faster than shared_ptr
- Never stop the world
- Cyclic data structures friendly
- CoW friendly
- Atomic pointers always lock-free

## Compiling
C++17 compiler required. Tested on Windows with VC++, Clang and MinGW compilers.
## Example
```cpp
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
    auto array = make_tracked<int[]>(5, 7);

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
        void to_string(void* p) override {
            std::cout << "to_string<int>: " << *static_cast<int*>(p) << std::endl;
        }
    } static mdata;
    metadata::set<int>(&mdata);
    any.metadata()->to_string(any.get());

    // Methods useful for state analysis and supporting testing processes.
    {
        // force cellect
        collector::force_collect();
    
        // force collect and wait for the cycle to complete
        collector::force_collect(true);
    
        // get number of living objects
        auto live_object_number = collector::live_object_count();
        std::cout << "live object number: " << live_object_number << std::endl;
    
        // get live objects
        auto live_objects = collector::live_objects();
        for (auto& v: live_objects) {
            std::cout << v.get() << " " << v.type().name() << std::endl;
        }
    
        // terminate collector
        collector::terminate();
    }
}
```
