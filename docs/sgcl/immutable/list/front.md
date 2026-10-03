[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::front

```cpp
const_reference front() const noexcept;
```

Returns a reference to the first element, the element of the first cell.

## Parameters

None.

## Return value

A `const` reference to the first element.

## Complexity

Constant.

## Exceptions

None. `front` on an empty list is undefined; debug builds assert.

## Notes

The reference is valid while some list holds the first cell: this one, or any list that shares it.

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<string> path = {"usr", "local"};
    auto deeper = path.push_front("bin");
    println("{} {}", path.front(), deeper.front());
}
```

Output:

```text
usr bin
```

## See also

- [pop_front](pop_front.md): the list without its first element
- [begin, cbegin](begin.md): an iterator to the beginning
- [sgcl::immutable::list\<T\>](../list.md)
