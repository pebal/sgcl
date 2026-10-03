[sgcl](../../README.md) › [core](../README.md) › [unexpected](../unexpected.md)

# sgcl::swap (sgcl::unexpected)

```cpp
friend void swap(unexpected& x, unexpected& y) noexcept(noexcept(x.swap(y)))
    requires std::is_swappable_v<E>;
```

Swaps the errors of `x` and `y`: `x.swap(y)` ([swap](swap.md)). A hidden friend: found by the argument's type
alone, by `swap(a, b)` written without a namespace and by the `using std::swap; swap(a, b);` of generic code.

## Parameters

| Parameter | Description |
|---|---|
| `x`, `y` | the `unexpected` objects to swap |

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
    unexpected<string> a(string("first")), b(string("second"));
    swap(a, b);
    println("{} {}", a.error(), b.error());
}
```

Output:

```text
second first
```

## See also

- [swap](swap.md): the member function
- [sgcl::unexpected\<E\>](../unexpected.md)
