[sgcl](../../README.md) › [core](../README.md) › [range](README.md)

# sgcl::range\<It\>::begin

```cpp
It begin() const noexcept;
```

The iterator to the first element, the one the range was made with. It is the container's iterator and stays valid
as long as the container says: the range holds a copy of it and nothing else.

## Parameters

None.

## Return value

The iterator to the beginning.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <algorithm>
#include <ranges>

using namespace sgcl;

int main() {
    range r(3, 8);
    println("{}", *r.begin());
    println("{}", *std::ranges::max_element(range(3, 8)));  // borrowed: the iterator outlives it
}
```

Output:

```text
3
7
```

## See also

- [end](end.md): the iterator to the end
- [front](front.md): the first element
- [sgcl::range\<It\>](README.md)
