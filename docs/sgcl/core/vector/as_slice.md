[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::as_slice, operator slice

```cpp
slice<T> as_slice() noexcept;                                                 // (1)
slice<const T> as_slice() const noexcept;                                     // (2)
slice<T> as_slice(size_type pos, size_type n = size_type(-1));                // (3)
slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const;    // (4)
operator slice<T>() noexcept;                                                 // (5)
operator slice<const T>() const noexcept;                                     // (6)
```

Returns the elements as a [slice](../slice.md) that holds the buffer they lie in.

- (1–2) All the elements.
- (3–4) The elements `[pos, pos + n)`, `n` cut to the size: `as_slice(pos)` is every element from `pos` on.
- (5–6) The conversion: the same as (1–2), so that a function that takes a slice takes a vector as it is.

The slice stays valid whatever the vector does next. A reallocation gives the vector a new buffer and leaves the
slice on the old one, alive and unchanged, never on freed memory; the destruction of the vector leaves it the
same way.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the first element of the slice |
| `n` | the number of elements, at most `size() - pos` |

## Return value

A slice of the elements, which keeps their buffer alive.

## Complexity

Constant.

## Exceptions

- (1–2), (5–6) None.
- (3–4) `out_of_range` when `pos > size()`.

## Notes

`as_slice` has no counterpart in `std::vector`: a `std::span` of a vector dangles after a reallocation, a slice
does not. A vector converts to a slice by itself, so a function that takes a slice takes a vector as it is:
`f(v)`, not `f(v.as_slice())`. A slice of a vector is what a stream reads into, `r.read(v)`, and what a function
of the library takes as a range of elements.

A write through the slice and a write through the vector reach the same element only while the vector keeps the
buffer the slice was taken from.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int sum(const slice<const int>& values) {
    int total = 0;
    for (int x : values) {
        total += x;
    }
    return total;
}

int main() {
    vector v = {1, 2, 3};
    slice<int> all = v.as_slice();
    slice<int> tail = v.as_slice(1);
    println("{} {} sum {}", all, tail, sum(v));

    v.shrink_to_fit();
    v.push_back(4);  // a new buffer: the slices keep the old one
    v[0] = 10;
    println("{} {} {}", v, all, tail);
}
```

Output:

```text
[1, 2, 3] [2, 3] sum 6
[10, 2, 3, 4] [1, 2, 3] [2, 3]
```

## See also

- [slice](../slice.md): a view of elements that holds their buffer
- [data](data.md): the buffer as a plain pointer
- [sgcl::vector\<T\>](../vector.md)
