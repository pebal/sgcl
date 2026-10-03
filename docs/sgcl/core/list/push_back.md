[sgcl](../../README.md) › [core](../README.md) › [list](../list.md)

# sgcl::list\<T\>::push_back

```cpp
void push_back(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push_back(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Appends an element at the end, in a node of its own.

1. Appends a copy of `value`.
2. Appends `value`, moved.

The element is constructed in its node before the node is linked, so `value` may be an element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the copy (1) or the move (2) constructor of `T` throws; none when it is noexcept. If an exception is thrown,
the list is as it was before the call.

## Notes

The first insertion into a list makes its sentinel; every other one allocates the node alone
([Benchmarks: Containers](../benchmarks.md#containers): 15.8 ns per `push_back` against 25.6 ns for `std::list`).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    list<string> names;
    string first = "Ada";
    names.push_back(first);
    names.push_back("Grace");
    names.push_back(names.front());  // an element of the list itself
    println("{}", names);

    list<tracked_ptr<int>> squares;
    for (int i : range(1, 1001)) {
        squares.push_back(make_tracked<int>(i * i));
    }
    println("{} squares, the last {}", squares.size(), *squares.back());
}
```

Output:

```text
["Ada", "Grace", "Ada"]
1000 squares, the last 1000000
```

## See also

- [emplace_back](emplace_back.md): constructs an element in place at the end
- [pop_back](pop_back.md): removes the last element
- [push_front](push_front.md): inserts an element at the beginning
- [sgcl::list\<T\>](../list.md)
