[sgcl](../../README.md) › [core](../README.md) › [forward_list](README.md)

# sgcl::forward_list\<T\>::front

```cpp
reference front() noexcept;                // (1)
const_reference front() const noexcept;    // (2)
```

Returns a reference to the first element. The list must not be empty: on an empty list the call is undefined, and
debug builds assert.

## Parameters

None.

## Return value

A reference to the first element.

## Complexity

Constant.

## Exceptions

None.

## Notes

The reference stays valid until the element is erased, whatever else the list does: an insertion, a
`splice_after`, a `sort` or a `reverse` moves nodes, never elements. It does not keep the node alive; the list does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list l = {3, 1, 2};
    int& first = l.front();
    first = 30;

    l.push_front(0);
    l.sort();
    println("{}, front {}, the old first {}", l, l.front(), first);
}
```

Output:

```text
[0, 1, 2, 30], front 0, the old first 30
```

## See also

- [begin](begin.md): an iterator to the first element
- [push_front](push_front.md), [pop_front](pop_front.md): add, remove the first element
- [sgcl::forward_list\<T\>](README.md)
