# Sgcl::Queue, Sgcl::PriorityQueue

```cpp
#include "sgcl/Sgcl/Containers/Queue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Container = Deque<T>>
    class Queue;

    template<class T, class Container = List<T>, class Compare = std::less<T>>
    class PriorityQueue;
}
```

The same classes in the `sgcl` interface: [queue, priority_queue](../../containers/queue.md).

`Queue<T, Container>` is `std::queue` over a managed container: a FIFO adapter with `Peek`, `Last`, `Enqueue`, `Emplace`, `Dequeue`, `IsEmpty`, `Count`, `Swap` and the comparisons of the container. The container is `Deque<T>` by default; `LinkedList<T>` works as well, as does any sequence of the interface with a first and a last element, an add at the back and a removal at the front. `Dequeue` hands the element back, where `std::queue::pop` only removes it; `TryDequeue` and `TryPeek` hand back `None` and null on an empty queue, where `Dequeue` and `Peek` must not be called.

`PriorityQueue<T, Container, Compare>` is `std::priority_queue` over a managed container: a heap kept with `std::push_heap`/`std::pop_heap`, `Peek()` the largest element under `Compare`. The container is `List<T>` by default; `Deque<T>` works as well, as does any sequence with random-access iterators, a first element and an add and a removal at the back.

The adapters add nothing of their own: everything about where the elements live, when they are destroyed and what an add costs is the container's ([Deque](Deque.md), [List](List.md), [LinkedList](LinkedList.md)).

## Rules

- The container holds tracked pointers, so an adapter lives where a `Ptr` may: on a thread's stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- An element is destroyed by `Dequeue()` (after it is moved out) and in the destructor, exactly as with the `std` adapter over the same `std` container; the container's memory is the collector's ([Containers](../../containers/README.md#containers)).
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); `Peek()` and `Last()` are references, valid as long as the container's would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization. A queue shared between threads is a [ConcurrentQueue](../Concurrent/ConcurrentQueue.md).

## Queue

### Types

```cpp
using ValueType = T;
using ContainerType = Container;
using InnerType = sgcl::queue<T, Container::InnerType>;
using SizeType = size_t;
```

### Constructors

```cpp
Queue();
explicit Queue(const Container& c);
explicit Queue(Container&& c);
template<std::input_iterator It> Queue(It first, It last);
Queue(std::initializer_list<T> il);
explicit Queue(InnerType q) noexcept;
```

An empty queue, a queue over a copy of `c` or over `c` itself (moved in), or a queue whose container is built from a range or a list, the first element at the front. Copy and move construction and assignment are the container's.

```cpp
Deque d = {1, 2, 3};
Queue<int> fromCopy(d);                              // Peek() is 1, d unchanged
Queue<int> fromMove(std::move(d));                   // d is empty now
List src = {4, 5};
Queue<int> fromRange(begin(src), end(src));        // Peek() is 4, Last() is 5
Queue<int, LinkedList<int>> onList;         // any managed sequence with AddLast and RemoveFirst
```

### Peek, TryPeek, Last

```cpp
T& Peek() noexcept;
const T& Peek() const noexcept;
T* TryPeek() noexcept;
const T* TryPeek() const noexcept;
T& Last() noexcept;
const T& Last() const noexcept;
```

The oldest and the newest element; `Peek` and `Last` on an empty queue are undefined, `TryPeek` is null then.

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

### Enqueue, Emplace

```cpp
void Enqueue(const T& value);
void Enqueue(T&& value);
template<class... A> decltype(auto) Emplace(A&&... a);
```

An add at the back of the container; `Emplace` returns what the container's emplace at the back returns, a reference to the new element for the containers of the interface.

```cpp
Queue<Ptr<int>> q;
q.Enqueue(Make<int>(1));
int& two = *q.Emplace(Make<int>(2));      // a reference to the element at the back
```

### Dequeue, TryDequeue

```cpp
T Dequeue();
Optional<T> TryDequeue();
```

The front element, moved out and removed from the container. `Dequeue` on an empty queue is undefined; `TryDequeue` is `None` then, C#'s `TryDequeue` as an `Optional`.

```cpp
Queue<int> q = {1};
Optional<int> first = q.TryDequeue();                // 1
Optional<int> none = q.TryDequeue();                 // None: the queue is empty
```

### Clear

```cpp
void Clear() noexcept;
```

Every element destroyed, the container replaced by an empty one.

### Swap

```cpp
void Swap(Queue& other) noexcept;
template<class T, class Container> void swap(Queue<T, Container>& l, Queue<T, Container>& r) noexcept;
```

Swaps the containers; no element is touched.

### Comparisons

```cpp
bool operator==(const Queue& l, const Queue& r);
auto operator<=>(const Queue& l, const Queue& r);
```

The comparisons of the containers, front to back: `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
Deque d = {1, 2};
Queue<int> a(d), b(d);
b.Enqueue(3);
bool less = a < b;                  // true: a prefix
```

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The adapter inside, as its own type.

## PriorityQueue

### Types

```cpp
using ValueType = T;
using ContainerType = Container;
using CompareType = Compare;
using InnerType = sgcl::priority_queue<T, Container::InnerType, Compare>;
using SizeType = size_t;
```

