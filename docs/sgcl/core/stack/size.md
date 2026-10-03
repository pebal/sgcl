[sgcl](../../README.md) › [core](../README.md) › [stack](../stack.md)

# sgcl::stack\<T, Container\>::size

```cpp
size_type size() const noexcept(noexcept(c.size()));
```

Returns the number of elements: `c.size()`.

## Parameters

None.

## Return value

The number of elements.

## Complexity

Constant for the containers of the library.

## Exceptions

None for the containers of the library, whose `size` is noexcept; what the container's `size` throws otherwise.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    stack<int, list<int>> s;
    for (int i : range(5)) {
        s.push(i);
    }
    s.pop();
    println("{}", s.size());
}
```

Output:

```text
4
```

## See also

- [empty](empty.md): checks whether the stack is empty
- [sgcl::stack\<T, Container\>](../stack.md)
