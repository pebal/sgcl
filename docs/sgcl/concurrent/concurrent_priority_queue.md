# sgcl::concurrent_priority_queue

```cpp
#include "sgcl/concurrent/concurrent_priority_queue.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, class Compare = std::less<T>>
    class concurrent_priority_queue;
}
```

`sgcl::concurrent_priority_queue<T, Compare>` is a priority queue shared by any number of producers and consumers: a binary heap under a lock, the counterpart of Java's `PriorityBlockingQueue`, which is the same design (Go's library has none). The element that is least by `Compare` comes out first, as from `std::priority_queue` with its comparison reversed: `std::less` gives the smallest first, `std::greater` the largest. A push sifts the element up the heap, a pop takes the front and sifts the last element down, both logarithmic, both under the lock; `try_top` copies the front under it; `empty` and `size` read a count kept beside the heap, without the lock.

Not lock-free, on purpose. This class was first the skip-list priority queue of Shavit and Lotan over the list of [concurrent_map](concurrent_map.md), and it lost to every heap under a lock it was measured against, two to fourteen times per operation at one to sixteen threads ([benchmarks](benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map)): the front of a priority queue is one place every consumer writes, a lock-free structure pays for every pop with a race on the cache lines of that front, and a heap under a lock keeps them in one thread's cache at a time; the authors of `java.util.concurrent` found the same, and `PriorityBlockingQueue` is a heap under a `ReentrantLock`. The lock here is a test-and-test-and-set with the exponential backoff of the [stack](concurrent_stack.md) (`config::BackoffMax`) and a park on the word after the spin: a thread that finds the lock taken for the few dozen nanoseconds of another's push or pop spins through it, and only one that would wait longer goes to the kernel; an unlock wakes a parked thread only when there is one. What the lock costs the program: a thread descheduled while it holds the lock holds the others up for the length of its time slice, which the lock-free containers of this module never do; for a queue whose consumers are many and whose front is hot that is the price of being several times faster.

Equal elements come out in the order they went in: every element carries the number its push took from a counter, and the heap orders by (value, number), FIFO among equals, which a plain heap does not give. The heap is a [vector](../containers/vector.md) on the managed heap, so an element holding a `tracked_ptr` is traced in its buffer; an element is moved out at the pop and destroyed then, as `std::priority_queue`'s is, so `T` need not be copyable (`try_top`, which copies the least element, is there for a copyable `T` only). The interface has the names of `std::priority_queue` and of the other concurrent containers: `push`, `emplace`, `try_pop`, `pop`, `try_top`, `empty`, `size`, `clear`. `pop` waits on an empty queue, on the count of the pushes, which every push raises after its element is in the heap.

