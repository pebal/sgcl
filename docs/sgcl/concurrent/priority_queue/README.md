[sgcl](../../README.md) › [concurrent](../README.md)

# sgcl::concurrent::priority_queue\<T, Compare\>

```cpp
#include "sgcl/concurrent/priority_queue.h"   // or "sgcl/concurrent.h"

namespace sgcl::concurrent {
    template<class T, class Compare = std::less<T>>
    class priority_queue;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::concurrent::priority_queue<T, Compare>` is a priority queue shared by any number of producers and
consumers: a binary heap under a lock, the counterpart of Java's `PriorityBlockingQueue`, which is the same design
(Go's library has none). The element that is least by `Compare` comes out first, as from `std::priority_queue`
with its comparison reversed: `std::less` gives the smallest first, `std::greater` the largest. A push sifts the
element up the heap and a pop takes the front and sifts the last element down, both logarithmic, both under the
lock; `try_top` copies the front under it; `empty` and `size` read a count kept beside the heap, without the lock.

Not lock-free, on purpose. This class was first the skip-list priority queue of Shavit and Lotan over the list of
[sorted_map](../sorted_map/README.md), and it lost to every heap under a lock it was measured against, two to fourteen
times per operation at one to sixteen threads
([Benchmarks](../benchmarks.md#the-priority-queue-against-the-skip-list-it-replaced)): the front of a priority queue
is one place every consumer writes, a lock-free structure pays for every pop with a race on the cache lines of
that front, and a heap under a lock keeps them in one thread's cache at a time; the authors of
`java.util.concurrent` found the same, and `PriorityBlockingQueue` is a heap under a `ReentrantLock`. The lock here
is a test-and-test-and-set with the exponential backoff of the [stack](../stack/README.md) (`config::backoff_max`) and a park
on the word after the spin: a thread that finds the lock taken for the few dozen nanoseconds of another's push or
pop spins through it, and only one that would wait longer goes to the kernel; an unlock wakes a parked thread only
when there is one. What the lock costs the program: a thread descheduled while it holds the lock holds the others
up for the length of its time slice, which the lock-free containers of this module never do; for a queue whose
consumers are many and whose front is hot that is the price of being several times faster.

Equal elements come out in the order they went in: every element carries the number its push took from a
counter, and the heap orders by the value and then the number, FIFO among equals, which a plain heap does not
give. The heap is a [vector](../../core/vector/README.md) on the managed heap, so an element holding a `tracked_ptr` is
traced in its buffer. The interface has the names of `std::priority_queue` and of the other concurrent
containers; there is no `top()` by reference, and `pop` returns the element.

## Rules

- The container is the heap's vector, the lock and its counters.
- Every member function may be called from any thread at any time. `push`, `emplace`, `try_pop`, `try_top` and
  `clear` take the lock; `empty`, `size` and `value_comp` do not. `pop` waits while the queue is empty.
- An element is moved out at the pop into the `optional` returned, and destroyed then, as `std::priority_queue`
  destroys it; `clear` destroys every element at once. A `tracked_ptr` element keeps its object alive while it is
  in the heap and not a moment longer.
- Equal elements, by `Compare`, come out in the order they were pushed. Two elements neither of which is less than
  the other are equal, so a comparator over a part of the element, a priority field, gives a FIFO queue per
  priority.
- `Compare` is called under the lock, by the pushing and popping threads, on the elements in the heap: it reads its
  arguments and nothing else, as a `std::map`'s comparator does.
- Non-copyable, non-movable: a shared structure has one place.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements: any move-constructible object type. `try_top` requires it to be copy-constructible; its move constructor should not throw. |
| `Compare` | A strict weak order over `T`: `comp(a, b)` is `true` when `a` comes out before `b`. `std::less<T>` by default, the smallest first. Its call must be noexcept: one that is not is rejected at compile time, but for the function objects of `std` (`std::hash`, `std::equal_to`, `std::less`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `value_type` | `T` |
| `value_compare` | `Compare` |
| `size_type` | `size_t` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](priority_queue.md) | constructs the queue |
| `(destructor)` | destroys the elements still in the heap, as its vector does |

#### Element access

| Function | Description |
|---|---|
| [try_top](try_top.md) | a copy of the least element, or nothing when the queue is empty |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the queue holds an element |
| [size](size.md) | the number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [push](push.md) | inserts an element |
| [emplace](emplace.md) | constructs an element in place |
| [try_pop](try_pop.md) | takes the least element, or nothing when the queue is empty |
| [pop](pop.md) | takes the least element, waiting for one |
| [clear](clear.md) | destroys every element |

#### Observers

| Function | Description |
|---|---|
| [value_comp](value_comp.md) | a copy of the comparator |

## Complexity

- `push`, `emplace`, `try_pop`: logarithmic in the size, under the lock.
- `try_top`: constant, under the lock, plus the copy of the element.
- `empty`, `size`: constant, without the lock. `clear`: linear in the size.

On an empty queue the lock is the cost, 12 to 14 ns per operation at one to four threads; on a queue of 100,000
elements the sift of seventeen levels is, 70 ns at any number of threads
([Benchmarks](../benchmarks.md#the-bounded-queue-the-priority-queue-intern-and-the-weak-map)).

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Job {
    int priority, producer, seq;
};

struct ByPriority {
    bool operator()(const tracked_ptr<Job>& a, const tracked_ptr<Job>& b) const noexcept {
        return a->priority < b->priority;
    }
};

struct Scheduler {
    concurrent::priority_queue<tracked_ptr<Job>, ByPriority> jobs;  // inside a managed object
};

int main() {
    tracked_ptr scheduler = make_tracked<Scheduler>();
    vector<thread> producers;
    for (int p : range(4)) {
        producers.emplace_back([&, p] {
            for (int i : range(1000)) {
                scheduler->jobs.emplace(make_tracked<Job>(i % 3, p, i));
            }
        });
    }
    for (auto& t : producers) {
        t.join();
    }

    int count[3] = {0, 0, 0}, out_of_order = 0, priority = 0;
    int last[4] = {-1, -1, -1, -1};
    while (auto job = scheduler->jobs.try_pop()) {  // the least priority first
        tracked_ptr j = *job;
        if (j->priority != priority) {
            priority = j->priority;
            for (int& l : last) {
                l = -1;
            }
        }
        out_of_order += j->seq <= last[j->producer];  // equal priorities: FIFO per producer
        last[j->producer] = j->seq;
        ++count[j->priority];
    }
    println("priority 0: {}, 1: {}, 2: {}", count[0], count[1], count[2]);
    println("{} out of order", out_of_order);
}
```

Output:

```text
priority 0: 1336, 1: 1332, 2: 1332
0 out of order
```

## See also

- [queue](../queue/README.md): the FIFO queue; [stack](../stack/README.md): the LIFO one; [bounded_queue](../bounded_queue/README.md): a ring
- [vector](../../core/vector/README.md): the heap's buffer
- [priority_queue](../../core/queue/README.md): the sequential adapter, the largest first as `std::priority_queue`
- [README: Lock-free containers](../README.md#lock-free-containers)
