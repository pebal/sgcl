[sgcl](../../README.md) › [core](../README.md) › [stack](../stack.md)

# sgcl::stack\<T, Container\>::swap

```cpp
void swap(stack& other) noexcept(std::is_nothrow_swappable_v<Container>);
```

Exchanges the contents of this stack with those of `other`: the containers are swapped, `swap(c, other.c)`, and
for the containers of the library no element is touched.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the stack to exchange the contents with |

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
    stack<int> a(deque{1, 2, 3});
    stack<int> b(deque{9});
    a.swap(b);
    println("{} {}, {} {}", a.size(), a.top(), b.size(), b.top());
}
```

Output:

```text
1 9, 3 3
```

## See also

- [swap](swap2.md): the non-member form
- [sgcl::stack\<T, Container\>](../stack.md)
