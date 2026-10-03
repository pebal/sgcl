[sgcl](../../README.md) › [core](../README.md) › [stack](README.md)

# sgcl::stack\<T, Container\>::top

```cpp
reference top() noexcept(noexcept(c.back()));                // (1)
const_reference top() const noexcept(noexcept(c.back()));    // (2)
```

Returns a reference to the top element, the last one pushed: `c.back()`. The stack must not be empty.

## Parameters

None.

## Return value

A reference to the top element.

## Complexity

Constant.

## Exceptions

None for the containers of the library, whose `back` is noexcept; what the container's `back` throws otherwise.

## Notes

The reference is valid as long as the container's `back()` would be: until the element is popped, or, over a
`vector`, until a push reallocates. A `tracked_ptr` may not address the element; a copy of a `tracked_ptr` element
taken from `top()` is a pointer of its own and keeps its object after the pop.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    stack<string> s;
    s.push("bottom");
    s.push("top");
    println("{}", s.top());

    s.top() = "peak";
    println("{}", s.top());

    stack<tracked_ptr<int>> pointers;
    pointers.push(make_tracked<int>(7));
    tracked_ptr kept = pointers.top();
    pointers.pop();
    println("{} {}", *kept, pointers.empty());
}
```

Output:

```text
top
peak
7 true
```

## See also

- [push](push.md), [pop](pop.md): insert and remove the top element
- [sgcl::stack\<T, Container\>](README.md)
