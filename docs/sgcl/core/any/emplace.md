[sgcl](../../README.md) › [core](../README.md) › [any](README.md)

# sgcl::any::emplace

```cpp
template<class T, class... A, class VT = std::decay_t<T>>
requires std::is_constructible_v<VT, A...> && std::is_copy_constructible_v<VT>
VT& emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<VT, A...>);             // (1)
template<class T, class U, class... A, class VT = std::decay_t<T>>
requires std::is_constructible_v<VT, std::initializer_list<U>&, A...>
         && std::is_copy_constructible_v<VT>
VT& emplace(std::initializer_list<U> il, A&&... a)                                     // (2)
    noexcept(std::is_nothrow_constructible_v<VT, std::initializer_list<U>&, A...>);
```

Destroys the value held, if any, and constructs a `VT` in its place, placed as the [constructor](any.md) places
it.

1. From `a...`.
2. From `il` and `a...`.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the value is constructed from |
| `il` | the initializer list the value is constructed from |

## Return value

A reference to the new value: into the `any` itself, or into the node that holds it.

## Complexity

Constant: one managed allocation for a value in a node, none for the others.

## Exceptions

What the constructor of `VT` throws; none when it is noexcept.

The old value is destroyed before the new one is constructed: if the construction throws, the `any` is empty.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Point {
    int x, y;
};

int main() {
    any a = 2.5;
    Point& p = a.emplace<Point>(1, 2);  // the double destroyed first
    p.x = 10;
    println("{} {}", any_cast<Point&>(a).x, any_cast<Point&>(a).y);

    vector<int>& v = a.emplace<vector<int>>({3, 4, 5});
    v.push_back(6);
    println("{}", any_cast<vector<int>&>(a));
}
```

Output:

```text
10 2
[3, 4, 5, 6]
```

## See also

- [operator=](operator_assign.md): assigns a value made beforehand
- [make_any](../make_any.md): a new `any` with a value constructed in place
- [sgcl::any](README.md)
