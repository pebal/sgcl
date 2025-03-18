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
    
  - `unique_ptr`
    
    A unique pointer to objects created on the managed heap. Provides deterministic destruction.
      
  - `tracked_ptr`
    
    A tracked pointer for objects created on the managed heap. The object's destructor is executed in the GC thread at non-deterministic time.
    
  - `atomic`
  
    Allows to create an atomic tracked pointer.
  
  - `atomic_ref`
  
    Provides atomic operations on tracked pointers.
    
## make_tracked method
The `make_tracked` method is dedicated for creating objects and arrays on the managed heap. This method returns a unique pointer.

## Example
```cpp
#include "sgcl/sgcl.h"
#include <iostream>

int main() {
    // Creating object on the managed heap
    // Note: 'make_tracked' returns a unique pointer
    sgcl::make_tracked<int>();
    
    // Using unique pointer with deterministic destruction
    sgcl::unique_ptr unique = sgcl::make_tracked<int>(42);
    auto also_unique = sgcl::make_tracked<int>(2);
    
    // Using standard shared pointer with deterministic destruction
    std::shared_ptr shared = sgcl::make_tracked<int>(13);
    
    // Using tracked pointer without deterministic destruction
    sgcl::tracked_ptr tracked = sgcl::make_tracked<int>(24);
    
    // Moving from unique pointer to the tracked pointer
    // Note: now destructor will be called in a GC thread
    tracked = std::move(unique);
    
    // Creating a pointer alias
    // Note: 'value' must be located in the managed memory
    struct Faz {
        int value;
    };
    sgcl::tracked_ptr faz = sgcl::make_tracked<Faz>(12);
    sgcl::tracked_ptr faz_value(&faz->value);
    std::cout << "Faz::value: " << *faz_value << std::endl;
    
    // Creating managed array
    sgcl::tracked_ptr arr = sgcl::make_tracked<int[]>(10);
    arr = sgcl::make_tracked<int[]>(10, 0);
    arr = sgcl::make_tracked<int[]>({ 7, 8, 9 });
    
    // Iterating over the array
    std::cout << "arr: ";
    for (auto v: arr) {
        std::cout << v << " ";
    }
    for (auto i = arr.rbegin(); i < arr.rend(); ++i) {
        *i = 12;
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
        Foo(int v) : Bar{v * v}, value{v} {}
        int value;
    };
    sgcl::tracked_ptr foo = sgcl::make_tracked<Foo[]>({1, 2, 3, 4, 5});
    std::cout << "foo: ";
    for (auto& f: foo) {
        std::cout << f.value << " ";
    }
    // Casting 'foo' to base class
    // Note: this is safe in the SGCL
    sgcl::tracked_ptr<Bar[]> bar = foo;
    std::cout << std::endl << "bar: ";
    for (auto& b: bar) {
        std::cout << b.value << " ";
    }
    std::cout << std::endl;
    
    // Casting array to object
    sgcl::tracked_ptr<Foo> first_foo = foo;
    
    // Casting object to array
    sgcl::tracked_ptr<int[]> single_value_array = sgcl::make_tracked<int>(12);
    
    // Using an atomic pointer
    sgcl::atomic<sgcl::tracked_ptr<int>> atomic = sgcl::make_tracked<int>(2);
    
    // Using an atomic reference
    sgcl::tracked_ptr<int> value;
    sgcl::atomic_ref atomic_ref(value);
    
    // Check pointer type
    sgcl::tracked_ptr<void> any = sgcl::make_tracked<char>();
    std::cout << "any " << (any.is<int>() ? "is" : "is not") <<  " int" << std::endl;
    std::cout << "any " << (any.type() == typeid(char) ? "is" : "is not") <<  " char" << std::endl;
    
    // Casting
    // Note: 'as' method only allows to cast to the lowest level class.
    auto c = any.as<char>();
    // or
    c = static_pointer_cast<char>(any);
    
    // Cloning
    auto message = sgcl::make_tracked<std::string>("message");
    auto clone = message.clone();
    std::cout << "clone: " << *clone << std::endl;
    
    // Metadata usage
    sgcl::set_metadata<int>(new std::string("int metadata"));
    sgcl::set_metadata<double>(new std::string("double metadata"));
    any = sgcl::make_tracked<int>();
    std::cout << *any.metadata<std::string>() << std::endl;
    any = sgcl::make_tracked<double>();
    std::cout << *any.metadata<std::string>() << std::endl;
}
```
## Methods useful for state analysis
```cpp
// Forcing garbage collection
sgcl::collector::force_collect();

// Forcing garbage collection and waiting for the cycle to complete
sgcl::collector::force_collect(true);

// Get number of live objects
// Note: A full GC cycle is performed before returning the data
auto live_object_count = sgcl::collector::get_live_object_count();
std::cout << "live object count: " << live_object_count << std::endl;

{
    // Get list of live objects
    // Note: A full GC cycle is performed before returning the data
    // Note: pause_guard and std::vector with raw pointers is returned
    //       The GC engine is paused until the pause guard is destroyed
    auto [pause_guard, live_objects] = sgcl::collector::get_live_objects();
    for (auto& v: live_objects) {
        std::cout << v << " ";
    }
    std::cout << std::endl;
} // The pause guard is destroyed at this point

// Terminate collector
// Note: This call is optional
sgcl::collector::terminate();
```
## Dependencies
This library is written in C++20 and a compliant compiler is necessary. 

No external library is necessary and there are no other requirements.
## Usage
This is a header only library. You can just copy the `sgcl` subfolder somewhere in your include path.
