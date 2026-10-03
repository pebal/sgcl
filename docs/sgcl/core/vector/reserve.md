[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::reserve

```cpp
void reserve(size_type new_capacity);
```

Makes room for at least `new_capacity` elements. When `new_capacity` is above `capacity()`, the elements move
into a fresh buffer of at least that capacity; otherwise nothing happens. `reserve` never shrinks the buffer
([shrink_to_fit](shrink_to_fit.md) does).

## Parameters

| Parameter | Description |
|---|---|
| `new_capacity` | the number of elements the buffer must hold |

## Return value

None.

## Complexity

Linear in `size()` when the vector reallocates; constant otherwise.

## Exceptions

- `length_error` when `new_capacity > max_size()`.
- What the move of `T` throws.

If an exception is thrown, the vector is as it was before the call.

## Notes

The capacity after `reserve` is what the buffer's size class holds, which may be more than was asked for, as the
standard allows: `reserve(100)` of `int`s gives 124. Past a page it is what the pages hold next to the
buffer's header, so that a doubling fills whole pages again: `reserve(131072)` of `int`s gives 147 452.

The old buffer is left to the collector, as after any reallocation: a [slice](../slice.md) taken before still
reads the elements as they were.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> v;
    v.reserve(100);
    println("size {}, capacity {}", v.size(), v.capacity());

    int* before = v.data();
    for (int i : range(100)) {
        v.push_back(i);
    }
    println("size {}, the same buffer: {}", v.size(), v.data() == before);

    v.reserve(10);  // below the capacity: nothing happens
    println("capacity {}", v.capacity());

    vector<int> large;
    large.reserve(131072);
    println("capacity {}", large.capacity());
}
```

Output:

```text
size 0, capacity 124
size 100, the same buffer: true
capacity 124
capacity 147452
```

## See also

- [capacity](capacity.md), [shrink_to_fit](shrink_to_fit.md): the capacity, a buffer sized for the elements
- [push_back](push_back.md): appends an element, growing the buffer when needed
- [sgcl::vector\<T\>](../vector.md)
