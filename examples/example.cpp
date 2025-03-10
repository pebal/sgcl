#include "sgcl/sgcl.h"

#include <iostream>

using namespace sgcl;

// Tracked pointer in the managed object
struct Node {
    TrackedPtr<Node> next;
    int value = 5;
};

int main() {
    // Creating managed object
    // Note: 'make_tracked' returns a unique pointer
    make_tracked<int>();

    // Using unique pointer with deterministic destruction
    UniquePtr unique = make_tracked<int>(42);

    // Using shared pointer with deterministic destruction
    std::shared_ptr shared = make_tracked<int>(13);

    // Using stack pointer without deterministic destruction
    // Note: destructor will be called in a GC thread
    StackPtr stack = make_tracked<int>(24);
    Pointer<int, PointerPolicy::Stack> also_stack;

    // Invalid, tracked pointer cannot be on the stack
    // TrackedPtr<int> tracked;
    // Pointer<int, PointerPolicy::Tracked> also_tracked;

    // Reference to tracked pointer
    // Note: stack pointer can be cast to tracked pointer reference
    // Note: tracked pointer reference can be on the stack
    TrackedPtr<int>& tracked_ref = stack;
    Pointer<int, PointerPolicy::Tracked>& also_tracked_ref = stack;

    // Creating managed object included tracked pointer
    StackPtr node = make_tracked<Node>();

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
    StackPtr value(&node->value);

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
    StackPtr foo = make_tracked<Foo[]>(5);

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
    StackPtr min = fmin(value, stack);
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
    StackPtr c = any.as<char>();
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

    // Forcing garbage collection
    Collector::force_collect();

    // Forcing garbage collection and waiting for the cycle to complete
    Collector::force_collect(true);

    // Get number of living objects
    // Note: This is the number calculated in the last GC cycle
    auto last_living_objects_number = Collector::last_living_objects_number();
    std::cout << "last living objects number: " << last_living_objects_number << std::endl;

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
}
