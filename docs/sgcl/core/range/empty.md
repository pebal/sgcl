[sgcl](../../README.md) › [core](../README.md) › [range](../range.md)

# sgcl::range\<It\>::empty

```cpp
bool empty() const noexcept;
```

Checks whether the range has no elements: `begin() == end()`.

## Parameters

None.

## Return value

`true` when the range is empty, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    sorted_multimap<string, int> m = {{"a", 1}};
    println("{} {}", range(m.equal_range("a")).empty(), range(m.equal_range("z")).empty());
    println("{} {}", range(0).empty(), range(3, 3).empty());
}
```

Output:

```text
false true
true true
```

## See also

- [size](size.md): the number of elements
- [sgcl::range\<It\>](../range.md)
