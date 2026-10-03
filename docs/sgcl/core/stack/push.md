[sgcl](../../README.md) › [core](../README.md) › [stack](README.md)

# sgcl::stack\<T, Container\>::push

```cpp
void push(const value_type& value) noexcept(noexcept(c.push_back(value)));          // (1)
void push(value_type&& value) noexcept(noexcept(c.push_back(std::move(value))));    // (2)
```

Inserts an element at the top: `c.push_back(value)`.

1. Inserts a copy of `value`.
2. Inserts `value`, moved.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value of the element to insert |

## Return value

None.

## Complexity

What the container's `push_back` costs: amortized constant over a `vector`, constant over a `deque` or a `list`.

## Exceptions

What the container's `push_back` throws: for the containers of the library, what the copy (1) or the move (2)
constructor of `T` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    stack<string> s;
    string first = "first";
    s.push(first);
    s.push("second");
    println("{} {} {}", s.size(), s.top(), first);
}
```

Output:

```text
2 second first
```

## See also

- [emplace](emplace.md): constructs an element in place at the top
- [pop](pop.md): removes the top element
- [sgcl::stack\<T, Container\>](README.md)
