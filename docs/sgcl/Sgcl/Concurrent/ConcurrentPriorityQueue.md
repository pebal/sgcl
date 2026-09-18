# Sgcl::ConcurrentPriorityQueue

```cpp
#include "sgcl/Sgcl/Concurrent/ConcurrentPriorityQueue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Compare = std::less<T>>
    class ConcurrentPriorityQueue;
}
```

The same class in the `sgcl` interface: [concurrent_priority_queue](../../concurrent/concurrent_priority_queue.md).

`ConcurrentPriorityQueue<T, Compare>` is a priority queue shared by any number of producers and consumers: a binary heap under a lock, the counterpart of Java's `PriorityBlockingQueue`, which is the same design (Go's library has none). The element that is least by `Compare` comes out first: `std::less` gives the smallest first, `std::greater` the largest. An enqueue sifts the element up the heap, a dequeue takes the front and sifts the last element down, both logarithmic, both under the lock; `TryPeek` copies the front under it; `IsEmpty` and `Count` read a count kept beside the heap, without the lock.

Not lock-free, on purpose: this class was first a lock-free skip-list priority queue, and it lost to every heap under a lock it was measured against, two to fourteen times per operation ([benchmarks](../../concurrent/benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map)), because the front of a priority queue is one place every consumer writes, and a heap under a lock keeps its cache lines in one thread's cache at a time where a lock-free structure races for them; `java.util.concurrent` came to the same design. The lock spins with a backoff for the few dozen nanoseconds of another thread's operation and parks on the word after that; an unlock wakes a parked thread only when there is one. The price: a thread descheduled while it holds the lock holds the others up for its time slice, which the lock-free containers of the module never do.

Equal elements come out in the order they went in: every element carries the number its enqueue took from a counter, and the heap orders by (value, number), FIFO among equals. The heap is a [List](../Containers/List.md)'s buffer on the managed heap, so an element holding a `Ptr` is traced in it; an element is moved out at the dequeue and destroyed then, so `T` need not be copyable (`TryPeek`, which copies the least element, is there for a copyable `T` only). `Dequeue` waits on an empty queue, on the count of the enqueues, which every enqueue raises after its element is in the heap.

## Rules

