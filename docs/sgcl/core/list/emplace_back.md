[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::emplace_back

```cpp
template<class... A>
reference emplace_back(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Constructs an element at the end from the arguments `a`, forwarded to the constructor of `T`, in a node of its own,
and returns a reference to it. The element is constructed before the node is linked: an argument may refer to an
element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `T` |

## Return value

A reference to the new element.

## Complexity

Constant.

## Exceptions

What the constructor of `T` throws; none when it is noexcept. If an exception is thrown, the list is as it was
before the call.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    list<Point> points;
    Point& p = points.emplace_back(1, 2);
    p.y = 20;
    points.emplace_back(3, 4);
    println("{} points, the first {} {}", points.size(), points.front().x, points.front().y);
}
```

Output:

```text
2 points, the first 1 20
```

## See also

- [push_back](push_back.md): appends an element
- [emplace_front](emplace_front.md): constructs an element in place at the beginning
- [emplace](emplace.md): constructs an element in place at a position
- [sgcl::list\<T\>](README.md)
