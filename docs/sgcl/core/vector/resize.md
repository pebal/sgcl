[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::resize

```cpp
/*(1)*/ void resize(size_type count);
/*(2)*/ void resize(size_type count, const value_type& value);
```

Changes the number of elements to `count`. Below the size, the last elements are destroyed, at once. Above it,
elements are appended:

1. Value-initialized, `T()`. For elements whose value-initialized form is all zero bits (`int`, `double`, a
   struct of them) the new part of the buffer is zeroed in one pass and no constructor runs.
2. Copies of `value`. `value` may be an element of this vector.

When `count` is above the capacity, the vector grows first, geometrically, as an append grows it: a resize by
one element at a time reallocates as rarely as a loop of [push_back](push_back.md).

## Parameters

| Parameter | Description |
|---|---|
| `count` | the new number of elements |
| `value` | the value the appended elements are copied from |

## Return value

None.

## Complexity

Linear in the difference between `size()` and `count`, plus linear in `size()` when the vector reallocates.

## Exceptions

- `length_error` when `count > max_size()`.
- What the constructor (1) or the copy constructor (2) of `T` throws, and the move of the elements already there
  when the vector reallocates.

If an exception is thrown, the vector holds the elements it held before the call; after (1) its capacity may
have grown.

## Notes

A shrinking resize keeps the buffer; [shrink_to_fit](shrink_to_fit.md) releases it. The buffer a growth leaves is
collected, not freed at once: a [slice](../slice.md) taken before still reads the old elements.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> v = {1, 2, 3};
    v.resize(5);
    println("{}", v);

    v.resize(7, 9);
    println("{}", v);

    v.resize(2);
    println("{}, capacity {}", v, v.capacity());
}
```

Output:

```text
[1, 2, 3, 0, 0]
[1, 2, 3, 0, 0, 9, 9]
[1, 2], capacity 8
```

## See also

- [size](size.md): the number of elements
- [reserve](reserve.md): reserves storage without adding elements
- [sgcl::vector\<T\>](../vector.md)
