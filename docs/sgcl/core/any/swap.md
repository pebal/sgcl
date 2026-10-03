[sgcl](../../README.md) › [core](../README.md) › [any](README.md)

# sgcl::any::swap

```cpp
void swap(any& o) noexcept;
```

Swaps the values of `*this` and `o`. Each value moves to the other `any` as it is: a pointer word or a value in the
buffer by its move constructor, which does not throw, a value in a node with its node, without touching the value.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `any` to swap with |

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
    any a = 1;
    any b = string("two");
    a.swap(b);
    println("{} {}", any_cast<string&>(a), any_cast<int>(b));
}
```

Output:

```text
two 1
```

## See also

- [swap](swap2.md): the same as a free function
- [sgcl::any](README.md)
