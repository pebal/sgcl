[sgcl](../../README.md) › [core](../README.md) › [array](README.md)

# sgcl::array\<T, N\>::as_slice, operator slice

```cpp
slice<T> as_slice() noexcept;                                                 // (1)
slice<const T> as_slice() const noexcept;                                     // (2)
slice<T> as_slice(size_type pos, size_type n = size_type(-1));                // (3)
slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const;    // (4)
operator slice<T>() noexcept;                                                 // (5)
operator slice<const T>() const noexcept;                                     // (6)
```

Returns the elements as a [slice](../slice/README.md) without an owner, as a C array and a `std::array` give one: the
array lives on a stack or inside an object, and whoever holds that keeps the elements.

- (1–2) All the elements.
- (3–4) The elements `[pos, pos + n)`, `n` cut to the size: `as_slice(pos)` is every element from `pos` on.
- (5–6) The conversion: the same as (1–2), so that a function that takes a slice takes an array as it is
  (`encoding::big_endian::write_u32(header, v)`).

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the first element of the slice |
| `n` | the number of elements, at most `N - pos` |

## Return value

A slice of the elements.

## Complexity

Constant.

## Exceptions

- (1–2), (5–6) None.
- (3–4) `out_of_range` when `pos > N`.

## Notes

The slice does not keep the array alive: it is valid as long as the array is, as a `std::span` of a
`std::array`. That differs from the slice of a [vector](../vector/as_slice.md) or a
[dynamic_array](../dynamic_array/as_slice.md), which holds their managed buffer.

`slice s(a)` deduces `slice<T>` from an array, or `slice<const T>` from a const one
([Deduction guides](README.md#deduction-guides)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include <type_traits>

using namespace sgcl;

int sum(const slice<const int>& values) {
    int total = 0;
    for (int x : values) {
        total += x;
    }
    return total;
}

int main() {
    array<int, 4> a = {1, 2, 3, 4};
    slice<int> tail = a.as_slice(2);
    tail[0] = 30;  // writes the array's element
    println("{} {} sum {}", a, tail, sum(a));

    array<byte, 6> header = {};
    encoding::big_endian::write_u32(header, 0xCAFEBABE);
    println("{::02x}", header);

    const array<int, 2> fixed = {7, 8};
    slice s(fixed);  // slice<const int>
    println("{} {}", s, std::is_same_v<decltype(s), slice<const int>>);
}
```

Output:

```text
[1, 2, 30, 4] [30, 4] sum 37
[ca, fe, ba, be, 00, 00]
[7, 8] true
```

## See also

- [slice](../slice/README.md): a view of elements
- [data](data.md): the elements as a plain pointer
- [sgcl::array\<T, N\>](README.md)
