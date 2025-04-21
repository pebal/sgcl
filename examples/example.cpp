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
    for (auto i = arr.rbegin(); i != arr.rend(); ++i) {
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

    // Using a list and vector with cycle detection
    struct Node {
        int value;
        sgcl::list<sgcl::tracked_ptr<Node>> childs;
    };
    sgcl::vector<sgcl::tracked_ptr<Node>> nodes;
    sgcl::unordered_map<int, sgcl::tracked_ptr<Node>> nodes_map;

    // Metadata usage
    sgcl::set_metadata<int>(new std::string("int metadata"));
    sgcl::set_metadata<double>(new std::string("double metadata"));
    any = sgcl::make_tracked<int>();
    std::cout << *any.metadata<std::string>() << std::endl;
    any = sgcl::make_tracked<double>();
    std::cout << *any.metadata<std::string>() << std::endl;

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
}
