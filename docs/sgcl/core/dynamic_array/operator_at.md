[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::operator[]

```cpp
/*(1)*/ reference operator[](size_type pos) noexcept;
/*(2)*/ const_reference operator[](size_type pos) const noexcept;
```

Returns a reference to the element at `pos`, without bounds checking: `pos` must be less than `size()`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the element |

## Return value

A reference to the element.

## Complexity

Constant.

## Exceptions

None.

## Notes

[at](at.md) is the same access with the check. The buffer never moves, so a reference or a pointer taken from
`operator[]` is valid until the array is assigned over, moved from or destroyed, whatever is written to the
elements in between.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<int> counts(4);
    int& first = counts[0];
    for (int i : range(4)) {
        counts[i] = i * 10;
    }
    first += 1;
    println("{}", counts);
}
```

Output:

```text
[1, 10, 20, 30]
```

## See also

- [at](at.md): access an element with bounds checking
- [front](front.md), [back](back.md): the first and the last element
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
