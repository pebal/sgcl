[sgcl](../../README.md) › [core](../README.md) › [stack](README.md)

# sgcl::swap (sgcl::stack)

```cpp
#include "sgcl/core/stack.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class T, class Container>
    void swap(stack<T, Container>& lhs, stack<T, Container>& rhs) noexcept(noexcept(lhs.swap(rhs)));
}
```

Exchanges the contents of `lhs` and `rhs`: `lhs.swap(rhs)` ([swap](swap.md)), the containers swapped. Declared
in `sgcl`, it is found by the arguments' type: `swap(a, b)` written without a namespace, and
`std::ranges::swap(a, b)`.

## Parameters

| Parameter | Description |
|---|---|
| `lhs`, `rhs` | the stacks to exchange the contents of |

## Return value

None.

## Complexity

Constant for the containers of the library.

## Exceptions

None for the containers of the library, whose `swap` is noexcept; what the swap of the containers throws
otherwise.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    stack<string> undo;
    stack<string> redo;
    undo.push("type");
    undo.push("delete");

    swap(undo, redo);
    println("{} {} {}", undo.empty(), redo.size(), redo.top());
}
```

Output:

```text
true 2 delete
```

## See also

- [swap](swap.md): the member form
- [sgcl::stack\<T, Container\>](README.md)
