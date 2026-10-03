[sgcl](../../README.md) › [core](../README.md) › [range](README.md)

# sgcl::range\<It\>::end

```cpp
It end() const noexcept;
```

The iterator past the last element, the one the range was made with.

## Parameters

None.

## Return value

The iterator to the end.

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
    sorted_multimap<int, string> m = {{1, "one"}, {2, "two"}, {3, "three"}};
    range ones = m.equal_range(1);
    println("{}", ones.end()->second);  // the end of the range is the next element of the map
}
```

Output:

```text
two
```

## See also

- [begin](begin.md): the iterator to the beginning
- [sgcl::range\<It\>](README.md)
