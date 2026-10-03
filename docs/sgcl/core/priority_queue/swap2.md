[sgcl](../../README.md) › [core](../README.md) › [priority_queue](README.md)

# sgcl::swap (sgcl::priority_queue)

```cpp
template<class T, class Container, class Compare>
void swap(priority_queue<T, Container, Compare>& lhs, priority_queue<T, Container, Compare>& rhs)
    noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the contents of `lhs` and `rhs`: `lhs.swap(rhs)`. Found by the arguments' type, so a generic
`using std::swap; swap(a, b);` calls it.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the priority queues to exchange the contents of |

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
#include <functional>

using namespace sgcl;

int main() {
    using MinHeap = priority_queue<int, vector<int>, std::greater<int>>;
    MinHeap a(std::greater<int>(), vector{4, 6});
    MinHeap b(std::greater<int>(), vector{8, 2, 5});

    using std::swap;
    swap(a, b);
    println("{} {}, {} {}", a.size(), a.top(), b.size(), b.top());
}
```

Output:

```text
3 2, 2 4
```

## See also

- [swap](swap.md): the member form
- [sgcl::priority_queue\<T, Container, Compare\>](README.md)
