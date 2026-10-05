[sgcl](../../README.md) › [core](../README.md)

# sgcl::queue\<T, Container\>

```cpp
#include "sgcl/core/queue.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class Container = deque<T>>
    class queue;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::queue<T, Container>` is `std::queue` over a managed container: a FIFO adapter with `front`, `back`, `push`,
`emplace`, `pop`, `empty`, `size`, `swap` and the comparisons of the container. The container is `sgcl::deque<T>`
by default; `sgcl::list<T>` works as well, as does any container with `front`, `back`, `push_back`,
`emplace_back` and `pop_front`.

The adapter adds nothing of its own: the container is the protected member `c`, as in `std`, and everything
about where the elements live, when they are destroyed and what a push costs is the container's
([deque](../deque/README.md), [list](../list/README.md)). Go has no queue type: a Go program queues on a slice, appending at the
end and reslicing from the front, and the array keeps what was resliced away; here `pop()` destroys the element
at once, as in `std`.

## Rules

- An element is destroyed by `pop()` and in the destructor, exactly as with `std::queue` over the same `std`
  container; the container's memory is the collector's ([Containers](../README.md#containers)).
- A `tracked_ptr` may not address an element ([The rules](../README.md#the-rules), 4); `front()` and `back()` are
  references, valid as long as the container's would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization.

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements, the container's `value_type`. |
| `Container` | The container the elements are kept in: `deque<T>` by default, or `list<T>`, or any container with `front`, `back`, `push_back`, `emplace_back` and `pop_front`. |

## Member types

| Type | Definition |
|---|---|
| `container_type` | `Container` |
| `value_type` | `Container::value_type` |
| `size_type` | `Container::size_type` |
| `reference` | `Container::reference` |
| `const_reference` | `Container::const_reference` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](queue.md) | constructs the queue |
| `(destructor)` | destroys the container, and with it the elements |
| [operator=](operator_assign.md) | assigns the contents |

#### Element access

| Function | Description |
|---|---|
| [front](front.md) | access the first element, the oldest |
| [back](back.md) | access the last element, the newest |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the queue is empty |
| [size](size.md) | the number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [push](push.md) | appends an element at the end |
| [emplace](emplace.md) | constructs an element in place at the end |
| [pop](pop.md) | removes the first element |
| [swap](swap.md) | swaps the contents |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the elements lexicographically, front to back |
| [swap](swap2.md) | swaps the contents of two queues |

## Deduction guides

```cpp
template<class Container>
queue(Container) -> queue<typename Container::value_type, Container>;
template<class InputIt>
queue(InputIt, InputIt) -> queue<std::iter_value_t<InputIt>>;
```

## Complexity

Every member is one call of the container's: `push` and `emplace` its `push_back` and `emplace_back`, `pop` its
`pop_front`, `front` and `back` its own. With `deque` and `list` each is constant.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// Breadth first over a graph with a queue
struct Vertex {
    int id;
    vector<tracked_ptr<Vertex>> edges;
    bool seen = false;
};

int main() {
    // A ring of vertices with a chord from every third: the whole graph is one cycle
    vector<tracked_ptr<Vertex>> vertices;
    for (int i : range(12)) {
        vertices.push_back(make_tracked<Vertex>(i));
    }
    for (int i : range(12)) {
        vertices[i]->edges.push_back(vertices[(i + 1) % 12]);
        if (i % 3 == 0) {
            vertices[i]->edges.push_back(vertices[(i + 6) % 12]);
        }
    }
    tracked_ptr start = vertices[0];
    vertices.clear();  // the graph is reachable through `start` only

    // The queue roots the vertices waiting to be visited
    queue<tracked_ptr<Vertex>> pending;
    pending.push(start);
    start->seen = true;
    vector<int> order;
    while (!pending.empty()) {
        tracked_ptr v = pending.front();
        pending.pop();  // the pointer is destroyed, the vertex lives on
        order.push_back(v->id);
        for (const auto& w : v->edges) {
            if (!w->seen) {
                w->seen = true;
                pending.push(w);
            }
        }
    }
    println("{}", order);
}
```

Output:

```text
[0, 1, 6, 2, 7, 3, 8, 4, 9, 5, 10, 11]
```

## See also

- [priority_queue](../priority_queue/README.md): the adapter that gives the largest element first
- [stack](../stack/README.md): the LIFO adapter
- [deque](../deque/README.md), [list](../list/README.md): the containers a queue may sit on
- [concurrent::queue](../../concurrent/queue/README.md): the lock-free queue shared by threads
- [tracked_ptr](../tracked_ptr/README.md), [make_tracked](../make_tracked.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
