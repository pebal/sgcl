[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::queue\<T\>

```cpp
#include "sgcl/concurrent/queue.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T>
    class queue;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::queue<T>` is an unbounded lock-free FIFO queue shared by any number of producers and
consumers: the Michael–Scott queue in the form Java's `ConcurrentLinkedQueue` gives it, written as it is written
for a runtime with a collector. The nodes form a singly linked list; the head addresses a node at or before the
first element and the tail a node at or before the last one, both lagging on purpose. A push links the new node
after the last one with a compare-exchange on that node's link and swings the tail only when it found the tail a
node or more behind; a pop walks from the head to the first element not yet taken, claims it with a
compare-exchange on its node's flag, and swings the head only when the element was a node or more past it. That
halves the exchanges on the two words every thread contends for (Java's "hop two nodes at a time"), and a thread
that finds a word behind walks the links to where it should be, so no thread ever waits for another.

A node the head has passed is linked to itself: the sign, for a walk, that it left the list, and the reason an
old head a thread still holds retains nothing behind it. There is no ABA, no counted pointer, no hazard pointer
in the algorithm and no free list: a node is never reused while a thread holds it, and a node nobody holds is
reclaimed by the collector ([README: Lock-free containers](../README.md#lock-free-containers)).

What differs from `std::queue`: the interface has its names (`push`, `emplace`, `empty`, `size`), but there is no
`front()` and `pop` returns the element, because between a look at the front and its removal another thread may
take it; `try_pop` is the pop that does not wait. What differs from Java's `ConcurrentLinkedQueue`: `pop` waits on
an empty queue instead of returning nothing, and the element is moved out, not shared. The element type is any
movable `T`, a `tracked_ptr` included.

## Rules

- The container is two atomic words, the head and the tail, kept a cache line apart (`config::cache_line_size`) so that
  the consumers' line and the producers' line do not bounce for each other's traffic.
- Every member function may be called from any thread at any time. `push`, `emplace`, `push_range`, `try_pop`,
  `empty`, `size` and `clear` are lock-free; `push` and `try_pop` are linearizable at their compare-exchange on a
  link and on a node's flag. `pop` waits while the queue is empty.
- The queue is FIFO: every producer's elements come out in the order it pushed them, at every consumer.
- An element is moved out of its node by the thread that pops it, into the `optional` returned, and destroyed in
  the node there and then: what `std::queue::pop` does, on the popping thread. The elements still in the queue
  when it dies are destroyed with their nodes, by the collector.
- The nodes between the head and the first element, taken ones the head has not passed yet, are the queue's
  while it lives: at most a couple, as the head is swung every second node.
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
| [(constructor)](queue.md) | constructs an empty queue |
| `(destructor)` | leaves the nodes, and the elements still in them, to the collector |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the queue holds an element |
| [size](size.md) | counts the elements |

#### Modifiers

| Function | Description |
|---|---|
| [push](push.md) | appends an element |
| [emplace](emplace.md) | constructs an element in place at the end |
| [push_range](push_range.md) | appends the elements of a range together, with one exchange |
| [try_pop](try_pop.md) | takes the first element, or nothing when the queue is empty |
| [pop](pop.md) | takes the first element, waiting for one |
| [clear](clear.md) | pops every element there is |

## Complexity

- `push`, `emplace`, `try_pop`: constant, plus the walk over the nodes other threads linked or took since the
  tail or the head was last swung, a node or two.
- `push_range`: linear in the number of elements, with one compare-exchange on the list.
- `empty`: constant. `size`: linear in the number of elements.

A node is allocated per element, and the unbounded queue is ten times the time of a ring per element
([Benchmarks: The rings against the unbounded queue](../benchmarks.md#the-rings-against-the-unbounded-queue));
[bounded_queue](../bounded_queue/README.md) is the queue for a stream with a bound.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Message {
    int producer, seq;
};

struct Stage {
    concurrent::queue<tracked_ptr<Message>> inbox;  // inside a managed object
};

int main() {
    tracked_ptr stage = make_tracked<Stage>();
    atomic<int> received = 0, out_of_order = 0;
    vector<thread> threads;
    for (int p : range(4)) {
        threads.emplace_back([&, p] {
            for (int i : range(1000)) {
                stage->inbox.emplace(make_tracked<Message>(p, i));
            }
        });
        threads.emplace_back([&] {
            int last[4] = {-1, -1, -1, -1};
            for (int i : range(1000)) {
                tracked_ptr m = stage->inbox.pop();  // FIFO per producer, at every consumer
                out_of_order += m->seq <= last[m->producer];
                last[m->producer] = m->seq;
                ++received;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    println("{} messages, {} out of order", received.load(), out_of_order.load());
}
```

Output:

```text
4000 messages, 0 out of order
```

## See also

- [stack](../stack/README.md): the LIFO counterpart
- [bounded_queue](../bounded_queue/README.md), [spsc_queue](../spsc_queue/README.md): the rings, no allocation per element
- [channel](../../async/channel/README.md): a queue with the synchronization of both ends, whose lists of waiters are
  these queues
- [queue](../../core/queue/README.md): the sequential adapter
- [atomic](../../core/atomic.md): what the head, the tail and the links are
- [README: Lock-free containers](../README.md#lock-free-containers)
