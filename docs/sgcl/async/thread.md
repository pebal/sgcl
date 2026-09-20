# sgcl::thread

```cpp
#include "sgcl/core/aliases.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    using std::thread;
    namespace this_thread = std::this_thread;   // yield, sleep_for, sleep_until, get_id
}
```

`sgcl::thread` is `std::thread` under the library's name, and `sgcl::this_thread` is `std::this_thread` (`yield`, `sleep_for`, `sleep_until`, `get_id`), so that a program written against the library names one namespace for what it starts and what it shares: the containers, the atomics and the threads that use them. Nothing is added: the constructors, `join`, `detach`, `joinable`, `get_id`, `hardware_concurrency`, `swap` and the destructor (a thread still joinable when destroyed ends the program) are the standard's.

## Rules

- A thread is a thread to the collector: its stack is scanned, the tracked pointers on it are roots, from the first line of the function it runs ([README: The rules](../core/README.md#the-rules), 1).
- The closure of the function a thread runs is copied to unmanaged memory, so a `tracked_ptr` is not captured by value: it is captured by reference, to a frame that outlives the thread (a `main` that joins it), or handed over through a [`root_ptr`](../core/root_ptr.md), which lives anywhere.
- The threads of a program, kept until joined, live in a container that holds them where it may: `sgcl::vector<sgcl::thread>` on a stack ([vector](../containers/vector.md)).

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// Four threads count the elements of a shared set; the set stays in
// main's frame, referenced from each closure, and is joined before it
// is gone.
int main() {
    sgcl::concurrent_set<int> seen;
    sgcl::vector<sgcl::thread> workers;
    for (int t : sgcl::range(4)) {
        workers.emplace_back([&seen, t] {              // by reference: main's frame outlives the threads it joins
            for (int i : sgcl::range(1000)) {
                seen.insert(t * 1000 + i);
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    std::cout << seen.size() << "\n";                  // 4000
    return seen.size() == 4000 ? 0 : 1;
}
```

The output:

```
4000
```

## See also

- [root_ptr](../core/root_ptr.md): a pointer handed to a thread that outlives the frame; [atomic](../concurrent/atomic.md): a `tracked_ptr` shared between threads; [vector](../containers/vector.md): where the threads are kept
- [README: The rules](../core/README.md#the-rules)
