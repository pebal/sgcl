[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::swap

```cpp
void swap(vector& other) noexcept;
```

Exchanges the contents of this vector and `other`: the pointers to the buffers, the sizes and the capacities. No
element is moved, copied or destroyed.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the vector to exchange the contents with |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

Iterators, pointers and references to the elements stay valid and refer to the same elements, which now belong
to the other vector, as with `std::vector`; an `end()` taken before is the end of the other vector. A
[slice](../slice/README.md) holds the buffer it was taken from, whichever vector holds it now.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector a = {1, 2, 3};
    vector b = {7, 8};
    int* first = a.data();

    a.swap(b);
    println("{} {}", a, b);
    println("{}", first == b.data());
}
```

Output:

```text
[7, 8] [1, 2, 3]
true
```

## See also

- [swap](swap2.md): the non-member form, `swap(a, b)`
- [operator=](operator_assign.md): assigns another vector
- [sgcl::vector\<T\>](README.md)
