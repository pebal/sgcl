[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::erase

```cpp
iterator erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>);    // (1)
iterator erase(const_iterator first, const_iterator last)                             // (2)
    noexcept(std::is_nothrow_move_assignable_v<T>);
```

Erases elements.

1. Erases the element at `pos`. `erase(end())` does nothing.
2. Erases the elements of the range `[first, last)`. An empty range does nothing.

The shorter side of the deque shifts over the erased elements, by move assignment, and the elements left at that
end are popped: the elements are destroyed at the end nearer to the range, as `std::deque` may do.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element to erase |
| `first`, `last` | the range of elements to erase |

## Return value

An iterator to the element after the erased ones, or `end()` when they were the last.

## Complexity

Linear in the number of erased elements, plus linear in the distance between the range and the nearer end.

## Exceptions

What the move assignment of `T` throws; none when it is noexcept.

If an exception is thrown, the deque stays consistent and every element is destroyed exactly once, but the
values have changed.

## Notes

An erasure at the beginning invalidates only the erased elements, one at the end the erased elements and `end()`;
an erasure in the middle invalidates every iterator and reference
([Iterator invalidation](../deque.md#iterator-invalidation)). A block emptied by the erasure stays as the spare
of its end, as after [pop_front](pop_front.md) and [pop_back](pop_back.md).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque d = {1, 2, 3, 4, 5};
    auto it = d.erase(d.begin());
    println("{}, it -> {}", d, *it);

    it = d.erase(it + 1, d.end());
    println("{}, at the end: {}", d, it == d.end());

    d.erase(d.end());  // nothing to erase
    println("{}", d);
}
```

Output:

```text
[2, 3, 4, 5], it -> 2
[2], at the end: true
[2]
```

## See also

- [erase, erase_if](erase_if.md): erase every element equal to a value, or satisfying a predicate
- [pop_front](pop_front.md), [pop_back](pop_back.md): remove the first, the last element
- [clear](clear.md): destroys every element
- [sgcl::deque\<T\>](../deque.md)
