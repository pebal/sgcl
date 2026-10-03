[sgcl](../../README.md) › [core](../README.md) › [queue](README.md)

# sgcl::queue\<T, Container\>::back

```cpp
reference back() noexcept(noexcept(c.back()));                // (1)
const_reference back() const noexcept(noexcept(c.back()));    // (2)
```

Returns a reference to the last element, the newest one, the last [push](push.md) appended: `c.back()`. The
queue must not be empty.

## Parameters

None.

## Return value

A reference to the last element.

## Complexity

Constant.

## Exceptions

What the container's `back` throws; none for `deque` and `list`.

## Notes

`back` on an empty queue is undefined behaviour, as with `std::queue`. The reference is valid as long as the
container's would be: for `deque` and `list`, until the element is popped or the queue is destroyed.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<int> q;
    for (int i : range(1, 4)) {
        q.push(i * 10);
        println("front {}, back {}", q.front(), q.back());
    }
    q.back() += 5;
    println("back {}", q.back());
}
```

Output:

```text
front 10, back 10
front 10, back 20
front 10, back 30
back 35
```

## See also

- [front](front.md): access the first element
- [push](push.md): appends an element at the end
- [sgcl::queue\<T, Container\>](README.md)
