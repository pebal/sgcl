[sgcl](../../README.md) › [core](../README.md) › [any](../any.md)

# sgcl::swap (sgcl::any)

```cpp
#include "sgcl/core/any.h"   // or "sgcl/core.h"

namespace sgcl {
    void swap(any& l, any& r) noexcept;
}
```

Swaps the values of `l` and `r`: `l.swap(r)` ([swap](swap.md)).

## Parameters

| Parameter | Description |
|---|---|
| `l`, `r` | the `any` objects to swap |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Notes

Found by the argument's type: `swap(a, b)` written without a namespace, and the `using std::swap; swap(a, b);` of
generic code, call it.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    any a = 1.5;
    any b;
    swap(a, b);
    println("{} {}", a.has_value(), any_cast<double>(b));
}
```

Output:

```text
false 1.5
```

## See also

- [swap](swap.md): the member function
- [sgcl::any](../any.md)
