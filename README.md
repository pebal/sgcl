# SGCL
## About SGCL
SGCL (Smart Garbage Collection Library) is an advanced memory management library for C++ designed with performance and ease of use in mind. SGCL introduces fully tracked smart pointers, providing an experience similar to shared_ptr, but with additional automatic garbage collection and optimization mechanisms. Aligned with modern C++ standards (C++20 and later), SGCL aims to facilitate safer and more efficient memory management without the overhead typically associated with garbage collection techniques.
## Why SGCL?

SGCL was created to address specific scenarios where `unique_ptr` and `shared_ptr` fail. These standard smart pointers are versatile, but may not be the optimal choice for complex applications. Here's why SGCL could be a game-changer:

- **Support for local ownership cycles:** In complex object graphs, objects can reference each other in cycles, creating ownership loops that `shared_ptr` cannot resolve without additional intervention. SGCL offers a solution to efficiently manage local ownership cycles, without the risk of memory leaks.

- **Performance overhead with `shared_ptr`:** While `shared_ptr` is extremely useful in shared ownership scenarios, its performance can be limited by the overhead of reference counting, especially in multithreaded environments. SGCL introduces mechanisms that optimize memory management and reduce overhead, making it more suitable for high-performance applications.

- **Real-time requirements:** In systems with stringent real-time requirements, deterministic destructor calls (when objects go out of scope or are manually deleted) can be a problem. SGCL enables delayed execution of destructors, thereby offloading the main application thread and making it easier to meet real-time performance criteria.
## GC Engine
SGCL uses a mark-and-sweep algorithm to manage memory. The use of a cheap write barrier made it possible to make the algorithm fully concurrent, efficient, and pause-free. Managed objects are never moved around in memory.
## Features
- **Zero reference counting**

    Unlike many smart pointer implementations, SGCL avoids the overhead and complexity of reference counts, leading to improved performance.

- **Simplicity and familiarity**

    It uses an intuitive API, making the transition smooth and easy for developers accustomed to `shared_ptr`.

- **Reduced memory overhead**

    Optimized to use less memory than `shared_ptr`, facilitating more efficient resource utilization in applications.

- **Optimized for performance**

    Benchmarks show that SGCL outperforms `shared_ptr` in a variety of scenarios, providing faster execution times.

- **Garbage Collection Without Pauses**

    SGCL is designed to run fully concurrently, ensuring continuous application performance without disruptive GC pauses.

- **Support for Cyclic Data Structures**

    Supports cyclic references, eliminating the common pitfalls of manually managing memory in complex structures.

- **Lock-Free Atomic Pointers**

    Ensures that operations on atomic pointers are always lock-free, improving performance in concurrent scenarios.
## SGCL Pointers
SGCL introduces four types of smart pointers.
    
  - `UniquePtr`
    
    This pointer can be used on the stack, heap or managed heap, and serves as the root of the application's object graph. It is a specialization of std::unique_ptr and, like it, provides deterministic object destruction.
    
  - `StackPtr`
    
    This pointer can be used on the stack only. Similar to UniquePtr it is serves as the root of the application's object graph. It provides non-deterministic destruction.
    
  - `TrackedPtr`
    
    Created to be part of structures or arrays created with make_tracked. TrackedPtr is intended for exclusive use on the managed heap.
    
  - `UnsafePtr`
    
    A pointer for accessing objects without managing ownership. It is intended for performance optimization where direct access is required and the lifecycle is managed elsewhere.
    
## make_tracked method
The `make_tracked` method is dedicated for creating objects and arrays on the managed heap. This method returns a UniquePtr.

