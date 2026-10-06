[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::of

```cpp
static currency of(const locale& l) noexcept;
```

Returns the currency of a locale's region today (CLDR's `currencyData`), the region filled from the [likely
subtags](../locale/maximize.md) when the locale has none: `pl` is PLN, `de-CH` CHF, `en` USD, `pt` BRL.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |

## Return value

The currency; no currency for a region without one.

## Complexity

Logarithmic in the size of CLDR's tables.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto tag : {"pl", "de-CH", "en", "pt", "ja"}) {
        println("{} {}", tag, txt::currency::of(txt::locale(tag)).code());
    }
}
```

Output:

```text
pl PLN
de-CH CHF
en USD
pt BRL
ja JPY
```

## See also

- [locale::maximize](../locale/maximize.md)
- [sgcl::txt::currency](README.md)
