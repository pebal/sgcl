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

tracked_ptr<int> i = make_tracked<int>(10);
tracked_ptr<void> v = i;
tracked_ptr<int> i2 = static_pointer_cast<int>(v);
tracked_ptr<int> i3 = i2.clone();

auto arr = make_tracked<char[]>(6);
arr[3] = 's';

struct foo {
        auto ptr() {
                return tracked_ptr<foo>(this);
        }
};
auto f = make_tracked<foo>();
auto f2 = f->ptr();

metadata<foo>::user_data = new int(3);
tracked_ptr<void> b = f2;
std::cout << b.metadata().type_info.name() << std::endl;
std::cout << *(int*)(b.metadata().user_data) << std::endl;

std::atomic<tracked_ptr<int>> a;
a.store(i, std::memory_order_relaxed);
a.compare_exchange_strong(i, i3);
```
## Not working
Currently cycles are not detected in the standard containers. All pointers in the containers are roots.
