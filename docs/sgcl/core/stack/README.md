[sgcl](../../README.md) › [core](../README.md)

# sgcl::stack\<T, Container\>

```cpp
#include "sgcl/core/stack.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class Container = deque<T>>
    class stack;
}
```

**Requires [rooted](../rooted/README.md) outside a stack or a managed object.**

`sgcl::stack<T, Container>` is `std::stack` over a managed container: a LIFO adapter with `top`, `push`,
`emplace`, `pop`, `empty`, `size`, `swap` and the comparisons of the container. The container is
`sgcl::deque<T>` by default; `sgcl::vector<T>` and `sgcl::list<T>` work as well, as does any container with
`back`, `push_back`, `emplace_back` and `pop_back`. The adapter adds nothing of its own: the container is the
protected member `c`, the bottom element first, reached by a class derived from the stack as in `std`, and
everything about where the elements live, when they are destroyed and what a push costs is the container's ([deque](../deque/README.md), [vector](../vector/README.md), [list](../list/README.md)).

## Rules

- An element is destroyed by `pop()` and in the destructor, exactly as with `std::stack` over the same `std`
  container; the container's memory is the collector's ([Containers](../README.md#containers)).
- A `tracked_ptr` may not address an element ([The rules](../README.md#the-rules), 4); `top()` is a reference,
  valid as long as the container's `back()` would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization. A
  stack shared between threads is a different structure, lock-free: [concurrent::stack](../../concurrent/stack/README.md),
  or one built from `sgcl::atomic`
  ([benchmarks/concurrent/lockfree_stack.cpp](../../../../benchmarks/concurrent/lockfree_stack.cpp)).

## Template parameters

| Parameter | Description |
|---|---|
| `T` | The type of the elements, the `value_type` of `Container`. |
| `Container` | The container that holds the elements, `deque<T>` by default: a sequence with `back`, `push_back`, `emplace_back`, `pop_back`, `empty` and `size`, whose `value_type` is `T`. The comparisons of the stack may be used only when the container's `==` and `<=>` are there. |

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
| [(constructor)](stack.md) | constructs the stack |
| `(destructor)` | destroys the container; implicitly declared |
| `operator=` | assigns the container; implicitly declared |

#### Element access

| Function | Description |
|---|---|
| [top](top.md) | access the top element |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | checks whether the stack is empty |
| [size](size.md) | the number of elements |

#### Modifiers

| Function | Description |
|---|---|
| [push](push.md) | inserts an element at the top |
| [emplace](emplace.md) | constructs an element in place at the top |
| [pop](pop.md) | removes the top element |
| [swap](swap.md) | swaps the contents |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator\<=\>](operator_cmp.md) | compare the containers of two stacks |
| [swap](swap2.md) | swaps the contents of two stacks |

## Deduction guides

```cpp
template<class Container>
stack(Container) -> stack<typename Container::value_type, Container>;
template<class InputIt>
stack(InputIt, InputIt) -> stack<std::iter_value_t<InputIt>>;
```

As for `std::stack`: `stack(v)` of a `vector<int>` is a `stack<int, vector<int>>`, and a stack from a range of
iterators is over the default `deque`.

## Complexity

Every operation is one call of the container's: `push` and `emplace` are a `push_back` and an `emplace_back`,
`pop` a `pop_back`, `top` a `back`, and cost what they cost there.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

// A tree walked without recursion: the stack of pending nodes is a root
// for every node it holds, the stack of visited values a plain sequence
struct Node {
    int value;
    tracked_ptr<Node> left, right;
};

tracked_ptr<Node> build(int depth, int& next) {
    if (depth == 0) {
        return nullptr;
    }
    tracked_ptr node = make_tracked<Node>(next++);
    node->left = build(depth - 1, next);
    node->right = build(depth - 1, next);
    return node;
}

int main() {
    println("a tree walked depth first");
    // counted after the first line: io's own objects are not the example's
    auto base = collector::get_live_object_count();
    int next = 0;
    tracked_ptr root = build(10, next);  // 1023 nodes

    stack<tracked_ptr<Node>> pending;  // on the stack: a root for the nodes it holds
    stack<int, vector<int>> visited;  // over a vector: one contiguous buffer
    pending.push(root);
    root = nullptr;  // the tree is reachable through pending only
    while (!pending.empty()) {
        tracked_ptr node = pending.top();
        pending.pop();  // the pointer is destroyed, the node lives on behind node
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
    collector::force_collect(true);
    auto live = collector::get_live_object_count() - base;
    println("{} nodes visited, last value {}, {} live objects", visited.size(), visited.top(), live);
}
```

Output:

```text
a tree walked depth first
1023 nodes visited, last value 1022, 2 live objects
```

## See also

- [queue](../queue/README.md), [priority_queue](../priority_queue/README.md): the FIFO adapter, the adapter that gives the largest element first
- [deque](../deque/README.md), [vector](../vector/README.md), [list](../list/README.md): the containers a stack may adapt
- [concurrent::stack](../../concurrent/stack/README.md): a lock-free stack shared between threads
- [tracked_ptr](../tracked_ptr/README.md), [atomic](../atomic.md)
- [README: Containers](../README.md#containers), [README: The rules](../README.md#the-rules)
