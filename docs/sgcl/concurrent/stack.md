[sgcl](../README.md) › [concurrent](README.md)

# sgcl::concurrent::stack\<T\>

```cpp
#include "sgcl/concurrent/stack.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T>
    class stack;
}
```

`sgcl::concurrent::stack<T>` is a lock-free LIFO stack shared by any number of threads: the Treiber stack, one
atomic word for the head and a compare-exchange to push or pop, written as it is written for a runtime with a
collector. A push makes a node holding the element, links it above the head it loaded and publishes it with a
compare-exchange on the head; a pop loads the head and swings it to the node below with a compare-exchange. A
thread that loses the exchange backs off before the retry, exponentially, as the stack of Herlihy and Shavit
does: on one word every thread contends for, the retries of many threads otherwise cost more than the operations.

There is no ABA problem, no hazard pointer to publish, no epoch to enter and no reclamation scheme in the
container, because a node is never reused while a thread holds it: a popped node is garbage the collector
reclaims once nothing holds it ([README: Lock-free containers](README.md#lock-free-containers)).

What differs from `std::stack`: the interface has its names (`push`, `emplace`, `empty`, `size`), but there is no
`top()` and `pop` returns the element, because between a look at the top and its removal another thread may take
it; `try_pop` is the pop that does not wait. The interface is that of Java's `ConcurrentLinkedDeque` used at one
end, except that `pop` waits on an empty stack instead of returning nothing, and the element is moved out, not
shared. Go's library has no stack. The element type is any movable `T`, a `tracked_ptr` included.

## Rules

- The container is the atomic head and a count of the threads waiting in `pop`. The stack lives where a
  `tracked_ptr` may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- Every member function may be called from any thread at any time. `push`, `emplace`, `try_pop`, `empty`, `size`
  and `clear` are lock-free; `push` and `try_pop` are linearizable at their compare-exchange on the head. `pop`
  waits while the stack is empty, on the head's `wait`; a push notifies only when a thread waits.
- A failed compare-exchange of `push`, `emplace` or `try_pop` on the head backs off before the retry,
  exponentially up to `config::backoff_max` pause instructions ([config](../core/config.md)): the backoff turns
  the storm of lost exchanges into near-serial ones, and the cap bounds what one operation may wait under that
  contention.
- An element is moved out of its node by the thread that pops it, into the `optional` returned, and destroyed in
  the node there and then: what `std::stack::pop` does, on the popping thread. The node is garbage from that
  moment. The elements still on the stack when it dies are destroyed with their nodes, by the collector.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any object type that is move-constructible. Its move constructor should not throw: an element whose move throws on the way out is lost. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](stack/stack.md) | constructs an empty stack |
| `(destructor)` | leaves the nodes, and the elements still in them, to the collector |

#### Capacity

| Function | Description |
|---|---|
| [empty](stack/empty.md) | checks whether the stack holds an element |
| [size](stack/size.md) | counts the elements |

#### Modifiers

| Function | Description |
|---|---|
| [push](stack/push.md) | puts an element on the top |
| [emplace](stack/emplace.md) | constructs an element in place on the top |
| [try_pop](stack/try_pop.md) | takes the top element, or nothing when the stack is empty |
| [pop](stack/pop.md) | takes the top element, waiting for one |
| [clear](stack/clear.md) | takes every element off at once |

## Complexity

- `push`, `emplace`, `try_pop`: constant, plus the retries of a lost compare-exchange and their backoff.
- `empty`: constant, one load. `size`: linear in the number of elements. `clear`: linear in the number of
  elements, with one compare-exchange on the head.

A node is allocated per element. With the backoff the stack costs 9.9 ns per operation on one thread and 10.8 at
sixteen threads; without it, sixteen threads at one word spent 607
([Benchmarks: Lock-free stack](benchmarks.md#lock-free-stack)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Job {
    int id;
};

int main() {
    concurrent::stack<tracked_ptr<Job>> jobs;  // on the stack of this thread
    atomic<int> done = 0;
    vector<thread> threads;
    for (int p : range(4)) {
        threads.emplace_back([&, p] {
            for (int i : range(1000)) {
                jobs.emplace(make_tracked<Job>(p * 1000 + i));
            }
        });
        threads.emplace_back([&] {
            for (int i : range(1000)) {
                tracked_ptr job = jobs.pop();  // waits when the stack is empty
                done += job->id >= 0;
            }  // a job is garbage once nothing holds it
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{} jobs, empty: {}", done.load(), jobs.empty());
}
```

Output:

```text
4000 jobs, empty: true
```

## See also

- [queue](queue.md): the FIFO counterpart
- [stack](../core/stack.md): the sequential adapter
- [atomic](../core/atomic.md): what the head is
- [benchmarks/concurrent/lockfree_stack.cpp](../../../benchmarks/concurrent/lockfree_stack.cpp): the same
  structure written by hand
- [README: Lock-free containers](README.md#lock-free-containers)
