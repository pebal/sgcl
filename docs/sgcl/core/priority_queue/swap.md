[sgcl](../../README.md) › [core](../README.md) › [priority_queue](README.md)

# sgcl::priority_queue\<T, Container, Compare\>::swap

```cpp
void swap(priority_queue& other)
    noexcept(std::is_nothrow_swappable_v<Container> && std::is_nothrow_swappable_v<Compare>);
```

Exchanges the contents of the priority queue with those of `other`: swaps the containers and the comparators,
and no element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the priority queue to exchange the contents with |

## Return value

None.

## Complexity

The swap of the containers and of the comparators: constant for `vector` and `deque`.

## Exceptions

What the swap of `Container` or of `Compare` throws; none for `vector` and `deque` with the function objects
of `std`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    priority_queue<int> a(std::less<int>(), vector{1, 2, 3});
    priority_queue<int> b(std::less<int>(), vector{9});
    a.swap(b);
    println("{} {}, {} {}", a.size(), a.top(), b.size(), b.top());
}
```

Output:

```text
1 9, 3 3
```

## See also

- [swap](swap2.md): the non-member form
- [operator=](operator_assign.md): assigns the contents
- [sgcl::priority_queue\<T, Container, Compare\>](README.md)
