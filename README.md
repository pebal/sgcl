# SGCL
## About the SGCL
SGCL (Smart Garbage Collection Library) is an advanced memory management library for C++, designed with performance and ease of use in mind. SGCL introduces fully tracked smart pointers, providing an experience similar to shared_ptr but with added mechanisms for automatic garbage collection and optimization. Tailored for the modern C++ standards (C++17 and later), SGCL aims to facilitate safer and more efficient memory management without the overhead typically associated with garbage collection techniques.
## Why SGCL?

SGCL was created to address specific scenarios where `unique_ptr` and `shared_ptr` fall short. These standard smart pointers are versatile, but there are cases in complex applications where they might not be the optimal choice. Here’s why SGCL can be a game-changer:

- **Handling Local Ownership Cycles:** In complex object graphs, objects may refer to each other in cycles, creating ownership loops that `shared_ptr` cannot resolve without additional intervention. SGCL offers a solution for managing local cycles of ownership efficiently, without the risk of memory leaks.

- **Performance Overheads with `shared_ptr`:** While `shared_ptr` is incredibly useful for shared ownership scenarios, its performance can be impacted by the overhead of reference counting, especially in multi-threaded environments. SGCL introduces mechanisms that optimize memory management and reduce overhead, making it more suitable for high-performance applications.

- **Real-time Requirements:** In systems with stringent real-time requirements, the non-deterministic timing of destructors (as objects go out of scope or are explicitly deleted) can pose a problem. SGCL allows for delayed execution of destructors, enabling better control over when resource cleanup occurs and ensuring that real-time performance criteria are met.

SGCL is designed for developers who need more control over memory management than what is offered by standard C++ smart pointers. By providing advanced features for cycle detection, efficient resource management, and deterministic destructor execution, SGCL opens new possibilities for designing complex, high-performance applications.
## Features
- **Thread-Safe Operations** 

    SGCL is designed for safe concurrent access, making it ideal for multi-threaded applications without the risk of data races.

- **Zero Reference Counting** 
    
    Unlike many smart pointer implementations, SGCL avoids the overhead and complexity of reference counters, leading to improved performance and lower memory usage.

- **Simplicity and Familiarity** 
    
    Utilizes an intuitive API, making the transition for developers accustomed to `shared_ptr` seamless and straightforward.

- **Reduced Memory Overhead** 

    Optimized to consume less memory than `shared_ptr`, facilitating more efficient resource usage in your applications.

- **Performance Optimized** 

    Benchmarks demonstrate that SGCL outperforms `shared_ptr` in various scenarios, ensuring faster execution times for your projects.

- **Pauseless Garbage Collection** 

    Designed to avoid "stop-the-world" pauses, SGCL ensures continuous application performance without disruptive GC interruptions.

- **Support for Cyclic Data Structures** 

    Handles cyclic references gracefully, eliminating the common pitfalls associated with manual memory management in complex structures.

- **Copy-On-Write (CoW) Optimization** 

    SGCL is CoW-friendly, supporting efficient memory usage patterns and optimizing scenarios where cloned data structures defer copying until mutation.

- **Lock-Free Atomic Pointers**

    Guarantees that atomic pointer operations are always lock-free, enhancing performance in concurrent usage scenarios.

These features make SGCL a robust and versatile choice for developers seeking to optimize their C++ applications with advanced garbage collection and memory management techniques, all while maintaining high performance and ease of use.
## SGCL pointers
SGCL introduces four types of smart pointers.

- `root_ptr, unique_ptr`

    These pointers can be utilized on the stack, heap, or managed heap. They serve as roots in the application's object graph. The unique_ptr provides a deterministic destruction mechanism.

- `tracked_ptr` 

    Crafted to be a part of structures or arrays created via make_tracked. Contrary to root_ptr and unique_ptr, tracked_ptr is designed for exclusive use in the managed heap.
    
- `unsafe_ptr`

    A pointer for accessing objects without ownership management. It is designed for performance optimization where direct access is needed and lifecycle is managed elsewhere.

## The make_tracked method
The `make_tracked` method is dedicated method for creating objects on the managed heap. This method returns a unique_ptr.
## Example
```cpp
#include "sgcl/sgcl.h"
#include <iostream>

int main() {
    using namespace sgcl;
    
    // Creating unique_ptr with deterministic destruction
    auto unique = make_tracked<int>(42);
    
    // Creating shared_ptr with deterministic destruction
    std::shared_ptr<int> shared = make_tracked<int>(1337);
    
    // Creating root_ptr, which does not have deterministic destruction (managed by GC)
    root_ptr<int> root = make_tracked<int>(2024);
    
    // Example of using root_ptr with a custom data type
    struct Node {
        tracked_ptr<Node> next;
        int value;
    };
    root_ptr<Node> node = make_tracked<Node>();
    node->value = 10; // Direct field access
    
    // Simple pointer operations
    root = std::move(unique); // Moving ownership from unique_ptr to root_ptr
    shared = make_tracked<int>(2048); // Assigning a new value to shared_ptr
    
    // Demonstrating array handling
    auto array = make_tracked<const int[]>({ 1, 2, 3 }); // Creating an array and initialization
    for (auto& elem: array) {
        std::cout << elem << " "; // Iteration and printing
    }
    std::cout << std::endl;
    
    // Creating a pointer alias
    // Note: only pointers to memory allocated on the managed heap, excluding arrays
    root_ptr<int> value(&node->value);
    
    // Using unsafe_ptr to compare and return the pointer to the minimum value
    auto min = [](unsafe_ptr<int> l, unsafe_ptr<int> r) -> unsafe_ptr<int> {
        return *l < *r ? l : r;
    };
    root_ptr<int> pmin = min(value, root);
    std::cout << "min: " << *pmin << std::endl;
    
    // Using unsafe_ptr for comparison and passing the result via reference to tracked_ptr
    auto max = [](unsafe_ptr<int> l, unsafe_ptr<int> r, tracked_ptr<int>& out) {
        out = *l > *r ? l : r;
    };
    root_ptr<int> pmax;
    max(value, root, pmax);
    std::cout << "max: " << *pmax << std::endl;
    
    // Using an atomic pointer
    atomic<root_ptr<int>> atomicRoot = make_tracked<int>(1234);
    
    // Using pointer casting
    root_ptr<void> any = root;
    root = static_pointer_cast<int>(any);
    if (any.is<int>() || any.type() == typeid(int)) {
        root = any.as<int>();
    }
    
    // Metadata usage
    // The metadata structure is defined in the configuration.h file
    struct: metadata {
        void to_string(void* p) override {
            std::cout << "to_string<int>: " << *static_cast<int*>(p) << std::endl;
        }
    } static mdata;
    metadata::set<int>(&mdata);
    any.metadata()->to_string(any.get()); // Currently any pointed a value of type int
}
```
## Methods useful for state analysis
```cpp
// Forcing garbage collection
collector::force_collect();

// Forcing garbage collection and waiting for the cycle to complete
collector::force_collect(true);

// Get the number of live objects
auto live_object_number = collector::live_object_count();
std::cout << "live object number: " << live_object_number << std::endl;

// Get an array of live objects
auto live_objects = collector::live_objects();
for (auto& v: live_objects) {
    std::cout << v.get() << " " << v.type().name() << std::endl;
}

// Terminate collector
collector::terminate();
```
## Dependencies
This library is written in C++17 and a compliant compiler is necessary. 

No external library is necessary and there are no other requirements.
## Usage
This is a header only library. You can just copy the `sgcl` subfolder somewhere in your include path.
