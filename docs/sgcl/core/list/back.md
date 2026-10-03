[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::back

```cpp
reference back() noexcept;                // (1)
const_reference back() const noexcept;    // (2)
```

Returns a reference to the last element. The list must not be empty: on an empty list the call is undefined, and
debug builds assert.

## Parameters

None.

## Return value

A reference to the last element.

## Complexity

Constant: the sentinel links the last node.

## Exceptions

None.

## Notes

The reference stays valid until the element is erased; a `push_back` after it adds a new last element and leaves
the old one where it is.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list l = {1, 2, 3};
    int& last = l.back();
    l.push_back(4);
    last *= 10;
    println("{}, back {}", l, l.back());
}
```

Output:

```text
[1, 2, 30, 4], back 4
```

## See also

- [front](front.md): access the first element
- [push_back](push_back.md), [pop_back](pop_back.md): add, remove the last element
- [sgcl::list\<T\>](README.md)
