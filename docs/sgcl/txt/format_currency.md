[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::format_currency

```cpp
template<class T>
string format_currency(T amount, const currency& c, const locale& l = {},
                       currency_display d = currency_display::symbol);
```

Returns an amount of a currency as a locale writes it: the locale's currency pattern, the currency's digits, its
symbol, narrow symbol or code where the locale puts it, a no-break space between a letter of the symbol and a digit.
The one-line form of a [number_format](number_format/README.md) of `number_style::currency`.

## Parameters

| Parameter | Description |
|---|---|
| `amount` | the amount: any arithmetic type but `bool` |
| `c` | the [currency](currency/README.md) |
| `l` | the locale; the root locale by default |
| `d` | what is written for the currency, [currency_display](currency_display.md) |

## Return value

The text.

## Complexity

Linear in the digits written, after lookups logarithmic in the numbers of locales and currencies.

## Exceptions

None that the program can cause; running out of memory ends it.

## Notes

Takes part only when `T` is an arithmetic type other than `bool`.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::currency pln("PLN");
    println("{}", txt::format_currency(12.5, pln, txt::locale("pl")));
    println("{}", txt::format_currency(12.5, pln, txt::locale("en")));
    println("{}", txt::format_currency(1234.5, txt::currency("JPY"), txt::locale("ja")));
    println("{}", txt::format_currency(-3.456, txt::currency("EUR"), txt::locale("de")));
}
```

Output:

```text
12,50 zł
PLN 12.50
￥1,234
-3,46 €
```

## See also

- [format_number](format_number.md)
- [currency](currency/README.md)
- [sgcl::txt](README.md)
