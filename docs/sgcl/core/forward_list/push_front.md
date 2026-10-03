[sgcl](../../README.md) › [core](../README.md) › [forward_list](../forward_list.md)

# sgcl::forward_list\<T\>::push_front

```cpp
void push_front(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push_front(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Inserts an element at the beginning, in a node of its own.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

The element is constructed in its node before the node is linked, so `value` may be an element of this list.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to insert |

## Return value

None.

## Complexity

Constant.

## Exceptions

What the copy (1) or the move (2) constructor of `T` throws; none when it is noexcept. If an exception is thrown,
the list is as it was before the call.

## Notes

Every insertion allocates the node alone: the sentinel is in the list object
([Benchmarks: Containers](../benchmarks.md#containers): 12.8 ns per `push_front` against 21.3 ns for
`std::forward_list`).

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    forward_list<string> names;
    string last = "Grace";
    names.push_front(last);
    names.push_front("Ada");
    names.push_front(names.front());  // an element of the list itself
    println("{}", names);

    forward_list<tracked_ptr<int>> squares;
    for (int i : range(1, 1001)) {
        squares.push_front(make_tracked<int>(i * i));
    }
    println("the first of the squares {}", *squares.front());
}
```

Output:

```text
["Ada", "Ada", "Grace"]
the first of the squares 1000000
```

## See also

- [emplace_front](emplace_front.md): constructs an element in place at the beginning
- [pop_front](pop_front.md): removes the first element
- [insert_after](insert_after.md): inserts elements after a position
- [sgcl::forward_list\<T\>](../forward_list.md)
