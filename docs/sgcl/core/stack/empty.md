[sgcl](../../README.md) › [core](../README.md) › [stack](../stack.md)

# sgcl::stack\<T, Container\>::empty

```cpp
bool empty() const noexcept(noexcept(c.empty()));
```

Checks whether the stack has no elements: `c.empty()`.

## Parameters

None.

## Return value

`true` when the stack has no elements, `false` otherwise.

## Complexity

Constant.

## Exceptions

None for the containers of the library, whose `empty` is noexcept; what the container's `empty` throws otherwise.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    stack<int> s;
    println("{}", s.empty());

    s.push(1);
    println("{}", s.empty());

    s.pop();
    println("{}", s.empty());
}
```

Output:

```text
true
false
true
```

## See also

- [size](size.md): the number of elements
- [sgcl::stack\<T, Container\>](../stack.md)
