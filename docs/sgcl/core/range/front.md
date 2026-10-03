[sgcl](../../README.md) › [core](../README.md) › [range](../range.md)

# sgcl::range\<It\>::front

```cpp
decltype(auto) front() const noexcept;
```

The first element, `*begin()`: a reference into the container for a container's iterator, a value for the counting
iterator. Precondition: the range is not empty; `front()` on an empty range is undefined, as `*begin()` is.

## Parameters

None.

## Return value

What `*begin()` gives.

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
    sorted_multimap<int, string> m = {{1, "one"}, {2, "two"}, {2, "deux"}};
    auto twos = range(m.equal_range(2));
    println("{} {}", twos.front().second, range(5, 9).front());
}
```

Output:

```text
two 5
```

## See also

- [begin](begin.md): the iterator to the first element
- [sgcl::range\<It\>](../range.md)
