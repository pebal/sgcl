[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::at

```cpp
constexpr reference at(size_type pos);                // (1)
constexpr const_reference at(size_type pos) const;    // (2)
```

Returns a reference to the element at `pos`, with bounds checking: a `pos` outside the array throws. For
`array<T, 0>` every `pos` is outside, and `at` always throws.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the element |

## Return value

A reference to the element.

## Complexity

Constant.

## Exceptions

`out_of_range` when `pos >= N`.

## Notes

[operator[]](operator_at.md) is the same access without the check. In a constant expression a `pos` outside the
array is an error at compile time.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    array<int, 3> a = {10, 20, 30};
    a.at(1) = 25;
    println("{}", a);

    try {
        a.at(3) = 40;
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }

    array<int, 0> none;
    try {
        none.at(0);
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
[10, 25, 30]
out of range: sgcl::array::at
out of range: sgcl::array::at
```

## See also

- [operator[]](operator_at.md): access an element without the check
- [get](get.md): the element at a position given at compile time
- [sgcl::array\<T, N\>](README.md)
