# sgcl::queue, sgcl::priority_queue

```cpp
#include "sgcl/containers/queue.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, class Container = deque<T>>
    class queue;

    template<class T, class Container = vector<T>, class Compare = std::less<typename Container::value_type>>
    class priority_queue;
}
```

`sgcl::queue<T, Container>` is `std::queue` over a managed container: a FIFO adapter with `front`, `back`, `push`, `emplace`, `pop`, `empty`, `size`, `swap` and the comparisons of the container. The container is `sgcl::deque<T>` by default; `sgcl::list<T>` works as well, as does any container with `front`, `back`, `push_back`, `emplace_back` and `pop_front`.

`sgcl::priority_queue<T, Container, Compare>` is `std::priority_queue` over a managed container: a heap kept with `std::push_heap`/`std::pop_heap`, `top()` the largest element under `Compare`. The container is `sgcl::vector<T>` by default; `sgcl::deque<T>` works as well, as does any container with random-access iterators, `front`, `push_back`, `emplace_back` and `pop_back`.

The adapters add nothing of their own: the container is the protected member `c` (and the comparator `comp`), as in `std`, and everything about where the elements live, when they are destroyed and what a push costs is the container's ([deque](deque.md), [vector](vector.md), [list](list.md)).

## Rules

- The container holds tracked pointers, so an adapter lives where a `tracked_ptr` may: on a thread's stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../core/README.md#the-rules), 1).
- An element is destroyed by `pop()` and in the destructor, exactly as with the `std` adapter over the same `std` container; the container's memory is the collector's ([Containers](README.md#containers)).
- A `tracked_ptr` may not address an element ([The rules](../core/README.md#the-rules), 4); `front()`, `back()` and `top()` are references, valid as long as the container's would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization.

## queue

### Types

```cpp
using container_type = Container;
using value_type = typename Container::value_type;
using size_type = typename Container::size_type;
using reference = typename Container::reference;
using const_reference = typename Container::const_reference;
```

### Constructors

```cpp
queue();
explicit queue(const Container& cont);
explicit queue(Container&& cont);
template<std::input_iterator InputIt> queue(InputIt first, InputIt last);
```

An empty queue, a queue over a copy of `cont` or over `cont` itself (moved in), or a queue whose container is built from a range, the first element at the front. Copy and move construction and assignment are the implicit ones, so they are those of the container.

```cpp
deque d = {1, 2, 3};
queue<int> from_copy(d);                          // front() is 1, d unchanged
queue<int> from_move(std::move(d));               // d is empty now
vector src = {4, 5};
queue<int> from_range(src.begin(), src.end());    // front() is 4, back() is 5
queue<int, list<int>> on_list;                // any managed sequence with push_back and pop_front
```

### front, back

```cpp
reference front();
const_reference front() const;
reference back();
const_reference back() const;
```

The oldest and the newest element, `c.front()` and `c.back()`; the queue must not be empty.

### empty, size

```cpp
bool empty() const;
size_type size() const;
```

### push, emplace

```cpp
void push(const value_type& value);
void push(value_type&& value);
template<class... A> decltype(auto) emplace(A&&... a);
```

`c.push_back(value)` and `c.emplace_back(a...)`; `emplace` returns what the container's `emplace_back` returns, a reference to the new element for the SGCL containers.

```cpp
queue<tracked_ptr<int>> q;
q.push(make_tracked<int>(1));
int& two = *q.emplace(make_tracked<int>(2));      // a reference to the element at the back
```

### pop

```cpp
void pop();
```

`c.pop_front()`: destroys the front element. The queue must not be empty.

### swap

```cpp
void swap(queue& other) noexcept(std::is_nothrow_swappable_v<Container>);
template<class T, class Container> void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Swaps the containers; no element is touched.

### Comparisons

```cpp
friend bool operator==(const queue& lhs, const queue& rhs);
friend auto operator<=>(const queue& lhs, const queue& rhs);
```

The comparisons of the containers, front to back: `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
deque d = {1, 2};
queue<int> a(d), b(d);
b.push(3);
bool less = a < b;                  // true: a prefix
```

## priority_queue

### Types

```cpp
using container_type = Container;
using value_compare = Compare;
using value_type = typename Container::value_type;
using size_type = typename Container::size_type;
using reference = typename Container::reference;
using const_reference = typename Container::const_reference;
```

### Constructors

```cpp
priority_queue();
explicit priority_queue(const Compare& compare);
priority_queue(const Compare& compare, const Container& cont);
priority_queue(const Compare& compare, Container&& cont);
template<std::input_iterator InputIt> priority_queue(InputIt first, InputIt last, const Compare& compare = Compare());
template<std::input_iterator InputIt> priority_queue(InputIt first, InputIt last, const Compare& compare, const Container& cont);
template<std::input_iterator InputIt> priority_queue(InputIt first, InputIt last, const Compare& compare, Container&& cont);
```

An empty queue, or one over a copy of `cont` or over `cont` itself (moved in), with the elements of the range appended when one is given; the container is then made a heap under `compare`. Copy and move construction and assignment are the implicit ones.

```cpp
vector values = {5, 1, 4, 1, 3};
priority_queue<int> max_heap(values.begin(), values.end());                        // top() is 5
priority_queue<int, vector<int>, std::greater<int>> min_heap(values.begin(), values.end());       // top() is 1
priority_queue<int, deque<int>> on_deque(std::less<int>(), deque<int>{2, 9, 4});            // top() is 9
```

### top

```cpp
const_reference top() const;
```

The largest element under `Compare`, `c.front()`; the queue must not be empty.

### empty, size

```cpp
bool empty() const;
size_type size() const;
```

### push, emplace

```cpp
void push(const value_type& value);
void push(value_type&& value);
template<class... A> void emplace(A&&... a);
```

Appends to the container and sifts the element up: logarithmic in the size. `emplace` returns nothing, as in `std`.

```cpp
struct ByValue {
    bool operator()(const tracked_ptr<int>& a, const tracked_ptr<int>& b) const { return *a < *b; }
};
priority_queue<tracked_ptr<int>, vector<tracked_ptr<int>>, ByValue> pq;
pq.push(make_tracked<int>(3));
pq.emplace(make_tracked<int>(7));
int top = *pq.top();                                  // 7
```

### pop

```cpp
void pop();
```

Moves the top element to the back of the container and destroys it there; logarithmic in the size. The queue must not be empty.

### swap

```cpp
void swap(priority_queue& other) noexcept(std::is_nothrow_swappable_v<Container> && std::is_nothrow_swappable_v<Compare>);
template<class T, class Container, class Compare>
void swap(priority_queue<T, Container, Compare>& lhs, priority_queue<T, Container, Compare>& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Swaps the containers and the comparators; no element is touched. There are no comparisons of priority queues, as in `std`.

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

using namespace sgcl;

// Breadth-first over a graph with a queue, then the vertices by weight with a priority queue
struct Vertex {
    int id;
    int weight;
    vector<tracked_ptr<Vertex>> edges;
    bool seen = false;
};

struct Heavier {
    bool operator()(const tracked_ptr<Vertex>& a, const tracked_ptr<Vertex>& b) const {
        return a->weight < b->weight;
    }
};

int main() {
    // A ring of vertices with a chord every fourth: the whole graph is one cycle
    vector<tracked_ptr<Vertex>> vertices;
    for (int i : range(64)) {
        vertices.push_back(make_tracked<Vertex>(i, (i * 37) % 64));
    }
    for (int i : range(64)) {
        vertices[i]->edges.push_back(vertices[(i + 1) % 64]);
        if (i % 4 == 0) {
            vertices[i]->edges.push_back(vertices[(i + 16) % 64]);
        }
    }
    tracked_ptr start = vertices[0];
    vertices.clear();                                 // the graph is reachable through `start` only

    // Breadth-first: the queue roots the vertices waiting to be visited
    queue<tracked_ptr<Vertex>> pending;
    priority_queue<tracked_ptr<Vertex>, vector<tracked_ptr<Vertex>>, Heavier> by_weight;
    pending.push(start);
    start->seen = true;
    int visited = 0;
    while (!pending.empty()) {
        tracked_ptr v = pending.front();
        pending.pop();                                // the pointer is destroyed, the vertex lives on
        ++visited;
        by_weight.push(v);
        for (const auto& w : v->edges) {
            if (!w->seen) {
                w->seen = true;
                pending.push(w);
            }
        }
    }

    // The heaviest three, in order
    int first = by_weight.top()->weight;
    by_weight.pop();
    int second = by_weight.top()->weight;
    by_weight.pop();
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    collector::force_collect(true);
    std::cout << visited << " vertices visited, heaviest " << first << " then " << second << ", "
              << by_weight.size() << " still queued; " << collector::get_live_object_count() << " live objects\n";
    return visited == 64 && first == 63 && second == 62 && by_weight.size() == 62 ? 0 : 1;
}
```

The output:

```
64 vertices visited, heaviest 63 then 62, 62 still queued; 131 live objects
```

## See also

- [stack](stack.md) for the LIFO adapter
- [deque](deque.md), [vector](vector.md), [list](list.md), the containers the adapters may sit on
- [tracked_ptr](../core/tracked_ptr.md), [make_tracked](../core/make_tracked.md)
- [README: Containers](README.md#containers), [README: The rules](../core/README.md#the-rules)
