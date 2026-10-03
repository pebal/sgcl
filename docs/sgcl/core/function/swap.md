[sgcl](../../README.md) › [core](../README.md) › [function](README.md)

# sgcl::function\<R(Args...)\>::swap

```cpp
void swap(function& o) noexcept;
```

Swaps the callables of `*this` and `o`. A callable in the buffer moves by its move constructor, which does not
throw; a closure in a node moves with its node, without touching the closure.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `function` to swap with |

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
    function<int()> a = [] { return 1; };
    function<int()> b = [] { return 2; };
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
- [sgcl::function\<R(Args...)\>](README.md)
