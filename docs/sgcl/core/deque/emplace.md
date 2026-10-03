[sgcl](../../README.md) › [core](../README.md) › [deque](README.md)

# sgcl::deque\<T\>::emplace

```cpp
template<class... A>
iterator emplace(const_iterator pos, A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A&&...> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
```

Constructs an element from `a` before `pos`. At either end the element is constructed in place, as
[emplace_front](emplace_front.md) and [emplace_back](emplace_back.md) do. In the middle it is constructed first,
before anything moves, so an argument that refers to an element of this deque stays valid; then the shorter side
of the deque shifts by one and the element is moved into its place.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element before which the new one goes; `end()` appends |
| `a` | the arguments of the constructor of `T` |

## Return value

An iterator to the new element.

## Complexity

Constant at either end; otherwise linear in the distance between `pos` and the nearer end.

## Exceptions

What the constructor of `T` from `a`, its move constructor and its move assignment throw; none when they are
noexcept.

When a constructor of `T` throws, the deque is as it was before the call. When a move assignment throws while the
elements shift, the deque stays consistent and every element is destroyed exactly once, but the values and the
size have changed.

## Notes

An insertion at either end keeps the references to the other elements valid and invalidates the iterators; an
insertion in the middle invalidates both ([Iterator invalidation](README.md#iterator-invalidation)).

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
    path.emplace(path.end(), 0, 0);
    path.emplace(path.end(), 2, 2);
    auto it = path.emplace(path.begin() + 1, 1, 1);
    println("({}, {})", it->x, it->y);

    for (const Point& p : path) {
        println("{} {}", p.x, p.y);
    }
}
```

Output:

```text
(1, 1)
0 0
1 1
2 2
```

## See also

- [insert](insert.md): inserts copies of values or a range
- [emplace_back](emplace_back.md), [emplace_front](emplace_front.md): construct an element in place at either end
- [sgcl::deque\<T\>](README.md)
