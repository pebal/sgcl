# sgcl::concurrent::stack

```cpp
#include "sgcl/concurrent/stack.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class stack;
}
```

`sgcl::concurrent::stack<T>` is a lock-free LIFO stack shared by any number of threads: the Treiber stack, a single atomic word for the head and a compare-exchange to push or pop, written the way it is written for a runtime with a collector. There is no ABA problem, no hazard pointer to publish, no epoch to enter and no reclamation scheme in the container, because a node is never reused while a thread holds it; the collector reclaims a node once nothing does ([README: Lock-free containers](README.md#lock-free-containers)). The interface is that of Java's `ConcurrentLinkedDeque` used at one end, with the names of `std::stack`: `push`, `emplace`, `try_pop`, `pop`, `empty`, `size`, `clear`. The element type is any movable `T`, a `tracked_ptr` included.

## Rules

- The container is one word, the atomic head. The stack lives where a `tracked_ptr` may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- Every operation is lock-free and may be called from any thread at any time; `push` and `try_pop` are linearizable at their compare-exchange. `pop` blocks while the stack is empty, on the head's `wait`; every `push` notifies.
- A failed compare-exchange backs off before the retry, exponentially up to `config::backoff_max` pause instructions (the backoff stack of Herlihy and Shavit): many threads at one word otherwise spend more on their retries than on their operations, and the backoff turns the storm into near-serial exchanges, sixteen threads at 23 ns per operation instead of 740 ([config](../core/config.md#sgcl_backoff_max-backoffmax)). The cap bounds what one operation may wait under that contention.
- An element is moved out of its node by the thread that pops it, into the `optional` returned, and destroyed in the node there and then: what `std::stack::pop` does, on the popping thread. The node is garbage from that moment. The move should not throw: an element whose move constructor throws is lost.
- `size()` walks the nodes: linear, and a snapshot of no particular moment when other threads push or pop, as Java's `size` is. `empty()` is one load.
- A `tracked_ptr` may not address an element ([The rules](../core/README.md#the-rules), 4); there is no `top()`: the top element is what `try_pop` returns.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using value_type = T;
using size_type = size_t;
```

### Constructors

```cpp
stack() noexcept;
stack(const concurrent::stack&) = delete;
```

An empty stack: a null head, no allocation.

### push, emplace

```cpp
void push(const T& value);
void push(T&& value);
template<class... A> void emplace(A&&... a);
```

Creates a node on the managed heap holding the element (constructed from `a...` in place for `emplace`), links it above the current head and publishes it with a compare-exchange, retrying against concurrent pushes and pops; then notifies one thread waiting in `pop`.

```cpp
concurrent::stack<tracked_ptr<Work>> tasks;   // a global: 
tasks.push(make_tracked<Work>(1));
tasks.emplace(make_tracked<Work>(2));            // the element built from its arguments
```

### try_pop, pop

```cpp
optional<T> try_pop();   // optional, the alias of std::optional (sgcl/core/aliases.h)
T pop();
```

`try_pop` takes the top element: the element, or nothing when the stack was empty at the moment of the load. `pop` takes the top element, waiting while the stack is empty.

```cpp
if (auto t = tasks.try_pop()) {
    (*t)->run().wait();
}
tracked_ptr next = tasks.pop();   // blocks until a push
```

### empty, size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
```

`empty` is a load of the head; `size` counts the nodes, in linear time.

### clear

```cpp
void clear() noexcept;
```

Takes the whole stack off the head with a compare-exchange and destroys every element it held, on the calling thread; the nodes are the collector's.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// Producers push jobs, consumers pop them and block while there are
// none: the classic shape, with nothing in it about who frees a node
struct Job {
    int id;
};

int main() {
    concurrent::stack<tracked_ptr<Job>> jobs;   // on the stack: 
    atomic done = 0;
    vector<thread> threads;
    for (int p : range(4)) {
        threads.emplace_back([&, p] {
            for (int i : range(1000)) {
                jobs.emplace(make_tracked<Job>(p * 1000 + i));
            }
        });
        threads.emplace_back([&] {
            for (int i : range(1000)) {
                tracked_ptr job = jobs.pop();     // waits when the stack is empty
                done += job->id >= 0;
            }                                           // the job is garbage once nothing holds it
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << done << " jobs, " << (jobs.empty() ? "stack empty" : "?") << "\n";
    return done == 4000 && jobs.empty() ? 0 : 1;
}
```

The output:

```
4000 jobs, stack empty
```

## See also

- [concurrent::queue](queue.md) for the FIFO counterpart, [concurrent::sorted_map](sorted_map.md) for the sorted map
- [atomic](../core/atomic.md), what the head is; [examples/lock_free_stack.cpp](../../../examples/lock_free_stack.cpp), the same structure written by hand
- [stack](../core/stack.md), the sequential adapter
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
