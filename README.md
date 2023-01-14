# SGCL
## About the SGCL
SGCL is a real-time garbage collector for C++. Provides fully tracked smart pointers, similar in use to shared_ptr.
## Features
- Thread safe
- Does not use reference counters
- Easy to use like shared_ptr
- Less memory overhead than shared_ptr
- Faster than shared_ptr in many scenarios
- Automatic roots registration
- Never stop the world
- Cyclic data structures friendly
- CoW friendly
- Executing destructors in a separate thread
- Atomic pointers always lock-free
- Only one header file

## Compiling
C++17 compiler required. Tested on Windows with VC++, Clang and MinGW compilers. MinGW is not recommended because it TLS emulation.
## Example
```
#include "sgcl.h"

using namespace sgcl;

// create object
auto value = make_tracked<int>(10);

// create array
auto array = make_tracked<int[]>(4);

// array iteration
for (auto& v: array) {
    v = 5;
}

// cast pointer
tracked_ptr<void> any = value;
value = static_pointer_cast<int>(any);
if (any.is<int>() || any.type() == typeid(int)) {
    value = any.as<int>();
}

// copy pointer
auto ref = value;

// clone object
auto value2 = value.clone();

// pointer in structure
struct node {
    float data;
    tracked_ptr<node> next;
};

// create node on tracked heap
auto tracked_node = make_tracked<node>();

// create node on standard heap
auto unique_node = std::make_unique<node>();
auto shared_node = std::make_shared<node>();

// create node on stack
node stack_node;

// create object based on raw pointer
tracked_ptr<node> tracked_node_ref(tracked_node.get());
// undefined behavior
// tracked_ptr<node> unique_node_ref(unique_node.get());
// tracked_ptr<node> shared_node_ref(shared_node.get());
// tracked_ptr<node> stack_node_ref(&stack_node);

// create alias
// !!! only if size of owner object is less or equal than MaxAliasingDataSize
tracked_ptr<float> alias(&tracked_node->data);

// atomic pointer
std::atomic<tracked_ptr<int>> atomic = value;

// metadata using
metadata<int>::set(3.14);
std::cout << any.metadata<double>() << std::endl;  // ok, any is int and metadata is double
std::cout << std::any_cast<double>(any.metadata()) << std::endl;  // also ok
```
## Not working
Currently cycles are not detected in the standard containers. All pointers in these containers are roots.
