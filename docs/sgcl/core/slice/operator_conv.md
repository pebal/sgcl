[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::operator std::span

```cpp
operator std::span<T>() const noexcept;
```

The elements as a `std::span<T>`, without the owner: for a `std` interface, or for a place a slice may not live (a
`std` container, a global, `new` memory). The span keeps nothing alive; it is valid while the slice, or another
holder of the owner, lives.

## Parameters

None.

## Return value

A `std::span` of the elements.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <numeric>
#include <span>

using namespace sgcl;

int sum(std::span<const int> values) {
    return std::accumulate(values.begin(), values.end(), 0);
}

int main() {
    vector v = {1, 2, 3};
    slice<const int> s = v;
    println("{}", sum(s));
}
```

Output:

```text
6
```

## See also

- [data](data.md): the elements as a plain pointer
- [(constructor)](slice.md): a slice of a `std::span`
- [sgcl::slice\<T\>](README.md)
