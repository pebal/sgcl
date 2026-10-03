[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::shrink_to_fit

```cpp
void shrink_to_fit() noexcept(std::is_nothrow_move_constructible_v<T> ||
                              std::is_nothrow_copy_constructible_v<T>);
```

Replaces the buffer by one sized for the elements, when the capacity is above the size. The elements move into
the new buffer, with their move constructor when it is noexcept, else with their copy constructor; the
moved-from ones are destroyed. An empty vector lets its buffer go and has no buffer after, capacity 0. When the
capacity equals the size, nothing happens.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()` when the vector reallocates; constant otherwise.

## Exceptions

What the move constructor of `T` throws, or its copy constructor when the move is not noexcept; none when either
is noexcept.

If an exception is thrown, the vector is as it was before the call.

## Notes

Unlike `std::vector`'s, the request is always carried out, but the new buffer's size class may still round the
capacity up a little: ten `int`s get a buffer of 12. The old buffer is left to the collector, as after any
reallocation: a [slice](../slice/README.md) taken before still reads the elements as they were.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<int> v(1000);
    v.resize(10);
    println("size {}, capacity {}", v.size(), v.capacity());

    v.shrink_to_fit();
    println("size {}, capacity {}", v.size(), v.capacity());

    v.clear();
    v.shrink_to_fit();
    println("capacity {}, no buffer: {}", v.capacity(), v.data() == nullptr);
}
```

Output:

```text
size 10, capacity 1020
size 10, capacity 12
capacity 0, no buffer: true
```

## See also

- [reserve](reserve.md): reserves storage
- [capacity](capacity.md): the number of elements the buffer holds
- [sgcl::vector\<T\>](README.md)
