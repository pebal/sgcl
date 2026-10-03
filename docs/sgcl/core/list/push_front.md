[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::push_front

```cpp
/*(1)*/ void push_front(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(2)*/ void push_front(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);
```

Inserts an element at the beginning, in a node of its own.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

The element is constructed in its node before the node is linked, so `value` may be an element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to insert |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the copy (1) or the move (2) constructor of `T` throws; none when it is noexcept. If an exception is thrown,
the list is as it was before the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list<int> l;
    for (int i : range(4)) {
        l.push_front(i);
    }
    l.push_front(l.back());  // an element of the list itself
    println("{}", l);
}
```

Output:

```text
[0, 3, 2, 1, 0]
```

## See also

- [emplace_front](emplace_front.md): constructs an element in place at the beginning
- [pop_front](pop_front.md): removes the first element
- [push_back](push_back.md): appends an element at the end
- [sgcl::list\<T\>](../list.md)
