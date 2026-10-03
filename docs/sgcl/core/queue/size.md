[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::queue\<T, Container\>::size

```cpp
size_type size() const noexcept(noexcept(c.size()));
```

Returns the number of elements: `c.size()`.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant.

## Exceptions

What the container's `size` throws; none for `deque` and `list`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<int, list<int>> q;
    for (int i : range(5)) {
        q.push(i);
    }
    q.pop();
    println("{}", q.size());
}
```

Output:

```text
4
```

## See also

- [empty](empty.md): checks whether the queue is empty
- [sgcl::queue\<T, Container\>](../queue.md)
