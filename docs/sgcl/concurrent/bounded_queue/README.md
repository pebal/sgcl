[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::bounded_queue\<T\>

```cpp
#include "sgcl/concurrent/bounded_queue.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T>
    class bounded_queue;
}
```

`sgcl::concurrent::bounded_queue<T>` is a bounded lock-free FIFO queue shared by any number of producers and
consumers: the bounded MPMC queue of Dmitry Vyukov, the ring of Go's buffered `chan` and of Java's
`ArrayBlockingQueue` without the lock. The elements live in a ring of cells, a power of two of them, fixed at
construction; each cell carries a sequence number, and two counters, the enqueue and the dequeue position, count
up forever, a cell being the position masked. A cell whose sequence equals the enqueue position is free for the
producer that wins the position with a compare-exchange; the producer constructs the element in the cell and
publishes it by storing the sequence as the position plus one, which is what the consumer at that position looks
for; the consumer that wins the dequeue position moves the element out, destroys it, and stores the sequence as
the position plus the number of cells, the enqueue position of the next lap.

That is one compare-exchange per operation, on the word the producers share or on the one the consumers share,
and no allocation per element: the cells are one managed buffer, taken once. A cell reserved and not yet
published is nothing to a consumer, and a cell taken and not yet released nothing to a producer: the sequence
says so, and a thread that finds its position stale reloads it. The buffer is raw storage the queue constructs
elements in and destroys them in, at push and pop time, as the [vector](../../core/vector/README.md) does in its buffer:
the collector traces every cell (an unconstructed or destroyed element holds null pointers only) and never
destroys one, so an element holding a `tracked_ptr` keeps its object alive for exactly as long as it sits in the
ring.

What differs from `std::queue`: the queue is bounded, a push may find it full, and there is no `front()`: `pop`
returns the element, and `try_pop` is the pop that does not wait. What differs from Go's buffered channel: there
is no close and no `select` (the [channel](../../async/channel/README.md) of the async module, whose buffer is this ring,
has both), and
a full queue makes `try_push` return `false` where Java's `offer` does. The element type is any movable `T`, a
`tracked_ptr` included.

## Rules

- The container is three groups of words a cache line apart (`config::cache_line_size`): the buffer, the mask
  and the count of waiters, read by every thread and written by none but a thread about to wait; the enqueue
  position, the producers' word; the dequeue position, the consumers'. The queue holds its buffer by a
  `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object
  ([The rules](../../core/README.md#the-rules), 1).
- The capacity is rounded up to a power of two, at least two; [capacity](capacity.md) is what the
  ring holds.
- Every member function may be called from any thread at any time. `try_push`, `try_emplace`, `try_pop`, `size`,
  `empty` and `full` never wait; `try_push`, `try_emplace` and `try_pop` are lock-free and linearizable at their
  compare-exchange on a position. `push` waits while the queue is full and `pop` while it is empty, each on the
  sequence of the cell at its position, which the other side's store to that cell notifies: a spin of a few
  microseconds first, then a wait in the kernel.
- A lost compare-exchange on a position, or a position found stale, backs off before the retry, exponentially up
  to 1024 pause instructions.
- The queue is FIFO: every producer's elements come out in the order it pushed them, at every consumer.
- An element is constructed in its cell by the thread that pushes it, moved out of the cell by the thread that
  pops it, into the `optional` returned, and destroyed in the cell there and then, on the popping thread. The
  elements still in the ring when the queue dies are destroyed by its destructor, wherever it runs: on a stack,
  or in a sweep inside a managed object.
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
| [(constructor)](bounded_queue.md) | constructs an empty queue of a capacity |
| `(destructor)` | destroys the elements still in the ring; the buffer is left to the collector |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the queue holds an element |
| [full](full.md) | checks whether the queue holds as many elements as it has cells |
| [size](size.md) | the number of elements |
| [capacity](capacity.md) | the number of cells |

#### Modifiers

| Function | Description |
|---|---|
| [try_push](try_push.md) | appends an element, or returns `false` when the queue is full |
| [try_emplace](try_emplace.md) | constructs an element in place at the end, or returns `false` when the queue is full |
| [push](push.md) | appends an element, waiting for room |
| [try_pop](try_pop.md) | takes the first element, or nothing when the queue is empty |
| [pop](pop.md) | takes the first element, waiting for one |

## Complexity

- `try_push`, `try_emplace`, `try_pop`: constant, plus the retries of a lost compare-exchange and their backoff.
- `push`, `pop`: constant once there is room or an element; the wait is as long as the queue stays full or empty.
- `size`, `empty`, `full`, `capacity`: constant.

There is no allocation per element: through a ring of 1024 a push and a pop of an `int` cost 7.1 ns on one thread,
against 70 for the unbounded [queue](../queue/README.md)
([Benchmarks: The rings against the unbounded queue](../benchmarks.md#the-rings-against-the-unbounded-queue)).
Against Go's channel and Java's `ArrayBlockingQueue`:
[Benchmarks: The bounded queue, the priority queue, intern and the weak map](../benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map).

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
    concurrent::bounded_queue<tracked_ptr<Message>> inbox{64};  // inside a managed object
};

int main() {
    tracked_ptr stage = make_tracked<Stage>();
    atomic<int> received = 0, out_of_order = 0;
    vector<thread> threads;
    for (int p : range(4)) {
        threads.emplace_back([&, p] {
            for (int i : range(1000)) {
                stage->inbox.push(make_tracked<Message>(p, i));  // waits while the ring is full
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
    println("{} left in a ring of {}", stage->inbox.size(), stage->inbox.capacity());
}
```

Output:

```text
4000 messages, 0 out of order
0 left in a ring of 64
```

## See also

- [spsc_queue](../spsc_queue/README.md): the ring for one producer and one consumer, without a compare-exchange
- [queue](../queue/README.md): the unbounded queue; [stack](../stack/README.md): the LIFO counterpart
- [channel](../../async/channel/README.md): this ring with the synchronization of both ends and coroutines waiting on it
- [atomic](../../core/atomic.md): what the positions and the sequences are
- [README: Lock-free containers](../README.md#lock-free-containers)
