[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::cash_increment

```cpp
int cash_increment() const noexcept;
```

Returns the step cash is rounded to, in units of the last cash digit: 5 for the Swiss franc (0.05), 50 for the
Danish krone (0.50), 0 for a currency whose cash is not rounded so.

## Parameters

None.

## Return value

The step.

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
    for (auto code : {"CHF", "DKK", "EUR"}) {
        println("{} {}", code, txt::currency(code).cash_increment());
    }
}
```

Output:

```text
CHF 5
DKK 50
EUR 0
```

## See also

- [digits](digits.md), [cash_digits](cash_digits.md), [cash_increment](cash_increment.md)
- [number_options](../number_options.md)
- [sgcl::txt::currency](README.md)
