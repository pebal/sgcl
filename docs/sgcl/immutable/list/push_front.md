[sgcl](../../README.md) › [immutable](../README.md) › [list](../list.md)

# sgcl::immutable::list\<T\>::push_front

```cpp
/*(1)*/ list push_front(const T& value) const noexcept(std::is_nothrow_copy_constructible_v<T>);
/*(2)*/ list push_front(T&& value) const noexcept(std::is_nothrow_move_constructible_v<T>);
```

Returns the list with one more element in front: one new cell, linked to the first cell of this list, whose chain
the new list shares. This list is unchanged.

1. The new element is a copy of `value`.
2. The new element is `value`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to put in front |

## Return value

The new list, `size() + 1` elements.

## Complexity

Constant: one allocation, whatever the length.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept.

This list is never changed, so an exception leaves it as it was; no new list is made.

## Notes

A `push_front` is one managed allocation and one store through the barrier: 13 to 16 ns over a million `long`s,
each version let go of, against 11 ns for `std::forward_list` in place
([Benchmarks](../benchmarks.md#against-std)).

## Example

```cpp
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<int> l = {2, 3};
    auto one = l.push_front(1);
    auto zero = one.push_front(0);
    println("{} {} {}", l, one, zero);
    println("{}", zero.pop_front().pop_front() == l);  // the same chain
}
```

Output:

```text
[2, 3] [1, 2, 3] [0, 1, 2, 3]
true
```

## See also

- [emplace_front](emplace_front.md): the element constructed from arguments
- [pop_front](pop_front.md): the list without its first element
- [sgcl::immutable::list\<T\>](../list.md)
