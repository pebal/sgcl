[sgcl](../../README.md) › [core](../README.md) › [priority_queue](../priority_queue.md)

# sgcl::priority_queue\<T, Container, Compare\>::empty

```cpp
bool empty() const noexcept(noexcept(c.empty()));
```

Checks whether the priority queue has no elements: `c.empty()`.

## Parameters

None.

## Return value

`true` when the priority queue has no elements, `false` otherwise.

## Complexity

Constant.

## Exceptions

What the container's `empty` throws; none for `vector` and `deque`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    priority_queue<int> pq(std::less<int>(), vector{2, 7, 5});
    while (!pq.empty()) {
        println("{}", pq.top());
        pq.pop();
    }
}
```

Output:

```text
7
5
2
```

## See also

- [size](size.md): the number of elements
- [sgcl::priority_queue\<T, Container, Compare\>](../priority_queue.md)
