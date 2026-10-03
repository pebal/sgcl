[sgcl](../../README.md) › [core](../README.md) › [move_only_function](../move_only_function.md)

# sgcl::move_only_function\<R(Args...)\>::swap

```cpp
void swap(move_only_function& o) noexcept;
```

Swaps the callables of `*this` and `o`. A callable in the buffer moves by its move constructor, which does not
throw; a callable in a node moves with its node.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `move_only_function` to swap with |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    move_only_function<int()> a = [] { return 1; };
    move_only_function<int()> b = [] { return 2; };
    a.swap(b);
    println("{} {}", a(), b());
}
```

Output:

```text
2 1
```

## See also

- [swap](swap2.md): the same as a free function
- [sgcl::move_only_function\<R(Args...)\>](../move_only_function.md)
