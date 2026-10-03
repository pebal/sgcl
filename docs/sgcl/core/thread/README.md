[sgcl](../../README.md) › [core](../README.md)

# sgcl::thread

```cpp
#include "sgcl/core/thread.h"   // or "sgcl/core.h"

namespace sgcl {
    class thread;

    void swap(thread& a, thread& b) noexcept;

    namespace this_thread = std::this_thread;
}
```

`sgcl::thread` is `std::thread` with the closure kept where a `tracked_ptr` may live. `std::thread` copies the
callable and its arguments to the unmanaged heap, where a `tracked_ptr` among them is seen by no one
([The rules](../README.md#the-rules), 1), so a program had to capture by reference, to a frame that outlives the
thread, or hand the object over through a [root_ptr](../root_ptr/README.md). Here the callable and the arguments go into a
managed node of their own, as a [function](../function/README.md) keeps its closure, and what travels through
`std::thread`'s state is a `root_ptr` to that node, which lives anywhere: the node is a root from the constructor
to the end of the function, with no window in which the new thread has not taken it yet, and the tracked pointers
inside are traced through the node's pointer map. The new thread calls the closure in the node and destroys it
the moment the call returns, as a `function` drops its closure; the node goes to the collector, the cell with
`std::thread`'s state.

The interface is the standard's: `join`, `detach`, `joinable`, `get_id`, `native_handle`, `swap`,
`hardware_concurrency`, the destructor that ends the program on a thread still joinable, the exception a function
lets out ending the program. What it costs beside `std::thread`: one managed node and one root cell per thread
started. `sgcl::this_thread` is `std::this_thread` (`yield`, `sleep_for`, `sleep_until`, `get_id`), declared in
`sgcl/core/aliases.h`.

`std::async` and `std::packaged_task` copy the closure to the heap as `std::thread` does and have no counterpart
here: a blocking call goes through [spawn_blocking](../../async/spawn_blocking.md), work with a result through a
[task](../../async/task/README.md).

## Rules

- A thread is a thread to the collector: its stack is scanned, the tracked pointers on it are roots, from the
  first line of the function it runs ([The rules](../README.md#the-rules), 1).
- The callable and the arguments are copied (decayed) into a managed node, so a `tracked_ptr` is captured by value
  and passed as an argument as it would be to a `function`; the callable is called once, with the arguments as
  rvalues, as `std::thread` calls it.
- The closure is destroyed on the new thread when the function returns, before `join` returns; what it held by
  value is released then and collected later.
- The threads of a program, kept until joined, live in a container that holds them where it may:
  `sgcl::vector<sgcl::thread>` on a stack ([vector](../vector/README.md)). A `thread` holds no tracked pointer itself, so it
  may live anywhere `std::thread` may.
- A thread still joinable when it is destroyed or assigned to ends the program with `std::terminate`, as a
  `std::thread` does; so does an exception the function lets out.

## Member types

| Type | Definition |
|---|---|
| `id` | `std::thread::id` |
| `native_handle_type` | `std::thread::native_handle_type` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](thread.md) | constructs a thread object, starting a thread when given a function |
| `(destructor)` | destroys the thread object; ends the program when the thread is still joinable |
| [operator=](operator_assign.md) | moves a thread object |

#### Observers

| Function | Description |
|---|---|
| [joinable](joinable.md) | checks whether the object stands for a thread that has not been joined or detached |
| [get_id](get_id.md) | the id of the thread |
| [native_handle](native_handle.md) | the handle of the thread on the platform |
| [hardware_concurrency](hardware_concurrency.md) | the number of threads the hardware runs at once (static) |

#### Operations

| Function | Description |
|---|---|
| [join](join.md) | waits for the thread to finish |
| [detach](detach.md) | lets the thread run on independently of the object |
| [swap](swap.md) | swaps two thread objects |

## Non-member functions

| Function | Description |
|---|---|
| [swap](swap.md) | swaps two thread objects |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

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
        // by value: the pointer goes with the closure, into a managed node
        workers.emplace_back([=] {
            for (int i : range(1000)) {
                sum->total += t * 1000 + i;
            }
        });
    }
    for (auto& w : workers) {
        w.join();
    }
    println("{}", sum->total.load());
}
```

Output:

```text
7998000
```

## See also

- [function](../function/README.md): the same closure in a managed node, called many times
- [root_ptr](../root_ptr/README.md): the root that carries the node through `std::thread`
- [atomic](../atomic.md): a `tracked_ptr` shared between threads
- [vector](../vector/README.md): where the threads are kept
- [spawn_blocking](../../async/spawn_blocking.md), [task](../../async/task/README.md): a blocking call, work with a result
- [README: The rules](../README.md#the-rules)
