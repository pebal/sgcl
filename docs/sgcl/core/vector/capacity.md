[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::capacity

```cpp
size_type capacity() const noexcept;
```

Returns the number of elements the current buffer holds: the elements there are and the room for those an
append may construct without a reallocation. It is 0 for a vector without a buffer.

## Parameters

None.

## Return value

The capacity of the buffer.

## Complexity

Constant.

## Exceptions

None.

## Notes

The capacity is what the buffer's size class holds, read once when the vector takes the buffer, and is often
above the number asked for: a vector made of 1000 `int`s holds 1020. A growth by an append at least doubles it,
because the buffer left behind is collected, not freed at once, and a doubling halves what waits for the
collector. [reserve](reserve.md) sets it ahead, [shrink_to_fit](shrink_to_fit.md) brings it down to the size.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> v;
    vector<size_t> capacities = {v.capacity()};
    for (int i : range(17)) {
        v.push_back(i);
        if (v.capacity() != capacities.back()) {
            capacities.push_back(v.capacity());
        }
    }
    println("{}", capacities);

    vector<int> thousand(1000);
    println("{}", thousand.capacity());
}
```

Output:

```text
[0, 4, 8, 16, 36]
1020
```

## See also

- [reserve](reserve.md): reserves storage
- [shrink_to_fit](shrink_to_fit.md): replaces the buffer by one sized for the elements
- [size](size.md): the number of elements
- [sgcl::vector\<T\>](README.md)
