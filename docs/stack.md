# sgcl::stack

```cpp
#include "sgcl/stack.h"   // or "sgcl/sgcl.h"

namespace sgcl {
    template<class T, class Container = deque<T>>
    class stack;
}
```

`sgcl::stack<T, Container>` is `std::stack` over a managed container: a LIFO adapter with `top`, `push`, `emplace`, `pop`, `empty`, `size`, `swap` and the comparisons of the container. The container is `sgcl::deque<T>` by default; `sgcl::vector<T>` and `sgcl::list<T>` work as well, as does any container with `back`, `push_back`, `emplace_back` and `pop_back`. The adapter adds nothing of its own: the container is the protected member `c`, as in `std`, and everything about where the elements live, when they are destroyed and what a push costs is the container's ([deque](deque.md), [vector](vector.md), [list](list.md)).

## Rules

- The container holds tracked pointers, so a stack lives where a `tracked_ptr` may: on a thread's stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../README.md#the-rules), 1).
- An element is destroyed by `pop()` and in the destructor, exactly as with `std::stack` over the same `std` container; the container's memory is the collector's ([Containers](../README.md#containers)).
- A `tracked_ptr` may not address an element ([The rules](../README.md#the-rules), 4); `top()` is a reference, valid as long as the container's `back()` would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization. A lock-free stack shared between threads is a different structure, built from `sgcl::atomic` ([examples/lock_free_stack.cpp](../examples/lock_free_stack.cpp)).

## Members

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
stack();
explicit stack(const Container& cont);
explicit stack(Container&& cont);
template<std::input_iterator InputIt> stack(InputIt first, InputIt last);
```

An empty stack, a stack over a copy of `cont` or over `cont` itself (moved in), or a stack whose container is built from a range, the first element at the bottom. Copy and move construction and assignment are the implicit ones, so they are those of the container.

```cpp
gc::deque<int> d = {1, 2, 3};
gc::stack<int> from_copy(d);                          // top() is 3, d unchanged
gc::stack<int> from_move(std::move(d));               // d is empty now
std::vector<int> src = {4, 5};
gc::stack<int> from_range(src.begin(), src.end());    // top() is 5
gc::stack<int, gc::vector<int>> on_vector;            // any managed sequence with push_back
```

### top

```cpp
reference top();
const_reference top() const;
```

The last element pushed, `c.back()`; the stack must not be empty.

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
gc::stack<gc::tracked_ptr<int>> s;
s.push(gc::make_tracked<int>(1));
int& two = *s.emplace(gc::make_tracked<int>(2));      // a reference to the element on top
```

### pop

```cpp
void pop();
```

`c.pop_back()`: destroys the top element. The stack must not be empty.

### swap

```cpp
void swap(stack& other) noexcept(std::is_nothrow_swappable_v<Container>);
template<class T, class Container> void swap(stack<T, Container>& lhs, stack<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Swaps the containers; no element is touched.

### Comparisons

```cpp
friend bool operator==(const stack& lhs, const stack& rhs);
friend auto operator<=>(const stack& lhs, const stack& rhs);
```

The comparisons of the containers, bottom to top: `<`, `<=`, `>`, `>=` and `!=` follow.

```cpp
gc::deque<int> d = {1, 2};
gc::stack<int> a(d), b(d);
b.push(3);
bool less = a < b;                  // true: a prefix
```

## Example

```cpp
#include "sgcl/sgcl.h"
#include <iostream>

// A tree walked without recursion: the stack of pending nodes is a root
// for every node it holds, the stack of visited values a plain sequence
struct Node {
    int value;
    gc::tracked_ptr<Node> left, right;
};

gc::tracked_ptr<Node> build(int depth, int& next) {
    if (depth == 0) {
        return nullptr;
    }
    gc::tracked_ptr node = gc::make_tracked<Node>(next++);
    node->left = build(depth - 1, next);
    node->right = build(depth - 1, next);
    return node;
}

int main() {
    int next = 0;
    gc::tracked_ptr root = build(10, next);           // 1023 nodes

    gc::stack<gc::tracked_ptr<Node>> pending;        // on the stack: a root for the nodes it holds
    gc::stack<int, gc::vector<int>> visited;         // over a vector: one contiguous buffer
    pending.push(root);
    root = nullptr;                                  // the tree is reachable through `pending` only
    while (!pending.empty()) {
        gc::tracked_ptr node = pending.top();
        pending.pop();                               // the pointer is destroyed, the node lives on behind `node`
        visited.push(node->value);
        if (node->right) {
            pending.push(node->right);
        }
        if (node->left) {
            pending.push(node->left);
        }
    }
    // Every node has been popped: the tree is garbage
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    gc::collector::force_collect(true);
    std::cout << visited.size() << " nodes visited, last value " << visited.top() << ", "
              << gc::collector::get_live_object_count() << " live objects\n";
    return visited.size() == 1023 && visited.top() == 1022 ? 0 : 1;
}
```

## See also

- [queue](queue.md) for the FIFO adapter and `priority_queue`
- [deque](deque.md), [vector](vector.md), [list](list.md), the containers a stack may adapt
- [tracked_ptr](tracked_ptr.md), [atomic](atomic.md) for a lock-free stack shared between threads
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
