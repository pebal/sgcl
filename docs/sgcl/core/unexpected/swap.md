[sgcl](../../README.md) › [core](../README.md) › [unexpected](README.md)

# sgcl::unexpected\<E\>::swap

```cpp
void swap(unexpected& o) noexcept(std::is_nothrow_swappable_v<E>)
    requires std::is_swappable_v<E>;
```

Swaps the errors of `*this` and `o`, with `swap` found as `using std::swap; swap(a, b)` finds it.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the `unexpected` to swap with |

## Return value

None.

## Complexity

Constant, plus the swap of the errors.

## Exceptions

What the swap of `E` throws; none when it is noexcept.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    unexpected<int> a(1), b(2);
    a.swap(b);
    println("{} {}", a.error(), b.error());
}
```

Output:

```text
2 1
```

## See also

- [swap](swap2.md): the same as a free function
- [sgcl::unexpected\<E\>](README.md)
