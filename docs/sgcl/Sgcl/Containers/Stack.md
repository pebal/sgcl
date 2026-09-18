# Sgcl::Stack

```cpp
#include "sgcl/Sgcl/Containers/Queue.h"   // or "sgcl/Sgcl/Sgcl.h"

namespace Sgcl {
    template<class T, class Container = Deque<T>>
    class Stack;
}
```

The same class in the `sgcl` interface: [stack](../../containers/stack.md).

`Stack<T, Container>` is `std::stack` over a managed container: a LIFO adapter with `Peek`, `Push`, `Emplace`, `Pop`, `IsEmpty`, `Count`, `Swap` and the comparisons of the container. The container is `Deque<T>` by default; `List<T>` and `LinkedList<T>` work as well, as does any sequence of the interface with a last element and an add and a removal at the back. `Pop` hands the element back, where `std::stack::pop` only removes it. The adapter adds nothing of its own: everything about where the elements live, when they are destroyed and what a push costs is the container's ([Deque](Deque.md), [List](List.md), [LinkedList](LinkedList.md)).

## Rules

- The container holds tracked pointers, so a stack lives where a `Ptr` may: on a thread's stack or inside a managed object, never in `new`/`malloc` memory, a `std` container, a global or a plain coroutine frame ([The rules](../../core/README.md#the-rules), 1).
- An element is destroyed by `Pop()` (after it is moved out) and in the destructor, exactly as with `std::stack` over the same `std` container; the container's memory is the collector's ([Containers](../../containers/README.md#containers)).
- A `Ptr` may not address an element ([The rules](../../core/README.md#the-rules), 4); `Peek()` is a reference, valid as long as the container's last element would be.
- Thread safety is the container's: concurrent readers, or one writer, with the program's own synchronization. A stack shared between threads is a [ConcurrentStack](../Concurrent/ConcurrentStack.md).

## Members

### Types

```cpp
using ValueType = T;
using ContainerType = Container;
using InnerType = sgcl::stack<T, Container::InnerType>;
using SizeType = size_t;
```

### Constructors

```cpp
Stack();
explicit Stack(const Container& c);
explicit Stack(Container&& c);
template<std::input_iterator It> Stack(It first, It last);
Stack(std::initializer_list<T> il);
explicit Stack(InnerType s) noexcept;
```

An empty stack, a stack over a copy of `c` or over `c` itself (moved in), or a stack whose container is built from a range or a list, the first element at the bottom. Copy and move construction and assignment are the container's.

```cpp
Deque d = {1, 2, 3};
Stack<int> fromCopy(d);                              // Peek() is 3, d unchanged
Stack<int> fromMove(std::move(d));                   // d is empty now
List src = {4, 5};
Stack<int> fromRange(begin(src), end(src));        // Peek() is 5
Stack<int, List<int>> onList;               // any managed sequence with Add at the back
```

### Peek, TryPeek

```cpp
T& Peek() noexcept;
const T& Peek() const noexcept;
T* TryPeek() noexcept;
const T* TryPeek() const noexcept;
```

The top element, the last of the container; `Peek` on an empty stack is undefined, `TryPeek` is null then.

### IsEmpty, Count

```cpp
bool IsEmpty() const noexcept;
SizeType Count() const noexcept;
```

### Push, Emplace

```cpp
void Push(const T& value);
void Push(T&& value);
template<class... A> decltype(auto) Emplace(A&&... a);
```

An add at the back of the container; `Emplace` returns what the container's emplace at the back returns, a reference to the new element for the containers of the interface.

```cpp
Stack<Ptr<int>> s;
s.Push(Make<int>(1));
int& two = *s.Emplace(Make<int>(2));      // a reference to the element on top
```

### Pop, TryPop

```cpp
T Pop();
Optional<T> TryPop();
```

The top element, moved out and removed from the container. `Pop` on an empty stack is undefined; `TryPop` is `None` then, C#'s `TryPop` as an `Optional`.

```cpp
Stack<int> s = {1};
Optional<int> top = s.TryPop();                      // 1
Optional<int> none = s.TryPop();                     // None: the stack is empty
```

### Clear

```cpp
void Clear() noexcept;
```

Every element destroyed, the container replaced by an empty one.

### Swap

```cpp
void Swap(Stack& other) noexcept;
template<class T, class Container> void swap(Stack<T, Container>& l, Stack<T, Container>& r) noexcept;
```

Swaps the containers; no element is touched.

### Comparisons

```cpp
bool operator==(const Stack& l, const Stack& r);
auto operator<=>(const Stack& l, const Stack& r);
```

The comparisons of the containers, bottom to top: `<`, `<=`, `>`, `>=` and `!=` follow.

### Inner

```cpp
InnerType& Inner() noexcept;
const InnerType& Inner() const noexcept;
```

The adapter inside, as its own type.

## Example

```cpp
#include "sgcl/Sgcl/Sgcl.h"
#include <iostream>

// A tree walked without recursion: the stack of pending nodes is a root
// for every node it holds, the stack of visited values a plain sequence
struct Node {
    int value;
    Ptr<Node> left, right;
};

Ptr<Node> Build(int depth, int& next) {
    if (depth == 0) {
        return nullptr;
    }
    Ptr node = Make<Node>(next++);
    node->left = Build(depth - 1, next);
    node->right = Build(depth - 1, next);
    return node;
}

int main() {
    int next = 0;
    Ptr root = Build(10, next);             // 1023 nodes

    Stack<Ptr<Node>> pending;      // on the stack: a root for the nodes it holds
    Stack<int, List<int>> visited; // over a List: one contiguous buffer
    pending.Push(root);
    root = nullptr;                                  // the tree is reachable through `pending` only
    while (!pending.IsEmpty()) {
        Ptr node = pending.Pop();           // the pointer moved out, the node lives on behind `node`
        visited.Push(node->value);
        if (node->right) {
            pending.Push(node->right);
        }
        if (node->left) {
            pending.Push(node->left);
        }
    }
    // Every node has been popped: the tree is garbage
    // Optional: the collector runs its cycles by itself; forced here only
    // to show the result at once
    Collector::Collect(true);
    std::cout << visited.Count() << " nodes visited, last value " << visited.Peek() << ", "
              << Collector::LiveObjectCount() << " live objects\n";
    return visited.Count() == 1023 && visited.Peek() == 1022 ? 0 : 1;
}
```

The output:

```
1023 nodes visited, last value 1022, 2 live objects
```

## See also

- [Queue](Queue.md) for the FIFO adapter and `PriorityQueue`
- [Deque](Deque.md), [List](List.md), [LinkedList](LinkedList.md), the containers a stack may adapt
- [Ptr](../Core/Ptr.md), [ConcurrentStack](../Concurrent/ConcurrentStack.md) for a lock-free stack shared between threads
- [README: Containers](../../containers/README.md#containers), [README: The rules](../../core/README.md#the-rules)
