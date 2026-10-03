[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::spsc_queue\<T\>

```cpp
#include "sgcl/concurrent/spsc_queue.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T>
    class spsc_queue;
}
```

`sgcl::concurrent::spsc_queue<T>` is a bounded wait-free FIFO queue for exactly one producer thread and one
consumer thread: a ring of cells, a power of two of them, fixed at construction, with a sequence number per cell,
the way the [bounded_queue](../bounded_queue/README.md) numbers its cells, and without its compare-exchange, since the
producer alone moves the tail and the consumer alone the head. A head and a tail count up forever, a cell being
the position masked; a cell's sequence says whose the cell is: the position, once the cell is free for a push at
it, or the position with a published bit, once the element at it is in. A push looks at the sequence of the cell
at the tail, constructs the element there and publishes the cell; a pop looks at the sequence of the cell at the
head, moves the element out, destroys it, and frees the cell for the next lap.

Each side reads its own index only, which the other never touches: what the two sides share is the cell, its
sequence and its element on one line, so an element costs one line crossing between the cores and no more, and a
push and a pop between two threads cost what the pair costs on one. Lamport's ring, with each side caching the
other's index, shares the indices instead, and a consumer running right behind the producer reloads the
producer's tail at nearly every pop, two lines crossing per element and both taken back by the producer for its
next push: measured, that ring took several times this one's time per element with the consumer at the
producer's heels, and this one is no slower with room to run. The buffer is raw storage the queue constructs
elements in and destroys them in, at push and pop time, as the [vector](../../core/vector/README.md) does in its buffer:
the collector traces every cell (an unconstructed or destroyed element holds null pointers only) and never
destroys one, so an element holding a `tracked_ptr` keeps its object alive for exactly as long as it sits in the
ring.

The interface is that of the [bounded_queue](../bounded_queue/README.md): `try_push`, `try_emplace`, `try_pop`, a blocking
`push` and `pop`, `size`, `capacity`, `empty`, `full`. What differs from `std::queue` is what differs for the
bounded queue: a bound, no `front()`, a `pop` that returns the element. Neither Go's library nor Java's has a
single-producer queue. The element type is any movable `T`, a `tracked_ptr` included.

## Rules

- One producer and one consumer, and no more: the producer's functions (`try_push`, `try_emplace`, `push`) may be
  called by one thread at a time, and the consumer's (`try_pop`, `pop`) by one thread at a time; the same thread
  may be both. A second thread on either side is a data race on that side's index; several producers or
  consumers take a [bounded_queue](../bounded_queue/README.md). `size`, `empty`, `full` and `capacity` may be called from
  any thread.
- The container is three groups of words a cache line apart (`config::cache_line_size`): the buffer, the mask and
  the count of waiters, read by both sides and written by neither but a side about to wait; the head, the
  consumer's line; the tail, the producer's. The queue holds its buffer by a `tracked_ptr`, so it lives where one
  may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- The capacity is rounded up to a power of two, at least one; [capacity](capacity.md) is what the ring
  holds.
- `try_push`, `try_emplace` and `try_pop` are wait-free: a load of the side's own index, a load of the cell's
  sequence, a store, no compare-exchange. `push` waits while the queue is full, on the cell at the tail, which the
  pop that frees it notifies; `pop` waits while it is empty, on the cell at the head, which the push that publishes
  it notifies: a spin of a few microseconds first, then a wait in the kernel.
- The queue is FIFO.
- An element is constructed in its cell by the producer, moved out of the cell by the consumer, into the
  `optional` returned, and destroyed in the cell there and then, on the consumer's thread. The elements still in
  the ring when the queue dies are destroyed by its destructor, wherever it runs: on a stack, or in a sweep inside
  a managed object.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any object type that is move-constructible. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `size_type` | `size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](spsc_queue.md) | constructs an empty queue of a capacity |
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
| [try_push](try_push.md) | the producer's: appends an element, or returns `false` when the queue is full |
| [try_emplace](try_emplace.md) | the producer's: constructs an element in place at the end, or returns `false` when the queue is full |
| [push](push.md) | the producer's: appends an element, waiting for room |
| [try_pop](try_pop.md) | the consumer's: takes the first element, or nothing when the queue is empty |
| [pop](pop.md) | the consumer's: takes the first element, waiting for one |

## Complexity

- `try_push`, `try_emplace`, `try_pop`: constant, a bounded number of steps.
- `push`, `pop`: constant once there is room or an element; the wait is as long as the queue stays full or empty.
- `size`, `empty`, `full`, `capacity`: constant.

Through a ring of 1024 a push and a pop of an `int` cost 5.1 ns on one thread and 5.7 between a producer and a
consumer, against 21 to 27 for Lamport's ring and 30 for the bounded queue
([Benchmarks: The rings against the unbounded queue](../benchmarks.md#the-rings-against-the-unbounded-queue));
on the channel's harness, an object allocated per push:
[Benchmarks: The single-producer queue and the cache](../benchmarks.md#the-single-producer-queue-and-the-cache).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Sample {
    int seq;
    double value;
};

struct Pipeline {
    concurrent::spsc_queue<tracked_ptr<Sample>> samples{16};  // inside a managed object
};

int main() {
    tracked_ptr pipeline = make_tracked<Pipeline>();
    thread reader([&] {  // the one producer
        for (int i : range(10000)) {
            // waits while the ring is full
            pipeline->samples.push(make_tracked<Sample>(i, i * 0.5));
        }
    });
    double sum = 0;  // the one consumer: this thread
    int out_of_order = 0, last = -1;
    for (int i : range(10000)) {
        tracked_ptr s = pipeline->samples.pop();  // waits while the ring is empty
        out_of_order += s->seq != last + 1;
        last = s->seq;
        sum += s->value;
    }
    reader.join();
    println("sum {}, {} out of order", sum, out_of_order);
    println("{} left in a ring of {}", pipeline->samples.size(), pipeline->samples.capacity());
}
```

Output:

```text
sum 24997500, 0 out of order
0 left in a ring of 16
```

## See also

- [bounded_queue](../bounded_queue/README.md): the ring for any number of producers and consumers
- [queue](../queue/README.md): the unbounded queue
- [channel](../../async/channel/README.md): a ring with the synchronization of both ends and coroutines waiting on it
- [atomic](../../core/atomic.md): what the sequences, the head and the tail are
- [README: Lock-free containers](../README.md#lock-free-containers)
