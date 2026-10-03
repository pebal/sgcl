[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::as_slice

```cpp
slice as_slice() const noexcept;
```

The slice itself: what every container with a buffer answers ([vector](../vector/as_slice.md), `string`), so that
generic code asks one question of a vector, a string and a slice alike.

## Parameters

None.

## Return value

A copy of the slice, with the same owner.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

template<class Container>
size_t count_zeros(const Container& c) {
    return c.as_slice().count_of([](int x) { return x == 0; });  // a vector or a slice
}

int main() {
    vector v = {0, 1, 0, 2};
    slice<int> tail = v.as_slice(1);
    println("{} {}", count_zeros(v), count_zeros(tail));
}
```

Output:

```text
2 1
```

## See also

- [as_slice](../vector/as_slice.md): the slice of a vector
- [sgcl::slice\<T\>](README.md)
