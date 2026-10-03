[sgcl](../../README.md) › [core](../README.md) › [list](README.md)

# sgcl::list\<T\>::emplace

```cpp
template<class... A>
iterator emplace(const_iterator pos, A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Constructs an element before `pos` from the arguments `a`, forwarded to the constructor of `T`, in a node of its
own. The node is made and the element constructed in one step, before anything is linked: an argument may refer to
an element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `pos` | the element before which the new one goes; `end()` appends |
| `a` | the arguments of the constructor of `T` |

## Return value

An iterator to the new element.

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
    points.emplace(points.end(), 1, 2);
    auto it = points.emplace(points.begin(), 0, 0);
    println("{} {}, {} {}", it->x, it->y, points.back().x, points.back().y);

    list<string> words = {"b"};
    words.emplace(words.begin(), 3, 'a');  // string(3, 'a')
    println("{}", words);
}
```

Output:

```text
0 0, 1 2
["aaa", "b"]
```

## See also

- [insert](insert.md): inserts elements
- [emplace_back](emplace_back.md), [emplace_front](emplace_front.md): construct an element in place at an end
- [sgcl::list\<T\>](README.md)
