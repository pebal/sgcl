[sgcl](../README.md) › [core](README.md)

# sgcl::priority_queue\<T, Container, Compare\>

```cpp
#include "sgcl/core/queue.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class Container = vector<T>,
             class Compare = std::less<typename Container::value_type>>
    class priority_queue;
}
```

`sgcl::priority_queue<T, Container, Compare>` is `std::priority_queue` over a managed container: a heap kept with
`std::push_heap`/`std::pop_heap`, `top()` the largest element under `Compare`. The container is `sgcl::vector<T>`
by default; `sgcl::deque<T>` works as well, as does any container with random-access iterators, `front`,
`push_back`, `emplace_back` and `pop_back`.

The adapter adds nothing of its own: the container is the protected member `c` and the comparator `comp`, as in
`std`, and everything about where the elements live, when they are destroyed and what a push costs is the
container's ([vector](vector.md), [deque](deque.md)). Go has no priority queue type: `container/heap` gives the
heap operations over a slice the program keeps; here the adapter keeps the container and the comparator
together. There are no comparisons of priority queues, as in `std`.

## Rules

- The container holds tracked pointers, so a priority queue lives where a `tracked_ptr` may: on a thread's stack
  or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine
  frame ([The rules](README.md#the-rules), 1).
- An element is destroyed by `pop()` and in the destructor, exactly as with `std::priority_queue` over the same
  `std` container; the container's memory is the collector's ([Containers](README.md#containers)).
- A `tracked_ptr` may not address an element ([The rules](README.md#the-rules), 4); `top()` is a reference, valid
  as long as the container's would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements, the container's `value_type`. |
| `Container` | The container the heap is kept in: `vector<T>` by default, or `deque<T>`, or any container with random-access iterators, `front`, `push_back`, `emplace_back` and `pop_back`. |
| `Compare` | The order of the heap, `std::less` by default: `top()` is an element no other is greater than under it, and `std::greater` makes it the smallest. Its call must be noexcept: a function object whose call is not is rejected at compile time, but for those of `std` (`std::less`, `std::greater`, …), taken as they are. |

## Member types

| Type | Definition |
|---|---|
| `container_type` | `Container` |
| `value_compare` | `Compare` |
| `value_type` | `Container::value_type` |
| `size_type` | `Container::size_type` |
| `reference` | `Container::reference` |
| `const_reference` | `Container::const_reference` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](priority_queue/priority_queue.md) | constructs the priority queue |
| `(destructor)` | destroys the container, and with it the elements |
| [operator=](priority_queue/operator_assign.md) | assigns the contents |

#### Element access

| Function | Description |
|---|---|
| [top](priority_queue/top.md) | access the largest element |

#### Capacity

| Function | Description |
|---|---|
| [empty](priority_queue/empty.md) | checks whether the priority queue is empty |
| [size](priority_queue/size.md) | the number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [push](priority_queue/push.md) | inserts an element and sifts it up |
| [emplace](priority_queue/emplace.md) | constructs an element in place and sifts it up |
| [pop](priority_queue/pop.md) | removes the largest element |
| [swap](priority_queue/swap.md) | swaps the contents |

## Non-member functions

| Function | Description |
|---|---|
| [swap](priority_queue/swap2.md) | swaps the contents of two priority queues |

## Deduction guides

```cpp
template<class Compare, class Container>
priority_queue(Compare, Container)
    -> priority_queue<typename Container::value_type, Container, Compare>;
template<class InputIt, class Compare = std::less<std::iter_value_t<InputIt>>>
priority_queue(InputIt, InputIt, Compare = Compare())
    -> priority_queue<std::iter_value_t<InputIt>, vector<std::iter_value_t<InputIt>>, Compare>;
template<class InputIt, class Compare, class Container>
priority_queue(InputIt, InputIt, Compare, Container)
    -> priority_queue<typename Container::value_type, Container, Compare>;
```

## Complexity

- `top`, `empty`, `size`: constant.
- `push`, `emplace`, `pop`: logarithmic in the size, plus the container's `push_back` or `pop_back` (amortized
  constant for `vector`).
- Construction from a container or a range: linear in the number of elements, the heap made by
  `std::make_heap`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Task {
    string name;
    int priority;
};

struct LowerPriority {
    bool operator()(const tracked_ptr<Task>& a, const tracked_ptr<Task>& b) const noexcept {
        return a->priority < b->priority;
    }
};

int main() {
    priority_queue<tracked_ptr<Task>, vector<tracked_ptr<Task>>, LowerPriority> tasks;
    tasks.push(make_tracked<Task>("write", 2));
    tasks.push(make_tracked<Task>("test", 5));
    tasks.push(make_tracked<Task>("review", 3));
    tasks.emplace(make_tracked<Task>("release", 1));

    while (!tasks.empty()) {
        println("{} {}", tasks.top()->priority, tasks.top()->name);
        tasks.pop();  // the pointer is destroyed, the task is the collector's
    }
}
```

Output:

```text
5 test
3 review
2 write
1 release
```

## See also

- [queue](queue.md): the FIFO adapter
- [stack](stack.md): the LIFO adapter
- [vector](vector.md), [deque](deque.md): the containers a priority queue may sit on
- [concurrent::priority_queue](../concurrent/priority_queue.md): the priority queue shared by threads
- [tracked_ptr](tracked_ptr.md), [make_tracked](make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](README.md#the-rules)
