[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::symbol

```cpp
string symbol(const locale& l) const;
```

Returns what a locale writes for the currency (CLDR's symbols): `"zł"` for PLN in Polish and `"PLN"` in English,
`"$"` for USD in English and `"US$"` in Canadian English, `"USD"` in Polish; the code where the locale has no
symbol.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |

## Return value

The symbol.

## Complexity

Logarithmic in the number of locales and currencies.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::currency usd("USD"), pln("PLN");
    for (auto tag : {"en", "en-CA", "pl", "ja"}) {
        println("{} {} {}", tag, usd.symbol(txt::locale(tag)), pln.symbol(txt::locale(tag)));
    }
}
```

Output:

```text
en $ PLN
en-CA US$ PLN
pl USD zł
ja $ PLN
```

## See also

- [narrow_symbol](narrow_symbol.md)
- [format_currency](../format_currency.md)
- [sgcl::txt::currency](README.md)
