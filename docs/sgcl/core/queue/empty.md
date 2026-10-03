[sgcl](../../README.md) › [core](../README.md) › [queue](../queue.md)

# sgcl::queue\<T, Container\>::empty

```cpp
bool empty() const noexcept(noexcept(c.empty()));
```

Checks whether the queue has no elements: `c.empty()`.

## Parameters

None.

## Return value

`true` when the queue has no elements, `false` otherwise.

## Complexity

Constant.

## Exceptions

What the container's `empty` throws; none for `deque` and `list`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<int> q;
    println("{}", q.empty());
    q.push(1);
    println("{}", q.empty());
    q.pop();
    println("{}", q.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::queue\<T, Container\>](../queue.md)
