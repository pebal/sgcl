[sgcl](../../README.md) › [core](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::priority_queue\<T, Container, Compare\>::top

```cpp
const_reference top() const noexcept(noexcept(c.front()));
```

Returns a reference to the largest element under `Compare`, the one the next [pop](pop.md) removes:
`c.front()`, the root of the heap. The priority queue must not be empty.

## Parameters

None.

## Return value

A const reference to the largest element.

## Complexity

Constant.

## Exceptions

What the container's `front` throws; none for `vector` and `deque`.

## Notes

`top` on an empty priority queue is undefined behaviour, as with `std::priority_queue`. The reference is const:
a change of the element would break the heap. It is valid as long as the container's would be, until the next
push or pop. Among elements equal under `Compare`, which one is on top is unspecified.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <functional>

using namespace sgcl;

int main() {
    priority_queue<int> largest;
    priority_queue<int, vector<int>, std::greater<int>> smallest;
    for (int x : {4, 9, 2, 7}) {
        largest.push(x);
        smallest.push(x);
    }
    println("{} {}", largest.top(), smallest.top());
}
```

Output:

```text
9 2
```

## See also

- [pop](pop.md): removes the largest element
- [sgcl::priority_queue\<T, Container, Compare\>](../priority_queue.md)
