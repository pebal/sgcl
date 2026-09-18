# sgcl::concurrent_bounded_queue

```cpp
#include "sgcl/concurrent/concurrent_bounded_queue.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T>
    class concurrent_bounded_queue;
}
```

The same class in the `Sgcl` interface: [ConcurrentBoundedQueue](../Sgcl/Concurrent/ConcurrentBoundedQueue.md).

`sgcl::concurrent_bounded_queue<T>` is a bounded lock-free FIFO queue shared by any number of producers and consumers: the bounded MPMC queue of Dmitry Vyukov, the ring of Go's buffered `chan` and of Java's `ArrayBlockingQueue` without the lock. The elements live in a ring of cells, a power of two of them, fixed at construction; each cell carries a sequence number, and two counters, the enqueue and the dequeue position, count up forever, a cell being the position masked. A cell whose sequence equals the enqueue position is free for the producer that wins the position with a compare-exchange; the producer constructs the element in the cell and publishes it by storing the sequence as the position plus one, which is what the consumer at that position looks for; the consumer that wins the dequeue position moves the element out, destroys it, and stores the sequence as the position plus the number of cells, the enqueue position of the next lap. One compare-exchange per operation, on the word the producers share or on the one the consumers share, and no allocation per element: the cells are one managed buffer, taken once. A cell reserved and not yet published is nothing to a consumer, a cell taken and not yet released nothing to a producer (the sequence says so), and a thread that finds its position stale reloads it, so no thread ever waits for another. The buffer is raw storage the queue constructs elements in and destroys them in, at push and pop time, as the [vector](../containers/vector.md) does in its buffer: the collector traces every cell (an unconstructed or destroyed element holds null pointers only) and never destroys one, so an element holding a `tracked_ptr` keeps its object alive for exactly as long as it sits in the ring ([README: Lock-free containers](README.md#lock-free-containers)). The interface has the names of `std::queue` and of the [concurrent_queue](concurrent_queue.md): `try_push`, `try_emplace`, `try_pop`, a blocking `push` and `pop`, `size`, `capacity`, `empty`, `full`. The element type is any movable `T`, a `tracked_ptr` included.

## Rules

- The container is three cache lines (`config::CacheLineSize`): the buffer, the mask and the count of waiters, read by every thread and written by none but a thread about to wait; the enqueue position, the producers' word; the dequeue position, the consumers'. The queue holds its buffer by a `tracked_ptr`, so it lives where one may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- The capacity is rounded up to a power of two, at least two (in a ring of one cell, a cell published at a position and free at the next would carry one number); `capacity()` is what the ring holds.
- `try_push`, `try_emplace` and `try_pop` are lock-free and may be called from any thread at any time; each is linearizable at its compare-exchange on a position. The queue is FIFO: every producer's elements come out in the order it pushed them, at every consumer. `push` waits while the queue is full and `pop` while it is empty, each on the sequence of the cell at its position, which the other side's store to that cell notifies: a spin of a few microseconds first (a side that has just caught up with the other is nanoseconds from its next element), then a wait in the kernel.
- An element is constructed in its cell by the thread that pushes it and moved out of the cell by the thread that pops it, into the `optional` returned, and destroyed in the cell there and then, on the popping thread. A constructor that throws leaves the queue as it was: the cell the producer won is published empty and skipped by the consumer that reaches it, and the exception reaches the caller of the push. An element whose move constructor throws on the way out is destroyed and lost, the queue intact.
- `size()` is the enqueue position less the dequeue one, two loads: a snapshot of no particular moment when other threads push or pop, a cell reserved by a push or a pop in progress counted with the side that reserved it. `empty()` and `full()` are `size()` against zero and the capacity.
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
explicit concurrent_bounded_queue(size_type capacity);
concurrent_bounded_queue(const concurrent_bounded_queue&) = delete;
```

A queue of `capacity` rounded up to a power of two, at least two: one managed buffer of that many cells, every cell free.

```cpp
sgcl::concurrent_bounded_queue<sgcl::tracked_ptr<Request>> requests(1000);   // a ring of 1024
```

### try_push, try_emplace

```cpp
bool try_push(const T& value);
bool try_push(T&& value);
template<class... A> bool try_emplace(A&&... a);
```

Wins the enqueue position when the cell there is free, constructs the element in the cell (from `a...` in place for `try_emplace`) and publishes it; `false`, and nothing constructed, when the cell still holds the element of the previous lap, the queue full at that moment.

```cpp
if (!requests.try_push(sgcl::make_tracked<Request>(1))) {   // full: the caller decides
    drop_or_wait();
}
requests.try_emplace(sgcl::make_tracked<Request>(2));
```

### push

```cpp
void push(T value);
```

The element appended, waiting for room while the queue is full: on the sequence of the cell at the enqueue position, which the consumer releasing that cell notifies.

```cpp
requests.push(sgcl::make_tracked<Request>(3));   // blocks until a pop makes room
```

### try_pop, pop

```cpp
optional<T> try_pop();   // sgcl::optional, the alias of std::optional (sgcl/core/aliases.h)
T pop();
```

`try_pop` wins the dequeue position when the cell there is published, moves the element out, destroys it in the cell and releases the cell for the next lap; nothing when the cell is not published yet, the queue empty at that moment (or its first element still being written by the producer that won it). `pop` takes the first element, waiting while the queue is empty, on the sequence of the cell at the dequeue position, which the producer publishing it notifies.

```cpp
while (auto r = requests.try_pop()) {
    (*r)->handle();
}
sgcl::tracked_ptr next = requests.pop();   // blocks until a push
```

### size, capacity, empty, full

```cpp
size_type size() const noexcept;
size_type capacity() const noexcept;
bool empty() const noexcept;
bool full() const noexcept;
```

`size` is the enqueue position less the dequeue one, at most the capacity; `capacity` the number of cells; `empty` and `full` are `size()` against zero and the capacity.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A pipeline stage with bounded buffering: producers push messages and
// wait when the ring is full, consumers pop them in order; the queue is
// a member of a managed object, and the ring is allocated once
struct Message {
    int producer, seq;
};

struct Stage {
    sgcl::concurrent_bounded_queue<sgcl::tracked_ptr<Message>> inbox{64};   // inside a managed object: sgcl::
};

int main() {
    sgcl::tracked_ptr stage = sgcl::make_tracked<Stage>();
    sgcl::atomic<int> received = 0, out_of_order = 0;
    sgcl::vector<sgcl::thread> threads;
    for (int p : sgcl::range(4)) {
        threads.emplace_back([&, p] {
            for (int i : sgcl::range(1000)) {
                stage->inbox.push(sgcl::make_tracked<Message>(p, i));   // waits while the ring is full
            }
        });
        threads.emplace_back([&] {
            int last[4] = {-1, -1, -1, -1};
            for (int i : sgcl::range(1000)) {
                sgcl::tracked_ptr m = stage->inbox.pop();   // FIFO per producer, at every consumer
                out_of_order += m->seq <= last[m->producer];
                last[m->producer] = m->seq;
                ++received;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    std::cout << received << " messages, " << out_of_order << " out of order, "
              << stage->inbox.size() << " left in a ring of " << stage->inbox.capacity() << "\n";
    return received == 4000 && out_of_order == 0 ? 0 : 1;
}
```

The output:

```
4000 messages, 0 out of order, 0 left in a ring of 64
```

## Measured

Nanoseconds per push and pop of an `int` through a ring of 1024, against the unbounded [concurrent_queue](concurrent_queue.md) (a node allocated per push), on the machine of [the benchmarks](benchmarks.md), `-O2`: one thread pushing and popping in turn; a producer and a consumer; four producers and four consumers, the blocking `push` and `pop`.

| threads | `concurrent_bounded_queue` | `concurrent_queue` |
|---|---|---|
| 1 | 7.1 | 70 |
| 1 + 1 | 9 | 104 |
| 4 + 4 | 37 | 340 |

The ring is a compare-exchange on one of two words and a store to a cell, and no allocation; the unbounded queue makes a node per element and walks to the end of the list. Between four producers and four consumers the ring's two words are what eight threads contend for, and the number is theirs. A lost exchange on a position backs off exponentially, as the [stack](concurrent_stack.md) does at its head, up to 1024 pauses: at eight producers and eight consumers that cap is the difference between 257 and 80 ns per item through a ring of 64 (228 and 60 through one of 1024), the storm of lost exchanges turned into near-serial ones; two and four threads do not feel it. Against Go's channel and Java's `ArrayBlockingQueue`: [benchmarks](benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map).

## See also

- [Benchmarks](benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map): measured against Go and Java

- [spsc_queue](spsc_queue.md) for one producer and one consumer, without a compare-exchange
- [concurrent_queue](concurrent_queue.md), the unbounded queue; [concurrent_stack](concurrent_stack.md), the LIFO counterpart
- [channel](../async/channel.md), a ring with the synchronization of both ends and coroutines waiting on it
- [atomic](atomic.md), what the positions and the sequences are
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
