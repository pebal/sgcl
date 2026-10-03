[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::operator[]

```cpp
reference operator[](size_type pos) noexcept;                // (1)
const_reference operator[](size_type pos) const noexcept;    // (2)
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

A `pos` outside the vector is undefined behaviour, as with `std::vector`; [at](at.md) is the same access with
the check. The reference is valid as long as the element: until it is removed, the vector reallocates or the
vector is destroyed. It does not keep the buffer alive; a [slice](../slice/README.md) from [as_slice](as_slice.md) does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {1, 2, 3, 4};
    for (size_t i : range(v.size())) {
        v[i] *= v[i];
    }
    println("{}", v);

    const vector<int>& view = v;
    println("{}", view[3]);
}
```

Output:

```text
[1, 4, 9, 16]
16
```

## See also

- [at](at.md): access an element with bounds checking
- [front](front.md), [back](back.md): access the first, the last element
- [sgcl::vector\<T\>](README.md)
