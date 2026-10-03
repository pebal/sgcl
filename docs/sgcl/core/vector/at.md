[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::at

```cpp
reference at(size_type pos);                // (1)
const_reference at(size_type pos) const;    // (2)
```

Returns a reference to the element at `pos`, with bounds checking: a `pos` outside the vector throws.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the element |

## Return value

A reference to the element.

## Complexity

Constant.

## Exceptions

`out_of_range` when `pos >= size()`.

## Notes

[operator[]](operator_at.md) is the same access without the check. The reference is valid as long as the element: until it is
removed, the vector reallocates or the vector is destroyed. It does not keep the buffer alive; a
[slice](../slice/README.md) from [as_slice](as_slice.md) does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {10, 20, 30};
    v.at(1) = 25;
    println("{}", v);

    try {
        v.at(3) = 40;
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
[10, 25, 30]
out of range: sgcl::vector::at
```

## See also

- [operator[]](operator_at.md), [front](front.md), [back](back.md): access an element without the check
- [as_slice](as_slice.md): the elements as a slice that holds the buffer
- [sgcl::vector\<T\>](README.md)
