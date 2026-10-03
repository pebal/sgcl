[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::emplace_back

```cpp
template<class... A>
reference emplace_back(A&&... a)
    noexcept(std::is_nothrow_constructible_v<T, A&&...> &&
             (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_copy_constructible_v<T>));
```

Appends an element constructed from `a...`, `T(std::forward<A>(a)...)`; with no arguments the element is
value-initialized, `T()`, so an `int` is 0.

When the size reaches the capacity, the vector first grows: it allocates a buffer of at least twice the capacity,
constructs the new element there, then moves the others over and destroys the moved-from ones. The arguments
may therefore refer to an element of this vector.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

A reference to the new element.

## Complexity

Amortized constant.

## Exceptions

What the constructor of `T` from `a...` throws; none when it and the move or the copy of `T` are noexcept. The
elements already there are moved to a new buffer with their move constructor when it is noexcept, else with
their copy constructor, which may throw too.

If an exception is thrown, the vector is as it was before the call.

## Notes

Without a growth, `emplace_back` is a compare of the size with the capacity, the construction of the element and
a store of the size, inlined into the caller's loop; the growth is a call of its own, as for
[push_back](push_back.md), which is `emplace_back` with one argument.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Edge {
    string from, to;
    int weight;
};

int main() {
    vector<Edge> edges;
    edges.emplace_back("a", "b", 3);
    Edge& e = edges.emplace_back("b", "c", 5);
    e.weight += 1;
    for (const Edge& edge : edges) {
        println("{} -> {}: {}", edge.from, edge.to, edge.weight);
    }

    vector<int> zeros;
    zeros.emplace_back();
    println("{}", zeros);
}
```

Output:

```text
a -> b: 3
b -> c: 6
[0]
```

## See also

- [push_back](push_back.md): appends a copy or a moved value
- [emplace](emplace.md): constructs an element in place at any position
- [sgcl::vector\<T\>](README.md)
