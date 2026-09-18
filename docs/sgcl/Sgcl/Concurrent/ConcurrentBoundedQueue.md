# Sgcl::ConcurrentBoundedQueue

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentBoundedQueue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T>
    class ConcurrentBoundedQueue;
}
```

The same class in the `sgcl` interface: [concurrent_bounded_queue](../../concurrent/concurrent_bounded_queue.md).

`ConcurrentBoundedQueue<T>` is a bounded lock-free FIFO queue shared by any number of producers and consumers: the bounded MPMC queue of Dmitry Vyukov, the ring of Go's buffered `chan` and of Java's `ArrayBlockingQueue` without the lock. The elements live in a ring of cells, a power of two of them, fixed at construction; each cell carries a sequence number, and two counters, the enqueue and the dequeue position, count up forever, a cell being the position masked. A cell whose sequence equals the enqueue position is free for the producer that wins the position with a compare-exchange; the producer constructs the element in the cell and publishes it by storing the sequence as the position plus one, which is what the consumer at that position looks for; the consumer that wins the dequeue position moves the element out, destroys it, and stores the sequence as the position plus the number of cells, the enqueue position of the next lap. One compare-exchange per operation, on the word the producers share or on the one the consumers share, and no allocation per element: the cells are one managed buffer, taken once. A cell reserved and not yet published is nothing to a consumer, a cell taken and not yet released nothing to a producer (the sequence says so), and a thread that finds its position stale reloads it, so no thread ever waits for another. The buffer is raw storage the queue constructs elements in and destroys them in, at enqueue and dequeue time, as a [List](../Containers/List.md) does in its buffer: the collector traces every cell (an unconstructed or destroyed element holds null pointers only) and never destroys one, so an element holding a `Ptr` keeps its object alive for exactly as long as it sits in the ring ([README: Lock-free containers](../../concurrent/README.md#lock-free-containers)). The interface has the names of a bounded queue: `TryEnqueue`, `TryEmplace`, `TryDequeue`, a blocking `Enqueue` and `Dequeue`, `Count`, `Capacity`, `IsEmpty`, `IsFull`. The element type is any movable `T`, a `Ptr` included.

## Rules

- The container is three cache lines (`config::CacheLineSize`): the buffer, the mask and the count of waiters, read by every thread and written by none but a thread about to wait; the enqueue position, the producers' word; the dequeue position, the consumers'. The queue holds its buffer by a `Ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- The capacity is rounded up to a power of two, at least two (in a ring of one cell, a cell published at a position and free at the next would carry one number); `Capacity()` is what the ring holds.
- `TryEnqueue`, `TryEmplace` and `TryDequeue` are lock-free and may be called from any thread at any time; each is linearizable at its compare-exchange on a position. The queue is FIFO: every producer's elements come out in the order it enqueued them, at every consumer. `Enqueue` waits while the queue is full and `Dequeue` while it is empty, each on the sequence of the cell at its position, which the other side's store to that cell notifies: a spin of a few microseconds first (a side that has just caught up with the other is nanoseconds from its next element), then a wait in the kernel.
- An element is constructed in its cell by the thread that enqueues it and moved out of the cell by the thread that dequeues it, into the `Optional` returned, and destroyed in the cell there and then, on the dequeuing thread. A constructor that throws leaves the queue as it was: the cell the producer won is published empty and skipped by the consumer that reaches it, and the exception reaches the caller of the enqueue. An element whose move constructor throws on the way out is destroyed and lost, the queue intact.
- `Count()` is the enqueue position less the dequeue one, two loads: a snapshot of no particular moment when other threads enqueue or dequeue, a cell reserved by an enqueue or a dequeue in progress counted with the side that reserved it. `IsEmpty()` and `IsFull()` are `Count()` against zero and the capacity.
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); there is no `Peek()`: the first element is what `TryDequeue` returns.
- The elements still in the ring are destroyed with the queue, wherever it dies: on a stack, or in a sweep inside a managed object.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::concurrent_bounded_queue<T>;
using SizeType = size_t;
```

### Constructors

```cpp
explicit ConcurrentBoundedQueue(SizeType capacity);
ConcurrentBoundedQueue(const ConcurrentBoundedQueue&) = delete;
```

A queue of `capacity` rounded up to a power of two, at least two: one managed buffer of that many cells, every cell free.

```cpp
struct Request { int id; void Handle() {} };
ConcurrentBoundedQueue<Ptr<Request>> requests(1000);   // a ring of 1024
```

### TryEnqueue, TryEmplace

```cpp
bool TryEnqueue(const T& value);
bool TryEnqueue(T&& value);
template<class... A> bool TryEmplace(A&&... a);
```

Wins the enqueue position when the cell there is free, constructs the element in the cell (from `a...` in place for `TryEmplace`) and publishes it; `false`, and nothing constructed, when the cell still holds the element of the previous lap, the queue full at that moment.

```cpp
struct Request { int id; void Handle() {} };
ConcurrentBoundedQueue<Ptr<Request>> requests(4);
if (!requests.TryEnqueue(Make<Request>(1))) {   // full: the caller decides
    // drop it, or wait
}
requests.TryEmplace(Make<Request>(2));
```

### Enqueue

```cpp
void Enqueue(T value);
```

The element appended, waiting for room while the queue is full: on the sequence of the cell at the enqueue position, which the consumer releasing that cell notifies.

```cpp
struct Request { int id; void Handle() {} };
ConcurrentBoundedQueue<Ptr<Request>> requests(4);
requests.Enqueue(Make<Request>(3));   // blocks until a dequeue makes room
```

### TryDequeue, Dequeue

```cpp
Optional<T> TryDequeue();
T Dequeue();
```

`TryDequeue` wins the dequeue position when the cell there is published, moves the element out, destroys it in the cell and releases the cell for the next lap; `None` when the cell is not published yet, the queue empty at that moment (or its first element still being written by the producer that won it). `Dequeue` takes the first element, waiting while the queue is empty, on the sequence of the cell at the dequeue position, which the producer publishing it notifies.

```cpp
struct Request { int id; void Handle() {} };
ConcurrentBoundedQueue<Ptr<Request>> requests(4);
requests.Enqueue(Make<Request>(1));
requests.Enqueue(Make<Request>(2));
while (auto r = requests.TryDequeue()) {
    (*r)->Handle();
}
requests.Enqueue(Make<Request>(3));
Ptr next = requests.Dequeue();   // blocks until an enqueue
```

### Count, Capacity, IsEmpty, IsFull

```cpp
SizeType Count() const noexcept;
SizeType Capacity() const noexcept;
bool IsEmpty() const noexcept;
bool IsFull() const noexcept;
```

`Count` is the enqueue position less the dequeue one, at most the capacity; `Capacity` the number of cells; `IsEmpty` and `IsFull` are `Count()` against zero and the capacity.

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

// A pipeline stage with bounded buffering: producers enqueue messages
// and wait when the ring is full, consumers dequeue them in order; the
// queue is a member of a managed object, and the ring is allocated once
struct Message {
    int producer, seq;
};

struct Stage {
    ConcurrentBoundedQueue<Ptr<Message>> inbox{64};   // inside a managed object
};

int main() {
    Ptr stage = Make<Stage>();
    Atomic<int> received = 0, outOfOrder = 0;
    List<Thread> threads;
    for (int p : Range(4)) {
        threads.Emplace([&, p] {
            for (int i : Range(1000)) {
                stage->inbox.Enqueue(Make<Message>(p, i));   // waits while the ring is full
            }
        });
        threads.Emplace([&] {
            int last[4] = {-1, -1, -1, -1};
            for (int i : Range(1000)) {
                Ptr m = stage->inbox.Dequeue();   // FIFO per producer, at every consumer
                outOfOrder += m->seq <= last[m->producer];
                last[m->producer] = m->seq;
                ++received;
            }
        });
    }
    for (auto& t : threads) {
        t.Join();
    }
    std::cout << received << " messages, " << outOfOrder << " out of order, "
              << stage->inbox.Count() << " left in a ring of " << stage->inbox.Capacity() << "\n";
    return received == 4000 && outOfOrder == 0 ? 0 : 1;
}
```

The output:

```
4000 messages, 0 out of order, 0 left in a ring of 64
```

## See also

- [SpscQueue](SpscQueue.md) for one producer and one consumer, without a compare-exchange
- [ConcurrentQueue](ConcurrentQueue.md), the unbounded queue; [ConcurrentStack](ConcurrentStack.md), the LIFO counterpart
- [Channel](../Async/Channel.md), a ring with the synchronization of both ends and coroutines waiting on it
- [Atomic](Atomic.md), what the positions and the sequences are
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
