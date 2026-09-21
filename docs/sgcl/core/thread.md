# sgcl::thread

```cpp
#include "sgcl/core/thread.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    class thread;
    namespace this_thread = std::this_thread;   // yield, sleep_for, sleep_until, get_id (aliases.h)
}
```

`sgcl::thread` is `std::thread` with the closure kept where a `tracked_ptr` may live. `std::thread` copies the callable and its arguments to the unmanaged heap, where a `tracked_ptr` among them is seen by no one ([The rules](README.md#the-rules), 1), so a program had to capture by reference, to a frame that outlives the thread, or hand the object over through a [`root_ptr`](root_ptr.md). Here the callable and the arguments go into a managed node of their own, as a [`function`](function.md) keeps its closure, and what travels through `std::thread`'s state is a `root_ptr` to that node, which lives anywhere: the node is a root from the constructor to the end of the function, with no window in which the new thread has not taken it yet, and the tracked pointers inside are traced through the node's pointer map. The new thread calls the closure in the node and destroys it the moment the call returns, as a `function` drops its closure; the node goes to the collector, the cell with `std::thread`'s state. The interface is the standard's: `join`, `detach`, `joinable`, `get_id`, `native_handle`, `swap`, `hardware_concurrency`, the destructor that ends the program on a thread still joinable, the exception a function lets out ending the program. What it costs beside `std::thread`: one managed node and one root cell per thread started. `sgcl::this_thread` is `std::this_thread`. `std::async` and `std::packaged_task` copy the closure to the heap as `std::thread` does and have no counterpart here: a blocking call goes through [`spawn_blocking`](../async/blocking.md), work with a result through a [`task`](../async/coroutine.md).

## Rules

- A thread is a thread to the collector: its stack is scanned, the tracked pointers on it are roots, from the first line of the function it runs ([The rules](README.md#the-rules), 1).
- The callable and the arguments are copied (decayed) into a managed node, so a `tracked_ptr` is captured by value and passed as an argument as it would be to a `function`; the callable is called once, with the arguments as rvalues, as `std::thread` calls it.
- The closure is destroyed on the new thread when the function returns, before `join` returns; what it held by value is released then and collected later.
- The threads of a program, kept until joined, live in a container that holds them where it may: `sgcl::vector<sgcl::thread>` on a stack ([vector](../containers/vector.md)). A `thread` holds no tracked pointer itself, so it may live anywhere `std::thread` may.

## Members

```cpp
using id = std::thread::id;
using native_handle_type = std::thread::native_handle_type;

thread() noexcept;                                              // no thread
template<class F, class... Args>
explicit thread(F&& f, Args&&... args);                         // starts f(args...): the callable and the arguments copied into a managed node
thread(thread&&) noexcept;                                      // not copyable
thread& operator=(thread&&) noexcept;                           // ends the program when this one is joinable
~thread();                                                      // ends the program when joinable

bool joinable() const noexcept;
void join();
void detach();
id get_id() const noexcept;
native_handle_type native_handle();
void swap(thread&) noexcept;
static unsigned hardware_concurrency() noexcept;
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

struct Sum {
    atomic<int> total = {0};
};

// Four threads add to a shared object; each closure holds the object by
// value, so main's frame owes it nothing and the threads may outlive it.
int main() {
    tracked_ptr sum = make_tracked<Sum>();
    vector<thread> workers;
    for (int t : range(4)) {
        workers.emplace_back([=] {                     // by value: the pointer goes with the closure, into a managed node
            for (int i : range(1000)) {
                sum->total += t * 1000 + i;
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    std::cout << sum->total.load() << "\n";            // 7998000
    return sum->total.load() == 7998000 ? 0 : 1;
}
```

The output:

```
7998000
```

## See also

- [function](function.md): the same closure in a managed node, called many times; [root_ptr](root_ptr.md): the root that carries the node through `std::thread`; [atomic](../concurrent/atomic.md): a `tracked_ptr` shared between threads; [vector](../containers/vector.md): where the threads are kept
- [The rules](README.md#the-rules)
