[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::queue\<T, Container\>::operator=

```cpp
/*(1)*/ queue& operator=(const queue& other);
/*(2)*/ queue& operator=(queue&& other);
```

Replaces the contents of the queue.

1. Copy assignment: the container of `other` is copied into this one.
2. Move assignment: the container of `other` is moved into this one.

Both are the implicit ones, so they are those of the container: noexcept as its copy and its move assignment
are, the move noexcept for `deque` and `list`.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the queue whose contents are taken |

## Return value

`*this`.

## Complexity

- (1) Linear in the sizes of both queues.
- (2) Linear in the size of this queue for `deque` and `list`, whose move assignment destroys the elements held
  before and takes the other's memory over.

## Exceptions

- (1) What the copy of `T` throws.
- (2) What the move assignment of `Container` throws; none for `deque` and `list`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<string> a;
    a.push("x");
    a.push("y");
    queue<string> b;
    b = a;
    b.pop();
    println("{} {}", a.front(), b.front());

    queue<string> c;
    c = std::move(a);
    println("{} {}", c.size(), a.size());
}
```

Output:

```text
x y
2 0
```

## See also

- [(constructor)](queue.md): constructs the queue
- [swap](swap.md): swaps the contents
- [sgcl::queue\<T, Container\>](../queue.md)
