[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::display_name

```cpp
string display_name(const locale& in) const;
```

Returns the locale's name in the language of `in` (TR35 §3.3 in its standard form, as ICU's `uloc_getDisplayName`
writes it): the language's name, with the script's and the region's in the locale's pattern — "niemiecki
(Szwajcaria)", "serbski (łacińskie, Czarnogóra)", "chiński (tradycyjne, Tajwan)" — from the display names of `in`'s
header (`sgcl/txt/names/<in>.h`, [display names](../names.md)) or of a parent's. A part without a name is its code;
where no header of `in`'s chain is included the whole is codes ("de (CH)"), and a Debug build says once on stderr
which header would give the names. The root locale is the language `und` ("Unknown language").

## Parameters

| Parameter | Description |
|---|---|
| `in` | the locale whose language the name is in |

## Return value

The name.

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
    auto pl = txt::locale("pl");
    for (auto tag : {"de-CH", "sr-Latn-ME", "zh-Hant-TW", "pt-BR"}) {
        println("{}", txt::locale(tag).display_name(pl));
    }
    println("{}", txt::locale("pl").display_name(txt::locale("de")));
}
```

Output:

```text
niemiecki (Szwajcaria)
serbski (łacińskie, Czarnogóra)
chiński (tradycyjne, Tajwan)
portugalski (Brazylia)
Polnisch
```

## See also

- [autonym](autonym.md): the name in its own language, built in
- [has_names](has_names.md)
- [display names](../names.md)
- [sgcl::txt::locale](README.md)
