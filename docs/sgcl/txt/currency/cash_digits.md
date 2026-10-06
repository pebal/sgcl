[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::cash_digits

```cpp
int cash_digits() const noexcept;
```

Returns the fraction digits of an amount paid in cash, where they differ: the Hungarian forint has two and none in
cash; otherwise the same as [digits](digits.md).

## Parameters

None.

## Return value

The count.

## Complexity

Logarithmic in the number of currencies.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::currency("HUF").digits(), txt::currency("HUF").cash_digits());
}
```

Output:

```text
2 0
```

## See also

- [digits](digits.md), [cash_digits](cash_digits.md), [cash_increment](cash_increment.md)
- [number_options](../number_options.md)
- [sgcl::txt::currency](README.md)
