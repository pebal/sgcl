[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::at

```cpp
/*(1)*/ reference at(size_type pos);
/*(2)*/ const_reference at(size_type pos) const;
```

Returns a reference to the element at `pos`, with bounds checking: a `pos` outside the array throws.

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

[operator[]](operator_at.md) is the same access without the check. The buffer never moves, so the reference is
valid until the array is assigned over, moved from or destroyed. It does not keep the buffer alive; a
[slice](../slice.md) from [as_slice](as_slice.md) does.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    dynamic_array<int> a = {10, 20, 30};
    a.at(1) = 25;
    println("{}", a);

    try {
        a.at(3) = 40;
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
[10, 25, 30]
out of range: sgcl::dynamic_array::at
```

## See also

- [operator[]](operator_at.md): access an element without the check
- [as_slice, operator slice](as_slice.md): the elements as a slice that holds the buffer
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
