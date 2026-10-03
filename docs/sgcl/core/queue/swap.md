[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::queue\<T, Container\>::swap

```cpp
void swap(queue& other) noexcept(std::is_nothrow_swappable_v<Container>);
```

Exchanges the contents of the queue with those of `other`: swaps the containers, and no element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the queue to exchange the contents with |

## Return value

None.

## Complexity

The swap of the containers: constant for `deque` and `list`.

## Exceptions

What the swap of `Container` throws; none for `deque` and `list`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<int> a(deque{1, 2, 3});
    queue<int> b(deque{9});
    a.swap(b);
    println("{} {}, {} {}", a.size(), a.front(), b.size(), b.front());
}
```

Output:

```text
1 9, 3 1
```

## See also

- [swap](swap2.md): the non-member form
- [operator=](operator_assign.md): assigns the contents
- [sgcl::queue\<T, Container\>](../queue.md)
