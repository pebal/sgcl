[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::push_front

```cpp
void push_front(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push_front(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Inserts an element at the beginning.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

The common case, a block with room at the front (the first one in use, or the spare), is a load of the map, a
load of the block, the construction and two stores (the block's range and the count). The map at its beginning or
a missing block goes the slow way: the spare block of the back moves across when there is one, else a block is
allocated; a map at its beginning is replaced by a fresh one, twice as large when more than half full, else of the
same size with the blocks re-centred. The elements already there never move, so `value` may be an element of this
deque.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to insert |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the copy constructor (1) or the move constructor (2) of `T` throws; none when it is noexcept.

If an exception is thrown, the deque is as it was before the call.

## Notes

The references to the other elements stay valid, the iterators do not
([Iterator invalidation](../deque.md#iterator-invalidation)). `std::vector` has no `push_front`: a deque is the
container with cheap insertion at both ends.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<tracked_ptr<int>> ptrs;
    for (int i : range(1000)) {
        ptrs.push_back(make_tracked<int>(i));
        ptrs.push_front(make_tracked<int>(-i));  // the outgrown maps are collected
    }
    println("{} elements, from {} to {}", ptrs.size(), *ptrs.front(), *ptrs.back());
}
```

Output:

```text
2000 elements, from -999 to 999
```

## See also

- [emplace_front](emplace_front.md): constructs an element in place at the beginning
- [pop_front](pop_front.md): removes the first element
- [push_back](push_back.md): appends an element at the end
- [sgcl::deque\<T\>](../deque.md)
