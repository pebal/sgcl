[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::currency_display

```cpp
#include "sgcl/txt/number.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class currency_display : uint8_t {
        symbol,
        narrow_symbol,
        code,
        name,
    };
}
```

What a [number_format](number_format/README.md) of a currency style writes for the currency. Where the symbol's
letter faces the digits, a no-break space comes between them (CLDR's `currencySpacing`): `PLN 12.50`, `12,50 zł`,
but `$12.50`. The name in the plural form of the amount comes from the optional headers of the [display
names](names.md); without the locale's, the code.

| Value | Description |
|---|---|
| `symbol` | the locale's [symbol](currency/symbol.md): `$`, `US$`, `zł`, `PLN` |
| `narrow_symbol` | the [narrow symbol](currency/narrow_symbol.md): `$` for any dollar |
| `code` | the ISO 4217 code: `USD` |
| `name` | the [name](currency/display_name.md) in the amount's plural form, after it: `12.50 Canadian dollars` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/en.h"

using namespace sgcl;

int main() {
    for (auto d : {txt::currency_display::symbol, txt::currency_display::narrow_symbol,
                   txt::currency_display::code, txt::currency_display::name}) {
        println("{}", txt::format_currency(12.5, txt::currency("CAD"), txt::locale("en"), d));
    }
}
```

Output:

```text
CA$12.50
$12.50
CAD 12.50
12.50 Canadian dollars
```

## See also

- [format_currency](format_currency.md)
- [currency](currency/README.md)
- [display names](names.md)
- [sgcl::txt](README.md)
