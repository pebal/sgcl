[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::currency

```cpp
#include "sgcl/txt/currency.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class currency;
}
```

`sgcl::txt::currency` is a currency: the code of ISO 4217 in two bytes, with what CLDR 46 says of it — how many
digits an amount has (two; none for the yen; three for the Kuwaiti dinar), how cash is rounded (Swiss francs to
0.05), what a locale writes for it (`zł` in Polish, `PLN` in English, `US$` in Canadian English) and which currency
a locale's region uses. A [number_format](../number_format/README.md) of a currency style writes amounts of it.

## Rules

- Two bytes, trivially copyable: it lives anywhere.
- A code the program writes is constructed and throws when it is not one; a code from outside is read by
  [parse](parse.md) into an `expected` with a [code_error](../code_error/README.md). A code ISO 4217 does not list
  is a currency still, with two digits and its code for a symbol.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](currency.md) | constructs no currency, or the one a code names |
| [parse](parse.md) | reads a code from outside the program (static) |
| [of](of.md) | the currency of a locale's region (static) |

#### Observers

| Function | Description |
|---|---|
| [code](code.md) | the code: `"PLN"` |
| [digits](digits.md) | the fraction digits of an amount |
| [cash_digits](cash_digits.md) | the fraction digits of an amount in cash |
| [cash_increment](cash_increment.md) | the step cash is rounded to |
| [symbol](symbol.md) | what a locale writes for it: `"zł"` |
| [narrow_symbol](narrow_symbol.md) | the narrow symbol: `"$"` |
| [display_name](display_name.md) | its name in a language: `"złoty polski"` |
| [operator==](operator_cmp.md) | checks whether two currencies are one |
| [operator bool](operator_bool.md) | checks whether it holds a currency |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto c = txt::currency::of(txt::locale("pl"));
    println("{} {} {}", c.code(), c.digits(), c.symbol(txt::locale("pl")));
    println("{}", txt::format_currency(12.5, c, txt::locale("pl")));
}
```

Output:

```text
PLN 2 zł
12,50 zł
```

## See also

- [format_currency](../format_currency.md)
- [number_options](../number_options.md)
- [code_error](../code_error/README.md)
- [sgcl::txt](../README.md)
