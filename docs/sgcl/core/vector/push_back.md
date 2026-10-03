[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::push_back

```cpp
void push_back(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (1)
void push_back(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (2)
```

Appends an element at the end.

1. Appends a copy of `value`.
2. Appends `value`, moved.

When the size reaches the capacity, the vector first grows: it allocates a buffer of at least twice the capacity,
constructs the new element there, then moves the others over and destroys the moved-from ones. `value` may
therefore be an element of this vector.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

None.

## Complexity

Amortized constant.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept. The elements already there are
moved to a new buffer with their move constructor when it is noexcept, else with their copy constructor, which
may throw too.

If an exception is thrown, the vector is as it was before the call.

## Notes

Without a growth, `push_back` is a compare of the size with the capacity, the construction of the element and a
store of the size, inlined into the caller's loop; the growth is a call of its own. The buffer a growth leaves is
not freed at once but collected, so the capacity doubles to halve what waits for the collector
([Benchmarks: Containers](../benchmarks.md#containers): 2.7 ns per `push_back` against 1.6 ns for
`std::vector`). [reserve](reserve.md) avoids the growths when the number of elements is known.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<string> names;
    string first = "Ada";
    names.push_back(first);
    names.push_back("Grace");
    println("{}", names);

    vector<tracked_ptr<int>> squares;
    for (int i : range(1, 6)) {
        squares.push_back(make_tracked<int>(i * i));  // the outgrown buffers are collected
    }
    println("{} squares, the last {}", squares.size(), *squares.back());
}
```

Output:

```text
["Ada", "Grace"]
5 squares, the last 25
```

## See also

- [insert](insert.md): inserts elements at any position
- [emplace_back](emplace_back.md), [pop_back](pop_back.md): construct an element in place at the end, remove
  the last element
- [sgcl::vector\<T\>](README.md)