### Constructors

```cpp
PriorityQueue();
explicit PriorityQueue(const Compare& cmp);
PriorityQueue(const Compare& cmp, const Container& c);
PriorityQueue(const Compare& cmp, Container&& c);
template<std::input_iterator It> PriorityQueue(It first, It last, const Compare& cmp = Compare());
PriorityQueue(std::initializer_list<T> il, const Compare& cmp = Compare());
explicit PriorityQueue(InnerType q) noexcept;
```

An empty queue, or one over a copy of `c` or over `c` itself (moved in), or one with the elements of a range or a list; the container is then made a heap under `cmp`. Copy and move construction and assignment are the implicit ones.

```cpp
List values = {5, 1, 4, 1, 3};
PriorityQueue<int> maxHeap(begin(values), end(values));                                            // Peek() is 5
PriorityQueue<int, List<int>, std::greater<int>> minHeap(begin(values), end(values));     // Peek() is 1
PriorityQueue<int, Deque<int>> onDeque(std::less<int>(), Deque<int>{2, 9, 4});     // Peek() is 9
```

### Peek, TryPeek

```cpp
const T& Peek() const noexcept;
const T* TryPeek() const noexcept;
```

The largest element under `Compare`; `Peek` on an empty queue is undefined, `TryPeek` is null then.

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

### Enqueue, Emplace

```cpp
void Enqueue(const T& value);
void Enqueue(T&& value);
template<class... A> void Emplace(A&&... a);
```

Appends to the container and sifts the element up: logarithmic in the size. `Emplace` returns nothing, as in `std`.

```cpp
struct ByValue {
    bool operator()(const Ptr<int>& a, const Ptr<int>& b) const { return *a < *b; }
};
PriorityQueue<Ptr<int>, List<Ptr<int>>, ByValue> pq;
pq.Enqueue(Make<int>(3));
pq.Emplace(Make<int>(7));
int top = *pq.Peek();                                  // 7
```

### Dequeue, TryDequeue

```cpp
T Dequeue();
Optional<T> TryDequeue();
```

The largest element, moved out; the heap is restored, logarithmic in the size. `Dequeue` on an empty queue is undefined; `TryDequeue` is `None` then.

### Clear, Swap

```cpp
void Clear() noexcept;
void Swap(PriorityQueue& other) noexcept;
template<class T, class Container, class Compare>
void swap(PriorityQueue<T, Container, Compare>& l, PriorityQueue<T, Container, Compare>& r) noexcept;
```

`Swap` swaps the containers and the comparators; no element is touched. There are no comparisons of priority queues, as in `std`.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// Breadth-first over a graph with a queue, then the vertices by weight with a priority queue
struct Vertex {
    int id;
    int weight;
    List<Ptr<Vertex>> edges;
    bool seen = false;
};

struct Heavier {
    bool operator()(const Ptr<Vertex>& a, const Ptr<Vertex>& b) const {
        return a->weight < b->weight;
    }
};

int main() {
    // A ring of vertices with a chord every fourth: the whole graph is one cycle
    List<Ptr<Vertex>> vertices;
    for (int i : Range(64)) {
        vertices.Add(Make<Vertex>(i, (i * 37) % 64));
    }
    for (int i : Range(64)) {
        vertices[i]->edges.Add(vertices[(i + 1) % 64]);
        if (i % 4 == 0) {
            vertices[i]->edges.Add(vertices[(i + 16) % 64]);
        }
    }
    Ptr start = vertices[0];
    vertices.Clear();                                 // the graph is reachable through `start` only

    // Breadth-first: the queue roots the vertices waiting to be visited
    Queue<Ptr<Vertex>> pending;
    PriorityQueue<Ptr<Vertex>, List<Ptr<Vertex>>, Heavier> byWeight;
    pending.Enqueue(start);
    start->seen = true;
    int visited = 0;
    while (!pending.IsEmpty()) {
        Ptr v = pending.Dequeue();           // the pointer moved out, the vertex lives on behind v
        ++visited;
        byWeight.Enqueue(v);
        for (const auto& w : v->edges) {
            if (!w->seen) {
                w->seen = true;
                pending.Enqueue(w);
            }
        }
    }

    // The heaviest three, in order
    int first = byWeight.Dequeue()->weight;
    int second = byWeight.Dequeue()->weight;
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << visited << " vertices visited, heaviest " << first << " then " << second << ", "
              << byWeight.Count() << " still queued; " << Collector::LiveObjectCount() << " live objects\n";
    return visited == 64 && first == 63 && second == 62 && byWeight.Count() == 62 ? 0 : 1;
}
```

The output:

```
64 vertices visited, heaviest 63 then 62, 62 still queued; 131 live objects
```

## See also

- [Stack](Stack.md) for the LIFO adapter
- [Deque](Deque.md), [List](List.md), [LinkedList](LinkedList.md), the containers the adapters may sit on
- [ConcurrentQueue](../Concurrent/ConcurrentQueue.md) for a queue shared between threads
- [Ptr](../Core/Ptr.md), [Make](../Core/Make.md)
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