## Example
```cpp
#include "sgcl/sgcl.h"

#include <iostream>

int main() {
    using namespace sgcl;

    // Creating managed object
    // Note: 'make_tracked' returns a unique pointer
    make_tracked<int>();

    // Using unique pointer with deterministic destruction
    UniquePtr<int> unique = make_tracked<int>(42);

    // Using shared pointer with deterministic destruction
    std::shared_ptr<int> shared = make_tracked<int>(13);

    // Using stack pointer without deterministic destruction
    // Note: destructor will be called in a GC thread
    StackPtr<int> stack = make_tracked<int>(24);
    Pointer<int, PointerPolicy::Stack> also_stack;

    // Invalid, tracked pointer cannot be on the stack
    // TrackedPtr<int> tracked;
    // Pointer<int, PointerPolicy::Tracked> also_tracked;

    // Reference to tracked pointer
    // Note: stack pointer can be cast to tracked pointer reference
    // Note: tracked pointer reference can be on the stack
    TrackedPtr<int>& tracked_ref = stack;
    Pointer<int, PointerPolicy::Tracked>& also_tracked_ref = stack;

    // Tracked pointer in the managed object
    struct Node {
        TrackedPtr<Node> next;
        int value = 5;
    };
    StackPtr<Node> node = make_tracked<Node>();

    // Invalid, 'Node::next' cannot be on the stack
    // Node stack_node;

    // Undefined behavior, 'Node::next' cannot be on the unmanaged heap
    // auto node = new Node;

    // If you need a universal Node class
    // template<PointerPolicy Policy = PointerPolicy::Tracked>
    // struct Node {
    //     Pointer<Node, Policy> next;
    //     int value = 5;
    // };
    // Node<PointerPolicy::Stack> stack_node;
    // auto node = make_tracked<Node>();

    // Creating a pointer alias
    // Note: 'value' must be in the managed memory
    // Note: undefined behavior when 'node' is array
    StackPtr<int> value(&node->value);

    // Moving from unique pointer to the stack pointer
    // Note: now destructor will be called in a GC thread
    stack = std::move(unique);

    // Creating managed arrays
    auto arr = make_tracked<int[]>(10);
    arr = make_tracked<int[]>(10, 0);
    arr = make_tracked<int[]>({ 1, 2, 3 });

    // Iterating over the array
    std::cout << "arr: ";
    for (auto v: arr) {
        std::cout << v << " ";
    }
    for (auto i = arr.rbegin(); i < arr.rend(); ++i) {
        std::cout << *i << " ";
    }
    for (auto i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << " ";
    }
    std::cout << std::endl;

    // Array casting
    struct Bar {
        int value;
    };
    struct Foo : Bar {
        void set_value(int v) {
            value = v;
            Bar::value = v * v;
        }
        int value;
    };
    StackPtr<Foo[]> foo = make_tracked<Foo[]>(5);

    // Casting 'foo' to base class
    // Note: this is safe in the SGCL
    StackPtr<Bar[]> bar = foo;
    for (int i = 0; i < foo.size(); ++i) {
        foo[i].set_value(i + 1);
    }
    std::cout << "foo: ";
    for (auto& f: foo) {
        std::cout << f.value << " ";
    }
    std::cout << std::endl << "bar: ";
    for (auto& b: bar) {
        std::cout << b.value << " ";
    }
    std::cout << std::endl;

    // Casting array to object
    StackPtr<Foo> first_foo = foo;

    // Casting object to array
    StackPtr<int[]> single_value_array = make_tracked<int>(12);

    // Using unsafe pointer to compare and return the pointer to the minimum value
    // Note: unsafe pointer is not tracked by GC
    auto fmin = [](UnsafePtr<int> l, UnsafePtr<int> r) -> UnsafePtr<int> {
        return *l < *r ? l : r;
    };
    StackPtr<int> min = fmin(value, stack);
    std::cout << "min: " << *min << std::endl;

    // Using unsafe pointer for comparison and passing the result via reference to tracked pointer
    auto fmax = [](UnsafePtr<int> l, UnsafePtr<int> r, TrackedPtr<int>& out) {
        out = *l > *r ? l : r;
    };
    StackPtr<int> max;
    fmax(value, stack, max);
    std::cout << "max: " << *max << std::endl;

    // Using an atomic pointer
    Atomic<StackPtr<int>> atomicRoot = make_tracked<int>(2);

    // Check pointer type
    StackPtr<void> any = make_tracked<char>();
    std::cout << "any " << (any.is<int>() ? "is" : "is not") <<  " int" << std::endl;
    std::cout << "any " << (any.type() == typeid(char) ? "is" : "is not") <<  " char" << std::endl;
    // Casting
    StackPtr<char> c = any.as<char>();
    c = static_pointer_cast<char>(any);

    // Cloning
    auto clone = c.clone();

    // Metadata usage
    set_metadata<int>(new std::string("int metadata"));
    set_metadata<double>(new std::string("double metadata"));
    any = make_tracked<int>();
    std::cout << *any.metadata<std::string>() << std::endl;
    any = make_tracked<double>();
    std::cout << *any.metadata<std::string>() << std::endl;

    // Using containers for tracked objects (vector, list, map, etc.)
    // Note: Currently only root containers are available
    // Note: Root containers do not support cycle detection in data structures
    //       when they are integrated into such structures
    RootVector<int> vec;
    RootList<int> list;
    // or
    std::vector<Node, RootAllocator<Node>> same_vec;
    std::list<Node, RootAllocator<Node>> same_list;

    // Undefined behavior, 'Node::next' cannot be on the unmanaged heap
    //std::vector<Node> vec;
}
```
## Methods useful for state analysis
```cpp
// Forcing garbage collection
Collector::force_collect();

// Forcing garbage collection and waiting for the cycle to complete
Collector::force_collect(true);

// Get number of living objects
auto living_objects_number = Collector::living_objects_number();
std::cout << "living objects number: " << living_objects_number << std::endl;

{
    // Get list of living objects
    // Note: A full GC cycle is performed before returning the data
    // Note: Pause guard and vector of raw pointers is returned
    //       The GC engine is paused until the pause guard is destroyed
    auto [pause_guard, living_objects] = Collector::living_objects();
    for (auto& v: living_objects) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
} // The pause guard is destroyed at this point

// Terminate collector
// Note: This call is optional
Collector::terminate();
```
## Dependencies
This library is written in C++20 and a compliant compiler is necessary. 

No external library is necessary and there are no other requirements.
## Usage
This is a header only library. You can just copy the `sgcl` subfolder somewhere in your include path.