- The container is the heap's buffer, the lock and two counters. It lives where a `Ptr` may: on a thread's stack or inside a managed object ([The rules](../../core/README.md#the-rules), 1).
- Every operation may be called from any thread at any time; `Enqueue`, `Emplace`, `TryDequeue`, `TryPeek` and `Clear` take the lock, `IsEmpty` and `Count` do not. `Dequeue` blocks while the queue is empty, on the count of the enqueues; every `Enqueue` raises it and wakes a waiter.
- An element is moved out at the dequeue into the `Optional` returned, and destroyed then; `Clear` destroys every element at once. A `Ptr` element keeps its object alive while it is in the heap and not a moment longer.
- Equal elements, by `Compare`, come out in the order they were enqueued. Two elements neither of which is less than the other are equal, so a comparator over a part of the element, a priority field, gives a FIFO queue per priority.
- `Compare` is called under the lock, by the enqueuing and dequeuing threads, on the elements in the heap: it reads its arguments and nothing else, as a `SortedDictionary`'s comparator does.
- `Count()` and `IsEmpty()` read the count as of the last enqueue or dequeue completed, without the lock: a snapshot of no particular moment when other threads enqueue or dequeue, as Java's `size` is.
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); there is no `Peek()` by reference: `TryPeek` is a copy.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using ValueType = T;
using InnerType = sgcl::concurrent_priority_queue<T, Compare>;
using SizeType = size_t;
```

### Constructors

```cpp
ConcurrentPriorityQueue();
explicit ConcurrentPriorityQueue(const Compare& cmp);
template<std::input_iterator It> ConcurrentPriorityQueue(It first, It last, const Compare& cmp = Compare());
ConcurrentPriorityQueue(std::initializer_list<T> il, const Compare& cmp = Compare());
ConcurrentPriorityQueue(const ConcurrentPriorityQueue&) = delete;
```

An empty queue, nothing allocated until the first enqueue; the elements of the range or the list enqueued one by one.

```cpp
ConcurrentPriorityQueue<int> a;                              // std::less: the smallest first
ConcurrentPriorityQueue<int, std::greater<int>> b;           // the largest first
ConcurrentPriorityQueue<int> c = {3, 1, 2};                  // holds 1, 2, 3
auto shorter = [](const String& x, const String& y) { return x.Length() < y.Length(); };
ConcurrentPriorityQueue<String, decltype(shorter)> d(shorter);
```

### Enqueue, Emplace

```cpp
void Enqueue(const T& value);
void Enqueue(T&& value);
template<class... A> void Emplace(A&&... a);
```

Under the lock: the element (constructed from `a...` for `Emplace`) with the next number of the counter appended to the heap and sifted up, logarithmic. Then the count of the enqueues raised and a thread waiting in `Dequeue` woken.

```cpp
struct Request { int priority; void Handle() {} };
struct ByPriority {
    bool operator()(const Ptr<Request>& a, const Ptr<Request>& b) const noexcept {
        return a->priority < b->priority;
    }
};
ConcurrentPriorityQueue<Ptr<Request>, ByPriority> requests;
requests.Enqueue(Make<Request>(2));
requests.Emplace(Make<Request>(1));
```

### TryDequeue, Dequeue

```cpp
Optional<T> TryDequeue();
T Dequeue();
```

`TryDequeue` takes the least element under the lock: the front of the heap moved out, the last element sifted down into its place, logarithmic; the element, or `None` when the queue is empty. The move should not throw: an element whose move throws is left at the back of the heap out of its order, as `std::priority_queue` leaves it. `Dequeue` takes the least element, waiting while the queue is empty: a spin first, then a park on the count of the enqueues.

```cpp
while (auto r = requests.TryDequeue()) {   // the least priority first
    (*r)->Handle();
}
requests.Enqueue(Make<Request>(0));
Ptr next = requests.Dequeue();   // blocks until an enqueue
```

### TryPeek

```cpp
Optional<T> TryPeek() const;
```

A copy of the least element, taken under the lock, or `None` when the queue is empty; the queue is not changed. For a copyable `T` only.

```cpp
ConcurrentPriorityQueue<int> q = {5, 2, 8};
if (auto least = q.TryPeek()) {   // a copy of 2; the queue still holds it
    std::cout << *least << " " << q.Count() << "\n";   // 2 3
}
```

### Equal elements

Elements equal by `Compare` come out in the order they went in:

```cpp
ConcurrentPriorityQueue<int> ties;
ties.Enqueue(1);
ties.Enqueue(1);
ties.Enqueue(0);
std::cout << *ties.TryDequeue() << *ties.TryDequeue() << *ties.TryDequeue() << "\n";   // 011
```

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

The count kept beside the heap, as of the last enqueue or dequeue completed, read without the lock: constant time.

### Clear

```cpp
void Clear();
```

Destroys every element, under the lock; the heap's buffer stays for the next enqueues.

### Comparer

```cpp
Compare Comparer() const;
```

A copy of the comparator.

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

// Jobs from many threads, taken in the order of their priority, equal
// priorities in the order they came; the queue is a member of a managed
// object, its heap a buffer on the managed heap
struct Job {
    int priority, producer, seq;
};

struct ByPriority {
    bool operator()(const Ptr<Job>& a, const Ptr<Job>& b) const noexcept {
        return a->priority < b->priority;
    }
};

struct Dispatcher {
    ConcurrentPriorityQueue<Ptr<Job>, ByPriority> jobs;   // inside a managed object
};

int main() {
    Ptr dispatcher = Make<Dispatcher>();
    List<Thread> producers;
    for (int p : Range(4)) {
        producers.Emplace([&, p] {
            for (int i : Range(1000)) {
                dispatcher->jobs.Emplace(Make<Job>(i % 3, p, i));
            }
        });
    }
    for (auto& t : producers) {
        t.Join();
    }
    int taken = 0, outOfOrder = 0, count[3] = {0, 0, 0};
    int priority = 0, last[4] = {-1, -1, -1, -1};
    while (auto job = dispatcher->jobs.TryDequeue()) {   // the least priority first, at every dequeue
        Ptr j = *job;
        if (j->priority != priority) {                  // the next priority: every producer's jobs from the start
            priority = j->priority;
            for (int& l : last) {
                l = -1;
            }
        }
        outOfOrder += j->seq <= last[j->producer];      // equal priorities: FIFO per producer
        last[j->producer] = j->seq;
        ++count[j->priority];
        ++taken;
    }
    std::cout << taken << " jobs: " << count[0] << " of priority 0, " << count[1] << " of priority 1, "
              << count[2] << " of priority 2; " << outOfOrder << " out of order\n";
    return taken == 4000 && outOfOrder == 0 ? 0 : 1;
}
```

The output:

```
4000 jobs: 1336 of priority 0, 1332 of priority 1, 1332 of priority 2; 0 out of order
```

## See also

- [ConcurrentQueue](ConcurrentQueue.md) for the FIFO queue, [ConcurrentStack](ConcurrentStack.md) for the LIFO one, [ConcurrentBoundedQueue](ConcurrentBoundedQueue.md) for a ring
- [PriorityQueue](../Containers/Queue.md), the sequential adapter (the largest first, as `std::priority_queue`)
- [README: Lock-free containers](../../concurrent/README.md#lock-free-containers), [README: The rules](../../core/README.md#the-rules)
