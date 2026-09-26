# sgcl::concurrent::spsc_queue

```cpp
#include "sgcl/concurrent/spsc_queue.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class spsc_queue;
}
```

`sgcl::concurrent::spsc_queue<T>` is a bounded wait-free FIFO queue for exactly one producer thread and one consumer thread: a ring of cells, a power of two of them, fixed at construction, with a sequence number per cell, the way the [concurrent::bounded_queue](bounded_queue.md) numbers its cells, and without its compare-exchange, since the producer alone moves the tail and the consumer alone the head. A head and a tail count up forever, a cell being the position masked; a cell's sequence says whose the cell is: the position, once the cell is free for a push at it, or the position with a published bit, once the element at it is in. A push looks at the sequence of the cell at the tail, constructs the element there and publishes the cell; a pop looks at the sequence of the cell at the head, moves the element out, destroys it, and frees the cell for the next lap. Each side reads its own index only, which the other never touches: what the two sides share is the cell, its sequence and its element on one line, so an element costs one line crossing between the cores and no more, and a push and a pop between two threads cost what the pair costs on one. Lamport's ring, with each side caching the other's index, shares the indices instead, and a consumer running right behind the producer reloads the producer's tail at nearly every pop, two lines crossing per element and both taken back by the producer for its next push: measured, that ring took twice this one's time per element with the consumer at the producer's heels, and no less with room to run (the table at the end). The buffer is raw storage the queue constructs elements in and destroys them in, at push and pop time, as the [vector](../core/vector.md) does in its buffer: the collector traces every cell (an unconstructed or destroyed element holds null pointers only) and never destroys one, so an element holding a `tracked_ptr` keeps its object alive for exactly as long as it sits in the ring ([README: Lock-free containers](README.md#lock-free-containers)). The interface is that of the [concurrent::bounded_queue](bounded_queue.md): `try_push`, `try_emplace`, `try_pop`, a blocking `push` and `pop`, `size`, `capacity`, `empty`, `full`. The element type is any movable `T`, a `tracked_ptr` included.

## Rules

- One producer and one consumer, and no more: the producer's side of the queue may be used by one thread at a time and the consumer's side by one thread at a time (the same thread may be both). A second thread on either side is a data race on that side's index; several producers or consumers take a [concurrent::bounded_queue](bounded_queue.md).
- The container is three cache lines (`config::cache_line_size`): the buffer, the mask and the count of waiters, read by both sides and written by neither but a side about to wait; the head, the consumer's line; the tail, the producer's. The queue holds its buffer by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- The capacity is rounded up to a power of two, at least one; `capacity()` is what the ring holds.
- `try_push`, `try_emplace` and `try_pop` are wait-free: a load, a store, no compare-exchange. The queue is FIFO. `push` waits while the queue is full, on the cell at the tail, which the pop that frees it notifies; `pop` waits while it is empty, on the cell at the head, which the push that publishes it notifies: a spin of a few microseconds first (a side that has just caught up with the other is nanoseconds from its next element), then a wait in the kernel.
- An element is constructed in its cell by the producer and moved out of the cell by the consumer, into the `optional` returned, and destroyed in the cell there and then, on the consumer's thread. A constructor that throws leaves the queue as it was: the tail moves only once the element is there. An element whose move constructor throws on the way out stays in its cell, the head unmoved, the exception the consumer's.
- `size()` is the tail less the head, two loads: exact on the producer's thread and the consumer's, a snapshot of some moment on any other. `empty()` and `full()` are `size()` against zero and the capacity.
- A `tracked_ptr` may not address an element ([The rules](../core/README.md#the-rules), 4); there is no `front()`: the first element is what `try_pop` returns.
- The elements still in the ring are destroyed with the queue, wherever it dies: on a stack, or in a sweep inside a managed object.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using value_type = T;
using size_type = size_t;
```

### Constructors

```cpp
explicit spsc_queue(size_type capacity);
spsc_queue(const spsc_queue&) = delete;
```

A queue of `capacity` rounded up to a power of two, at least one: one managed buffer of that many cells.

```cpp
concurrent::spsc_queue<tracked_ptr<Sample>> samples(1000);   // a ring of 1024
```

### try_push, try_emplace

```cpp
bool try_push(const T& value);
bool try_push(T&& value);
template<class... A> bool try_emplace(A&&... a);
```

The producer's: constructs the element in the cell at the tail (from `a...` in place for `try_emplace`) and moves the tail past it; `false`, and nothing constructed, when the ring is full: the cell at the tail still holds the element of the lap before, which its sequence says, so the producer reads no word of the consumer's.

```cpp
if (!samples.try_push(make_tracked<Sample>(1))) {   // full: the producer decides
    dropped++;
}
samples.try_emplace(make_tracked<Sample>(2));
```

### push

```cpp
void push(const T& value);
void push(T&& value);
```

The producer's: the element appended, waiting for room while the ring is full, on the cell at the tail, which the pop that frees it notifies.

```cpp
samples.push(make_tracked<Sample>(3));   // blocks until a pop makes room
```

### try_pop, pop

```cpp
optional<T> try_pop();   // optional, the alias of std::optional (sgcl/core/aliases.h)
T pop();
```

The consumer's: `try_pop` moves the element out of the cell at the head, destroys it in the cell and moves the head past it; nothing when the ring is empty: the cell at the head not published, which its sequence says, so the consumer reads no word of the producer's. `pop` takes the first element, waiting while the ring is empty, on the cell at the head, which the push that publishes it notifies.

```cpp
while (auto s = samples.try_pop()) {
    process(**s);
}
tracked_ptr next = samples.pop();   // blocks until a push
```

### size, capacity, empty, full

```cpp
size_type size() const noexcept;
size_type capacity() const noexcept;
bool empty() const noexcept;
bool full() const noexcept;
```

`size` is the tail less the head, at most the capacity; `capacity` the number of cells; `empty` and `full` are `size()` against zero and the capacity.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// A reader thread hands samples to a processing thread through a ring
// of 16: the reader waits when the ring is full, the processor when it
// is empty; the queue is a member of a managed object, the ring is
// allocated once and the samples are managed objects
struct Sample {
    int seq;
    double value;
};

struct Pipeline {
    concurrent::spsc_queue<tracked_ptr<Sample>> samples{16};   // inside a managed object: 
};

int main() {
    tracked_ptr pipeline = make_tracked<Pipeline>();
    thread reader([&] {                                   // the one producer
        for (int i : range(10000)) {
            pipeline->samples.push(make_tracked<Sample>(i, i * 0.5));   // waits while the ring is full
        }
    });
    double sum = 0;                                             // the one consumer: this thread
    int out_of_order = 0, last = -1;
    for (int i : range(10000)) {
        tracked_ptr s = pipeline->samples.pop();          // waits while the ring is empty
        out_of_order += s->seq != last + 1;
        last = s->seq;
        sum += s->value;
    }
    reader.join();
    std::cout << "sum " << sum << ", " << out_of_order << " out of order, "
              << pipeline->samples.size() << " left in a ring of " << pipeline->samples.capacity() << "\n";
    return sum == 24997500.0 && out_of_order == 0 ? 0 : 1;
}
```

The output:

```
sum 2.49975e+07, 0 out of order, 0 left in a ring of 16
```

## Measured

Nanoseconds per push and pop of an `int` through a ring of 1024, on the machine of [the benchmarks](benchmarks.md), `-O2`, 20 M elements: one thread pushing and popping in turn, and a producer and a consumer with the blocking `push` and `pop`, against the [concurrent::bounded_queue](bounded_queue.md) and the unbounded [concurrent::queue](queue.md), and against the ring this one replaced (September 2026), Lamport's with each side caching the other's index, built the same day from the same probe:

| threads | `spsc_queue` | Lamport's ring | `concurrent::bounded_queue` | `concurrent::queue` |
|---|---|---|---|---|
| 1 | 5.1 | 4.9 | 7.5 | 56 |
| 1 + 1 | 5.7 | 21 to 27 | 30 | 72 |

Between two threads an element is one line crossing between the cores, the cell's, with the sequence and the `int` on it: 5.7 ns, what one thread pays doing both. Lamport's ring at the same run is 21 to 27: a producer that does nothing but push keeps the ring full, and a consumer at its heels makes it reload the head, the consumer's word, at nearly every push, a second line crossing per element and the first taken back by the consumer for its next pop (the 2.5 ns of the previous version of this table is that ring with room to run, the two sides out of step by a part of a lap, which a stream between two threads on a busy machine is not). The bounded MPMC queue pays a compare-exchange per operation and the unbounded queue a node per element. On the channel's harness, an object allocated per push, [benchmarks](benchmarks.md#the-single-producer-queue-and-the-cache) has the ring against the bounded queue.

## See also

- [concurrent::bounded_queue](bounded_queue.md) for any number of producers and consumers; [concurrent::queue](queue.md), the unbounded queue
- [channel](../async/channel.md), a ring with the synchronization of both ends and coroutines waiting on it
- [atomic](../core/atomic.md), what the sequences, the head and the tail are
- [Benchmarks](benchmarks.md#the-single-producer-queue-and-the-cache): the ring on the channel's harness, an object allocated per item
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