Measured once, on the machine of [the benchmarks](benchmarks.md) at `-O2`, every thread pushing a pseudo-random priority and popping the least, 200 k times, over an empty queue (it stays as short as the number of threads: the lock is the cost) and over one holding 100 k elements (the sift of seventeen levels is), nanoseconds per operation over all the threads, against `std::priority_queue` under a `std::mutex` (the system's mutex, which parks at once under contention), Go's `container/heap` under a `sync.Mutex` and Java's `PriorityBlockingQueue`; the skip-list queue this class replaced in the last column:

| queue, threads | `concurrent_priority_queue` | `std::priority_queue`, `std::mutex` | Go heap, mutex | Java `PriorityBlockingQueue` | skip list (before) |
|---|---|---|---|---|---|
| empty, 1 | 12.4 | 22 | 17 | 29 | 56 |
| empty, 4 | 14.4 | 73 | 107 | 29 | 183 |
| empty, 16 | 31 | 50 | 127 | 24 | 351 |
| 100 k, 1 | 69 | 83 | 94 | 90 | |
| 100 k, 4 | 70 | 350 | 183 | 121 | |
| 100 k, 16 | 70 | 171 | 268 | 97 | |

The lock is what a lock costs when it is taken for a few dozen nanoseconds: a spin through the other thread's operation, at 14 ns per operation across four threads, where the system's mutex parks and wakes (73) and Go's queues its goroutines (107); at sixteen threads on an empty queue Java's `ReentrantLock` with HotSpot's adaptive spinning is a fifth ahead (24 against 31), the one cell of the table it holds. On a long queue the sift down of a pop is the work, sixteen comparisons across the levels of a heap of 100 k, and the lock serializes it: 70 ns per operation at any number of threads, ahead of Java's heap by a third, of Go's by two and a half times.

## Rules

- The container is the heap's vector, the lock and two counters. It lives where a `tracked_ptr` may: on a thread's stack or inside a managed object ([The rules](../core/README.md#the-rules), 1).
- Every operation may be called from any thread at any time; `push`, `emplace`, `try_pop`, `try_top` and `clear` take the lock, `empty` and `size` do not. `pop` blocks while the queue is empty, on the count of the pushes; every `push` raises it and wakes a waiter.
- An element is moved out at the pop into the `optional` returned, and destroyed then, exactly as `std::priority_queue::pop` destroys it; `clear` destroys every element at once. A `tracked_ptr` element keeps its object alive while it is in the heap and not a moment longer.
- Equal elements, by `Compare`, come out in the order they were pushed. Two elements neither of which is less than the other are equal, so a comparator over a part of the element, a priority field, gives a FIFO queue per priority.
- `Compare` is called under the lock, by the pushing and popping threads, on the elements in the heap: it reads its arguments and nothing else, as a `std::map`'s comparator does; a comparator that throws leaves the heap as `std::push_heap` leaves it.
- `size()` and `empty()` read the count as of the last push or pop completed, without the lock: a snapshot of no particular moment when other threads push or pop, as Java's `size` is.
- A `tracked_ptr` may not address an element ([The rules](../core/README.md#the-rules), 4); there is no `top()` by reference: `try_top` is a copy.
- Non-copyable, non-movable: a shared structure has one place.

## Members

### Types

```cpp
using value_type = T;
using value_compare = Compare;
using size_type = size_t;
```

### Constructors

```cpp
concurrent_priority_queue();
explicit concurrent_priority_queue(const Compare& comp);
template<std::input_iterator InputIt> concurrent_priority_queue(InputIt first, InputIt last, const Compare& comp = Compare());
concurrent_priority_queue(std::initializer_list<T> ilist, const Compare& comp = Compare());
concurrent_priority_queue(const concurrent_priority_queue&) = delete;
```

An empty queue, nothing allocated until the first push; the elements of the range or the list pushed one by one.

```cpp
sgcl::concurrent_priority_queue<int> a;                              // std::less: the smallest first
sgcl::concurrent_priority_queue<int, std::greater<int>> b;           // the largest first
sgcl::concurrent_priority_queue<int> c = {3, 1, 2};                  // holds 1, 2, 3
auto shorter = [](const std::string& x, const std::string& y) { return x.size() < y.size(); };
sgcl::concurrent_priority_queue<std::string, decltype(shorter)> d(shorter);
```

### push, emplace

```cpp
void push(const T& value);
void push(T&& value);
template<class... A> void emplace(A&&... a);
```

Under the lock: the element (constructed from `a...` for `emplace`) with the next number of the counter appended to the heap's vector and sifted up, logarithmic. Then the count of the pushes raised and a thread waiting in `pop` woken.

```cpp
struct Request { int priority; void handle() {} };
struct ByPriority {
    bool operator()(const sgcl::tracked_ptr<Request>& a, const sgcl::tracked_ptr<Request>& b) const noexcept {
        return a->priority < b->priority;
    }
};
sgcl::concurrent_priority_queue<sgcl::tracked_ptr<Request>, ByPriority> requests;   // a global: sgcl::
requests.push(sgcl::make_tracked<Request>(2));
requests.emplace(sgcl::make_tracked<Request>(1));
```

### try_pop, pop

```cpp
optional<T> try_pop();   // sgcl::optional, the alias of std::optional (sgcl/core/aliases.h)
T pop();
```

`try_pop` takes the least element under the lock: the front of the heap moved out, the last element sifted down into its place, logarithmic; the element, or nothing when the queue is empty. The move should not throw: an element whose move throws is left at the back of the heap out of its order, as `std::priority_queue` leaves it. `pop` takes the least element, waiting while the queue is empty: a spin first, then a park on the count of the pushes.

```cpp
while (auto r = requests.try_pop()) {   // the least priority first
    (*r)->handle();
}
sgcl::tracked_ptr next = requests.pop();   // blocks until a push
```

### try_top

```cpp
optional<T> try_top() const;
```

A copy of the least element, taken under the lock, or nothing when the queue is empty; the queue is not changed. For a copyable `T` only.

```cpp
sgcl::concurrent_priority_queue<int> q = {5, 2, 8};
if (auto least = q.try_top()) {   // a copy of 2; the queue still holds it
    std::cout << *least << " " << q.size() << "\n";   // 2 3
}
```

### Equal elements

Elements equal by `Compare` come out in the order they went in:

```cpp
sgcl::concurrent_priority_queue<int> ties;
ties.push(1);
ties.push(1);
ties.push(0);
std::cout << *ties.try_pop() << *ties.try_pop() << *ties.try_pop() << "\n";   // 011
```

### empty, size

```cpp
bool empty() const noexcept;
size_type size() const noexcept;
```

The count kept beside the heap, as of the last push or pop completed, read without the lock: constant time.

### clear

```cpp
void clear();
```

Destroys every element, under the lock; the heap's buffer stays for the next pushes.

### value_comp

```cpp
value_compare value_comp() const;
```

A copy of the comparator.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// Jobs from many threads, taken in the order of their priority, equal
// priorities in the order they came; the queue is a member of a managed
// object, its heap a buffer on the managed heap
struct Job {
    int priority, producer, seq;
};

struct ByPriority {
    bool operator()(const sgcl::tracked_ptr<Job>& a, const sgcl::tracked_ptr<Job>& b) const noexcept {
        return a->priority < b->priority;
    }
};

struct Scheduler {
    sgcl::concurrent_priority_queue<sgcl::tracked_ptr<Job>, ByPriority> jobs;   // inside a managed object: sgcl::
};

int main() {
    sgcl::tracked_ptr scheduler = sgcl::make_tracked<Scheduler>();
    sgcl::vector<sgcl::thread> producers;
    for (int p : sgcl::range(4)) {
        producers.emplace_back([&, p] {
            for (int i : sgcl::range(1000)) {
                scheduler->jobs.emplace(sgcl::make_tracked<Job>(i % 3, p, i));
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }
    int taken = 0, out_of_order = 0, count[3] = {0, 0, 0};
    int priority = 0, last[4] = {-1, -1, -1, -1};
    while (auto job = scheduler->jobs.try_pop()) {   // the least priority first, at every pop
        sgcl::tracked_ptr j = *job;
        if (j->priority != priority) {               // the next priority: every producer's jobs from the start
            priority = j->priority;
            for (int& l : last) {
                l = -1;
            }
        }
        out_of_order += j->seq <= last[j->producer];   // equal priorities: FIFO per producer
        last[j->producer] = j->seq;
        ++count[j->priority];
        ++taken;
    }
    std::cout << taken << " jobs: " << count[0] << " of priority 0, " << count[1] << " of priority 1, "
              << count[2] << " of priority 2; " << out_of_order << " out of order\n";
    return taken == 4000 && out_of_order == 0 ? 0 : 1;
}
```

The output:

```
4000 jobs: 1336 of priority 0, 1332 of priority 1, 1332 of priority 2; 0 out of order
```

## See also

- [Benchmarks](benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map): measured against Go and Java

- [concurrent_queue](concurrent_queue.md) for the FIFO queue, [concurrent_stack](concurrent_stack.md) for the LIFO one, [concurrent_bounded_queue](concurrent_bounded_queue.md) for a ring
- [vector](../containers/vector.md), the heap's buffer
- [priority_queue](../containers/queue.md), the sequential adapter (the largest first, as `std::priority_queue`)
- [README: Lock-free containers](README.md#lock-free-containers), [README: The rules](../core/README.md#the-rules)
