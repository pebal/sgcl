[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::operator bool

```cpp
constexpr explicit operator bool() const noexcept;
```

Checks whether the object holds a currency rather than none.

## Parameters

None.

## Return value

`false` for a default-constructed currency.

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
    println("{} {}", bool(txt::currency("EUR")), bool(txt::currency()));
}
```

Output:

```text
true false
```

## See also

- [(constructor)](currency.md)
- [sgcl::txt::currency](README.md)
