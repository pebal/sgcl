[sgcl](../../README.md) › [core](../README.md) › [dynamic_array](../dynamic_array.md)

# sgcl::dynamic_array\<T\>::as_slice, operator slice

```cpp
/*(1)*/ slice<T> as_slice() noexcept;
/*(2)*/ slice<const T> as_slice() const noexcept;
/*(3)*/ slice<T> as_slice(size_type pos, size_type n = size_type(-1));
/*(4)*/ slice<const T> as_slice(size_type pos, size_type n = size_type(-1)) const;
/*(5)*/ operator slice<T>() noexcept;
/*(6)*/ operator slice<const T>() const noexcept;
```

Returns the elements as a [slice](../slice.md) that holds the buffer they lie in.

- (1–2) All the elements.
- (3–4) The elements `[pos, pos + n)`, `n` cut to the size: `as_slice(pos)` is every element from `pos` on.
- (5–6) The conversion: the same as (1–2), so that a function that takes a slice takes an array as it is.

The buffer never moves, so the slice and the array reach the same elements for as long as the array holds that
buffer.

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

The slice keeps the memory of the buffer, never left on freed memory; the elements in it are the array's, and
the array destroys them when it gives the buffer up — assigned another size, moved over, destroyed — as a
[vector](../vector/as_slice.md) does on a reallocation.

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
    dynamic_array<int> a(4, 1);
    a.front() = 0;
    a.back() = 9;
    slice<int> all = a.as_slice();  // valid for as long as a is: the buffer never moves
    slice<int> tail = a.as_slice(2);
    tail[0] = 5;
    println("{} {} sum {}", all, tail, sum(a));

    try {
        a.as_slice(5);
    } catch (const out_of_range& e) {
        println("out of range: {}", e.what());
    }
}
```

Output:

```text
[0, 1, 5, 9] [5, 9] sum 15
out of range: sgcl::dynamic_array::as_slice
```

## See also

- [slice](../slice.md): a view of elements that holds their buffer
- [data](data.md): the buffer as a plain pointer
- [sgcl::dynamic_array\<T\>](../dynamic_array.md)
