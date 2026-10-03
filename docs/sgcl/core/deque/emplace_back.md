[sgcl](../../README.md) › [core](../README.md) › [deque](../deque.md)

# sgcl::deque\<T\>::emplace_back

```cpp
template<class... A>
reference emplace_back(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A&&...>);
```

Constructs an element from `a` in place at the end and returns a reference to it. The path is that of
[push_back](push_back.md): in the common case a load of the map, a load of the block, the construction and two
stores; the map at its end or a missing block goes the slow way. The elements already there never move, so an
argument may refer to an element of this deque.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `T` |

## Return value

A reference to the new element.

## Complexity

Constant.

## Exceptions

What the constructor of `T` from `a` throws; none when it is noexcept.

If an exception is thrown, the deque is as it was before the call.

## Notes

The references to the other elements stay valid, the iterators do not
([Iterator invalidation](../deque.md#iterator-invalidation)).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    deque<Point> path;
    path.emplace_back(0, 0);
    Point& last = path.emplace_back(3, 4);
    last.y = 5;
    println("{} points, the last ({}, {})", path.size(), path.back().x, path.back().y);

    deque<tracked_ptr<int>> ptrs;
    int& value = *ptrs.emplace_back(make_tracked<int>(1000));
    println("{}", value);
}
```

Output:

```text
2 points, the last (3, 5)
1000
```

## See also

- [push_back](push_back.md): appends a copy or a moved value
- [emplace_front](emplace_front.md): constructs an element in place at the beginning
- [emplace](emplace.md): constructs an element in place at any position
- [sgcl::deque\<T\>](../deque.md)
