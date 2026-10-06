[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::operator==

```cpp
constexpr bool operator==(const currency&) const noexcept = default;
```

Checks whether two currencies are one: the same code. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `(unnamed)` | the other currency |

## Return value

`true` when both have the same code, or neither has one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::currency("pln") == txt::currency("PLN"),
            txt::currency("PLN") != txt::currency("EUR"));
}
```

Output:

```text
true true
```

## See also

- [code](code.md)
- [sgcl::txt::currency](README.md)
