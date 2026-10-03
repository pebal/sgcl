[sgcl](../../README.md) › [core](../README.md) › [vector](../vector.md)

# sgcl::vector\<T\>::erase

```cpp
iterator erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>);    // (1)
iterator erase(const_iterator first, const_iterator last)                             // (2)
    noexcept(std::is_nothrow_move_assignable_v<T>);
```

Erases elements.

1. The element at `pos`. `pos` equal to `end()` erases nothing.
2. The elements of the range `[first, last)`; an empty range erases nothing.

The elements after the erased ones move down over them by move assignment, keeping their order; the vector then
destroys its last elements, as many as were erased. The capacity stays.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element to erase |
| `first`, `last` | the range of elements to erase |

## Return value

An iterator to the element that followed the last erased one, `end()` when the erased elements were the last.

## Complexity

Linear in the distance from the erased elements to the end, plus the destruction of the erased number of
elements.

## Exceptions

What the move assignment of `T` throws; none when it is noexcept.

If an exception is thrown, the vector stays consistent and every element is destroyed exactly once, but the
values have changed, as with `std::vector`.

## Notes

The elements are destroyed at once, here, not later by the collector. To erase every element equal to a value,
or every one a predicate accepts, in one pass: [erase, erase_if](erase_if.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector v = {0, 1, 2, 3, 4, 5, 6};

    auto it = v.erase(v.begin());
    println("{}, it -> {}", v, *it);

    it = v.erase(v.begin() + 1, v.begin() + 3);
    println("{}, it -> {}", v, *it);

    it = v.erase(v.end() - 1);
    println("{}, at the end: {}", v, it == v.end());
}
```

Output:

```text
[1, 2, 3, 4, 5, 6], it -> 1
[1, 4, 5, 6], it -> 4
[1, 4, 5], at the end: true
```

## See also

- [erase, erase_if](erase_if.md): erase every element equal to a value, or satisfying a predicate
- [clear](clear.md): destroys every element
- [pop_back](pop_back.md): removes the last element
- [sgcl::vector\<T\>](../vector.md)
