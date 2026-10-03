[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::swap (sgcl::queue)

```cpp
template<class T, class Container>
void swap(queue<T, Container>& lhs, queue<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

Exchanges the contents of `lhs` and `rhs`: `lhs.swap(rhs)`. Found by the arguments' type, so a generic
`using std::swap; swap(a, b);` calls it.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the queues to exchange the contents of |

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
    queue<string> today, tomorrow;
    today.push("write");
    tomorrow.push("review");
    tomorrow.push("release");

    using std::swap;
    swap(today, tomorrow);
    println("{} {}, {} {}", today.size(), today.front(), tomorrow.size(), tomorrow.front());
}
```

Output:

```text
2 review, 1 write
```

## See also

- [swap](swap.md): the member form
- [sgcl::queue\<T, Container\>](../queue.md)
