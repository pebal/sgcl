[sgcl](../../README.md) › [immutable](../README.md) › [list](README.md)

# sgcl::immutable::list\<T\>::emplace_front

```cpp
template<class... A>
list emplace_front(A&&... a) const noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Returns the list with one more element in front, constructed from `a...` in a new cell linked to the first cell of
this list. This list is unchanged.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

The new list, `size() + 1` elements.

## Complexity

Constant: one allocation, whatever the length.

## Exceptions

What the constructor of `T` from `a...` throws; none when it is noexcept.

This list is never changed, so an exception leaves it as it was; no new list is made.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/immutable.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    immutable::list<pair<string, int>> moves;
    auto one = moves.emplace_front("e4", 1);
    auto two = one.emplace_front("e5", 2);
    println("{} {} {}", moves.size(), two.front().first, two.size());
}
```

Output:

```text
0 e5 2
```

## See also

- [push_front](push_front.md): the list with a copy of a value in front
- [sgcl::immutable::list\<T\>](README.md)
