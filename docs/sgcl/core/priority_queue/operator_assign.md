[sgcl](../../README.md) › [core](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::priority_queue\<T, Container, Compare\>::operator=

```cpp
/*(1)*/ priority_queue& operator=(const priority_queue& other);
/*(2)*/ priority_queue& operator=(priority_queue&& other);
```

Replaces the contents of the priority queue.

1. Copy assignment: the container and the comparator of `other` are copied into this one.
2. Move assignment: the container and the comparator of `other` are moved into this one.

Both are the implicit ones: noexcept as the copy and the move assignment of the container and the comparator
are, the move noexcept for `vector` and `deque` with the function objects of `std`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the priority queue whose contents are taken |

## Return value

`*this`.

## Complexity

- (1) Linear in the sizes of both priority queues.
- (2) Linear in the size of this priority queue for `vector` and `deque`, whose move assignment destroys the
  elements held before and takes the other's memory over.

## Exceptions

- (1) What the copy of `T` and of `Compare` throws.
- (2) What the move assignment of `Container` and of `Compare` throws; none for `vector` and `deque` with the
  function objects of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    priority_queue<int> a(std::less<int>(), vector{3, 8, 5});
    priority_queue<int> b;
    b = a;
    b.pop();
    println("{} {}", a.top(), b.top());

    priority_queue<int> c;
    c = std::move(a);
    println("{} {}", c.size(), a.size());
}
```

Output:

```text
8 5
3 0
```

## See also

- [(constructor)](priority_queue.md): constructs the priority queue
- [swap](swap.md): swaps the contents
- [sgcl::priority_queue\<T, Container, Compare\>](../priority_queue.md)
