[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::digits

```cpp
int digits() const noexcept;
```

Returns the fraction digits an amount of the currency is written with (CLDR's `currencyData`): two for most, none
for the yen, three for the Kuwaiti dinar; two for a currency CLDR does not list and for no currency.

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
    for (auto code : {"PLN", "JPY", "KWD", "CLF"}) {
        println("{} {}", code, txt::currency(code).digits());
    }
}
```

Output:

```text
PLN 2
JPY 0
KWD 3
CLF 4
```

## See also

- [digits](digits.md), [cash_digits](cash_digits.md), [cash_increment](cash_increment.md)
- [number_options](../number_options.md)
- [sgcl::txt::currency](README.md)
