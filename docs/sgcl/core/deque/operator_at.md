[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::operator[]

```cpp
reference operator[](size_type pos) noexcept;                // (1)
const_reference operator[](size_type pos) const noexcept;    // (2)
```

Returns a reference to the element at `pos`, without bounds checking: `pos` must be less than `size()`.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the position of the element |

## Return value

A reference to the element.

## Complexity

Constant: a division by the block size and two loads.

## Exceptions

None.

## Notes

A `pos` outside the deque is undefined behaviour, as with `std::deque`; [at](at.md) is the same access with the
check. The reference stays valid across a push or a pop at either end, until the element is removed or an
insertion or an erasure in the middle moves the elements.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 3, 4};
    for (size_t i : range(d.size())) {
        d[i] *= d[i];
    }
    println("{}", d);

    int& last = d[3];
    d.push_front(0);
    d.push_back(25);
    println("{} {}", last, d);
}
```

Output:

```text
[1, 4, 9, 16]
16 [0, 1, 4, 9, 16, 25]
```

## See also

- [at](at.md): access an element with bounds checking
- [front](front.md), [back](back.md): access the first, the last element
- [sgcl::deque\<T\>](../deque.md)
