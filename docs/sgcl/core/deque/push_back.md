[sgcl](../../README.md) › [core](../README.md) › [deque](README.md)

# sgcl::deque\<T\>::push_back

```cpp
void push_back(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push_back(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Appends an element at the end.

1. Appends a copy of `value`.
2. Appends `value`, moved.

The common case, a block with room at the end (the last one in use, or the spare), is a load of the map, a load
of the block, the construction and two stores (the block's range and the count). The map at its end or a missing
block goes the slow way: the spare block of the front moves across when there is one, else a block is allocated;
a map at its end is replaced by a fresh one, twice as large when more than half full, else of the same size with
the blocks re-centred. The elements already there never move, so `value` may be an element of this deque.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the copy constructor (1) or the move constructor (2) of `T` throws; none when it is noexcept.

If an exception is thrown, the deque is as it was before the call.

## Notes

The references to the other elements stay valid, the iterators do not
([Iterator invalidation](README.md#iterator-invalidation)). An outgrown map is left to the collector, never
freed at once ([Benchmarks: Containers](../benchmarks.md#containers): 2.0 ns per push against 1.5 ns for
`std::deque`).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    deque<string> names;
    string first = "Ada";
    names.push_back(first);
    names.push_back("Grace");
    println("{}", names);

    deque<tracked_ptr<int>> squares;
    for (int i : range(1, 1001)) {
        squares.push_back(make_tracked<int>(i * i));  // the outgrown maps are collected
    }
    println("{} squares, the last {}", squares.size(), *squares.back());
}
```

Output:

```text
["Ada", "Grace"]
1000 squares, the last 1000000
```

## See also

- [emplace_back](emplace_back.md): constructs an element in place at the end
- [pop_back](pop_back.md): removes the last element
- [push_front](push_front.md): inserts an element at the beginning
- [sgcl::deque\<T\>](README.md)
