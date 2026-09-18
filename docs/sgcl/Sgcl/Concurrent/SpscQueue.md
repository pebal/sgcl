# Sgcl::SpscQueue

```cpp
#include "sgcl/Sgcl/Concurrent/SpscQueue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class SpscQueue;
}
```

The same class in the `sgcl` interface: [spsc_queue](../../concurrent/spsc_queue.md).

`SpscQueue<T>` is a bounded wait-free FIFO queue for exactly one producer thread and one consumer thread: a ring of cells, a power of two of them, fixed at construction, with a sequence number per cell, the way the [ConcurrentBoundedQueue](ConcurrentBoundedQueue.md) numbers its cells, and without its compare-exchange, since the producer alone moves the tail and the consumer alone the head. A head and a tail count up forever, a cell being the position masked; a cell's sequence says whose the cell is: the position, once the cell is free for an enqueue at it, or the position with a published bit, once the element at it is in. An enqueue looks at the sequence of the cell at the tail, constructs the element there and publishes the cell; a dequeue looks at the sequence of the cell at the head, moves the element out, destroys it, and frees the cell for the next lap. Each side reads its own index only, which the other never touches: what the two sides share is the cell, its sequence and its element on one line, so an element costs one line crossing between the cores and no more, and an enqueue and a dequeue between two threads cost what the pair costs on one (Lamport's ring, with each side caching the other's index, shares the indices instead, and cost twice as much with the consumer at the producer's heels: [spsc_queue](../../concurrent/spsc_queue.md#measured)). The buffer is raw storage the queue constructs elements in and destroys them in, at enqueue and dequeue time, as a [List](../Containers/List.md) does in its buffer: the collector traces every cell (an unconstructed or destroyed element holds null pointers only) and never destroys one, so an element holding a `Ptr` keeps its object alive for exactly as long as it sits in the ring ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). The interface is that of the [ConcurrentBoundedQueue](ConcurrentBoundedQueue.md): `TryEnqueue`, `TryEmplace`, `TryDequeue`, a blocking `Enqueue` and `Dequeue`, `Count`, `Capacity`, `IsEmpty`, `IsFull`. The element type is any movable `T`, a `Ptr` included.

## Rules

- One producer and one consumer, and no more: the producer's side of the queue may be used by one thread at a time and the consumer's side by one thread at a time (the same thread may be both). A second thread on either side is a data race on that side's index; several producers or consumers take a [ConcurrentBoundedQueue](ConcurrentBoundedQueue.md).
- The container is three cache lines (`config::CacheLineSize`): the buffer, the mask and the count of waiters, read by both sides and written by neither but a side about to wait; the head, the consumer's line; the tail, the producer's. The queue holds its buffer by a `Ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- The capacity is rounded up to a power of two, at least one; `Capacity()` is what the ring holds.
- `TryEnqueue`, `TryEmplace` and `TryDequeue` are wait-free: a load, a store, no compare-exchange. The queue is FIFO. `Enqueue` waits while the queue is full, on the cell at the tail, which the dequeue that frees it notifies; `Dequeue` waits while it is empty, on the cell at the head, which the enqueue that publishes it notifies: a spin of a few microseconds first (a side that has just caught up with the other is nanoseconds from its next element), then a wait in the kernel.
- An element is constructed in its cell by the producer and moved out of the cell by the consumer, into the `Optional` returned, and destroyed in the cell there and then, on the consumer's thread. A constructor that throws leaves the queue as it was: the tail moves only once the element is there. An element whose move constructor throws on the way out stays in its cell, the head unmoved, the exception the consumer's.
- `Count()` is the tail less the head, two loads: exact on the producer's thread and the consumer's, a snapshot of some moment on any other. `IsEmpty()` and `IsFull()` are `Count()` against zero and the capacity.
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); there is no `Peek()`: the first element is what `TryDequeue` returns.
- The elements still in the ring are destroyed with the queue, wherever it dies: on a stack, or in a sweep inside a managed object.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::spsc_queue<T>;
using SizeType = size_t;
```

### Constructors

```cpp
explicit SpscQueue(SizeType capacity);
SpscQueue(const SpscQueue&) = delete;
```

A queue of `capacity` rounded up to a power of two, at least one: one managed buffer of that many cells.

```cpp
struct Sample { int seq; double value; };
SpscQueue<Ptr<Sample>> samples(1000);   // a ring of 1024
```

### TryEnqueue, TryEmplace

```cpp
bool TryEnqueue(const T& value);
bool TryEnqueue(T&& value);
template<class... A> bool TryEmplace(A&&... a);
```

The producer's: constructs the element in the cell at the tail (from `a...` in place for `TryEmplace`) and moves the tail past it; `false`, and nothing constructed, when the ring is full: the cell at the tail still holds the element of the lap before, which its sequence says, so the producer reads no word of the consumer's.

```cpp
struct Sample { int seq; double value; };
SpscQueue<Ptr<Sample>> samples(4);
int dropped = 0;
if (!samples.TryEnqueue(Make<Sample>(1, 0.5))) {   // full: the producer decides
    dropped++;
}
samples.TryEmplace(Make<Sample>(2, 1.0));
```

### Enqueue

```cpp
void Enqueue(T value);
```

The producer's: the element appended, waiting for room while the ring is full, on the cell at the tail, which the dequeue that frees it notifies.

```cpp
struct Sample { int seq; double value; };
SpscQueue<Ptr<Sample>> samples(4);
samples.Enqueue(Make<Sample>(3, 1.5));   // blocks until a dequeue makes room
```

### TryDequeue, Dequeue

```cpp
Optional<T> TryDequeue();
T Dequeue();
```

The consumer's: `TryDequeue` moves the element out of the cell at the head, destroys it in the cell and moves the head past it; `None` when the ring is empty: the cell at the head not published, which its sequence says, so the consumer reads no word of the producer's. `Dequeue` takes the first element, waiting while the ring is empty, on the cell at the head, which the enqueue that publishes it notifies.

```cpp
struct Sample { int seq; double value; };
SpscQueue<Ptr<Sample>> samples(4);
samples.Enqueue(Make<Sample>(1, 0.5));
samples.Enqueue(Make<Sample>(2, 1.0));
double sum = 0;
while (auto s = samples.TryDequeue()) {
    sum += (*s)->value;
}
samples.Enqueue(Make<Sample>(3, 1.5));
Ptr next = samples.Dequeue();   // blocks until an enqueue
```

### Count, Capacity, IsEmpty, IsFull

```cpp
SizeType Count() const noexcept;
SizeType Capacity() const noexcept;
bool IsEmpty() const noexcept;
bool IsFull() const noexcept;
```

`Count` is the tail less the head, at most the capacity; `Capacity` the number of cells; `IsEmpty` and `IsFull` are `Count()` against zero and the capacity.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The queue inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A reader thread hands samples to a processing thread through a ring
// of 16: the reader waits when the ring is full, the processor when it
// is empty; the queue is a member of a managed object, the ring is
// allocated once and the samples are managed objects
struct Sample {
    int seq;
    double value;
};

struct Pipeline {
    SpscQueue<Ptr<Sample>> samples{16};   // inside a managed object
};

int main() {
    Ptr pipeline = Make<Pipeline>();
    Thread reader([&] {                                   // the one producer
        for (int i : Range(10000)) {
            pipeline->samples.Enqueue(Make<Sample>(i, i * 0.5));   // waits while the ring is full
        }
    });
    double sum = 0;                                       // the one consumer: this thread
    int outOfOrder = 0, last = -1;
    for (int i : Range(10000)) {
        Ptr s = pipeline->samples.Dequeue();              // waits while the ring is empty
        outOfOrder += s->seq != last + 1;
        last = s->seq;
        sum += s->value;
    }
    reader.Join();
    std::cout << "sum " << sum << ", " << outOfOrder << " out of order, "
              << pipeline->samples.Count() << " left in a ring of " << pipeline->samples.Capacity() << "\n";
    return sum == 24997500.0 && outOfOrder == 0 ? 0 : 1;
}
```

The output:

```
sum 2.49975e+07, 0 out of order, 0 left in a ring of 16
```

## See also

- [ConcurrentBoundedQueue](ConcurrentBoundedQueue.md) for any number of producers and consumers; [ConcurrentQueue](ConcurrentQueue.md), the unbounded queue
- [Channel](../Async/Channel.md), a ring with the synchronization of both ends and coroutines waiting on it
- [Atomic](Atomic.md), what the sequences, the head and the tail are
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
