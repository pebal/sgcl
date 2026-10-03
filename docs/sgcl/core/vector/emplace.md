[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::emplace

```cpp
template<class... A>
iterator emplace(const_iterator pos, A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A&&...> &&
             std::is_nothrow_move_constructible_v<T> &&
             std::is_nothrow_move_assignable_v<T>);
```

Inserts before `pos` an element constructed from `a...`, `T(std::forward<A>(a)...)`.

At the end, `emplace` is [emplace_back](emplace_back.md). Within the capacity the element is first constructed
aside, since the arguments may refer to an element that is about to move, then the elements from `pos` on move
up by one and the new one is moved into the gap. Above the capacity the element is constructed in a fresh buffer
first and the old ones move over after, so the arguments may refer to an element of this vector here too.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element before which the new one goes; `end()` appends |
| `a` | the arguments the element is constructed from |

## Return value

An iterator to the inserted element.

## Complexity

Constant, plus linear in the distance between `pos` and the end.

## Exceptions

What the constructor of `T` from `a...`, its move constructor and its move assignment throw; none when they are
noexcept.

When the vector reallocates, or when what throws is the construction of the new element, the vector is as it
was before the call. When a move of `T` throws within the capacity, the vector stays consistent and every
element is destroyed exactly once, but the values have changed, as with `std::vector`.

## Notes

A reallocation leaves the old buffer to the collector instead of freeing it: a [slice](../slice/README.md) taken
before the call still reads the old elements. The capacity of a growth is at least twice the old one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    vector<Point> points = {{1, 1}, {3, 3}};
    auto it = points.emplace(points.begin() + 1, 2, 2);
    println("({}, {}) inserted", it->x, it->y);
    for (const Point& p : points) {
        println("({}, {})", p.x, p.y);
    }
}
```

Output:

```text
(2, 2) inserted
(1, 1)
(2, 2)
(3, 3)
```

## See also

- [insert](insert.md): inserts copies or moved values
- [emplace_back](emplace_back.md): constructs an element in place at the end
- [sgcl::vector\<T\>](README.md)
