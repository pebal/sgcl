[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::display_name

```cpp
string display_name(const locale& in) const;              // (1)
string display_name(const locale& in, plural p) const;    // (2)
```

Returns the currency's name in the language of `in`, from the display names of `in`'s header
(`sgcl/txt/names/<in>.h`, [display names](../names.md)) or of a parent's; the code where none is included (a Debug
build says once on stderr which header would give it). The headers name the currencies in use today and the codes of
`X` (gold, special drawing rights).

1. Its name: "złoty polski", "US Dollar".
2. Its name in a [plural](../plural.md) form, as it follows a number: "złotych polskich" for `many`; the form `other`
   where the language has no name of that form, then (1). A [number_format](../number_format/README.md) with
   `currency_display::name` writes an amount so: "5,00 złotego polskiego".

## Parameters

| Parameter | Description |
|---|---|
| `in` | the locale whose language the name is in |
| `p` | the plural form |

## Return value

The name, or the code.

## Complexity

Linear in the number of included locales, then logarithmic in their names.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    txt::currency pln("PLN");
    auto pl = txt::locale("pl");
    println("{} | {} | {}", pln.display_name(pl), pln.display_name(pl, txt::plural::many),
            pln.display_name(txt::locale("de")));
    println("{}", txt::format_number(5, pl,
                                     {.style = txt::number_style::currency,
                                      .max_fraction = 0,
                                      .currency = pln,
                                      .display = txt::currency_display::name}));
}
```

Output:

```text
złoty polski | złotych polskich | Polnischer Złoty
5 złotych polskich
```

## See also

- [symbol](symbol.md)
- [display names](../names.md)
- [sgcl::txt::currency](README.md)
