# Sgcl::ConcurrentStack

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentQueue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class ConcurrentStack;
}
```

The same class in the `sgcl` interface: [concurrent_stack](../../concurrent/concurrent_stack.md).

`ConcurrentStack<T>` is a lock-free LIFO stack shared by any number of threads: the Treiber stack, a single atomic word for the head and a compare-exchange to push or pop, written the way it is written for a runtime with a collector. There is no ABA problem, no hazard pointer to publish, no epoch to enter and no reclamation scheme in the container, because a node is never reused while a thread holds it; the collector reclaims a node once nothing does ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). The interface is that of Java's `ConcurrentLinkedDeque` used at one end, with the names of a stack: `Push`, `Emplace`, `TryPop`, `Pop`, `IsEmpty`, `Count`, `Clear`. The element type is any movable `T`, a `Ptr` included.

## Rules

- The container is one word, the atomic head. The stack lives where a `Ptr` may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- Every operation is lock-free and may be called from any thread at any time; `Push` and `TryPop` are linearizable at their compare-exchange. `Pop` blocks while the stack is empty, on the head's wait; every `Push` notifies.
- A failed compare-exchange backs off before the retry, exponentially up to `config::BackoffMax` pause instructions (the backoff stack of Herlihy and Shavit): many threads at one word otherwise spend more on their retries than on their operations, and the backoff turns the storm into near-serial exchanges, sixteen threads at 23 ns per operation instead of 740 ([config](../../core/config.md#sgcl_backoff_max-backoffmax)). The cap bounds what one operation may wait under that contention.
- An element is moved out of its node by the thread that pops it, into the `Optional` returned, and destroyed in the node there and then, on the popping thread. The node is garbage from that moment. The move should not throw: an element whose move constructor throws is lost.
- `Count()` walks the nodes: linear, and a snapshot of no particular moment when other threads push or pop, as Java's `size` is. `IsEmpty()` is one load.
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); there is no `Peek()`: the top element is what `TryPop` returns.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::concurrent_stack<T>;
using SizeType = size_t;
```

### Constructors

```cpp
ConcurrentStack() noexcept;
ConcurrentStack(const ConcurrentStack&) = delete;
```

An empty stack: a null head, no allocation.

### Push, Emplace

```cpp
void Push(const T& value);
void Push(T&& value);
template<class... A> void Emplace(A&&... a);
```

Creates a node on the managed heap holding the element (constructed from `a...` in place for `Emplace`), links it above the current head and publishes it with a compare-exchange, retrying against concurrent pushes and pops; then notifies one thread waiting in `Pop`.

```cpp
struct Work { int id; void Run() {} };
ConcurrentStack<Ptr<Work>> tasks;
tasks.Push(Make<Work>(1));
tasks.Emplace(Make<Work>(2));            // the element built from its arguments
```

### TryPop, Pop

```cpp
Optional<T> TryPop();
T Pop();
```

`TryPop` takes the top element: the element, or `None` when the stack was empty at the moment of the load. `Pop` takes the top element, waiting while the stack is empty.

```cpp
struct Work { int id; void Run() {} };
ConcurrentStack<Ptr<Work>> tasks;
tasks.Push(Make<Work>(1));
tasks.Push(Make<Work>(2));
if (auto t = tasks.TryPop()) {
    (*t)->Run();
}
Ptr next = tasks.Pop();   // blocks until a push
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

`IsEmpty` is a load of the head; `Count` counts the nodes, in linear time.

### Clear

```cpp
void Clear() noexcept;
```

Takes the whole stack off the head with a compare-exchange and destroys every element it held, on the calling thread; the nodes are the collector's.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The stack inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// Producers push jobs, consumers pop them and block while there are
// none: the classic shape, with nothing in it about who frees a node
struct Job {
    int id;
};

int main() {
    ConcurrentStack<Ptr<Job>> jobs;   // on the stack
    Atomic done = 0;
    List<Thread> threads;
    for (int p : Range(4)) {
        threads.Emplace([&, p] {
            for (int i : Range(1000)) {
                jobs.Emplace(Make<Job>(p * 1000 + i));
            }
        });
        threads.Emplace([&] {
            for (int i : Range(1000)) {
                Ptr job = jobs.Pop();          // waits when the stack is empty
                done += job->id >= 0;
            }                                           // the job is garbage once nothing holds it
        });
    }
    for (auto& t : threads) {
        t.Join();
    }
    std::cout << done << " jobs, " << (jobs.IsEmpty() ? "stack empty" : "?") << "\n";
    return done == 4000 && jobs.IsEmpty() ? 0 : 1;
}
```

The output:

```
4000 jobs, stack empty
```

## See also

- [ConcurrentQueue](ConcurrentQueue.md) for the FIFO counterpart, [ConcurrentSortedDictionary](ConcurrentSortedDictionary.md) for the ordered dictionary
- [Atomic](Atomic.md), what the head is: the same structure written by hand is in its example
- [Stack](../Containers/Stack.md), the sequential adapter
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
