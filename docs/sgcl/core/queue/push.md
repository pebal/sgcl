[sgcl](../../README.md) › [core](../README.md) › [queue](README.md)

# sgcl::queue\<T, Container\>::push

```cpp
void push(const value_type& value) noexcept(noexcept(c.push_back(value)));          // (1)
void push(value_type&& value) noexcept(noexcept(c.push_back(std::move(value))));    // (2)
```

Appends an element at the end of the queue: `c.push_back(value)`.

1. Appends a copy of `value`.
2. Appends `value`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to append |

## Return value

None.

## Complexity

The container's `push_back`: constant for `deque` and `list`.

## Exceptions

What the copy or the move constructor of `T` throws; none when it is noexcept and the container's `push_back` is
noexcept with it, as for `deque` and `list`.

If an exception is thrown, the container's `push_back` leaves the queue as it was, for `deque` and `list`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    queue<string> names;
    string first = "Ada";
    names.push(first);
    names.push("Grace");
    println("{} {} {}", names.size(), names.front(), names.back());
}
```

Output:

```text
2 Ada Grace
```

## See also

- [emplace](emplace.md): constructs the element in place
- [pop](pop.md): removes the first element
- [sgcl::queue\<T, Container\>](README.md)
