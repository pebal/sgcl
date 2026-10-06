[sgcl](../../README.md) › [txt](../README.md) › [currency](README.md)

# sgcl::txt::currency::narrow_symbol

```cpp
string narrow_symbol(const locale& l) const;
```

Returns the narrow symbol, for a place where the reader knows from the context which dollar is meant: `"$"` for USD,
CAD and AUD, `"zł"`; the [symbol](symbol.md) where CLDR has no narrow one (`"CHF"`).

## Parameters

| Parameter | Description |
|---|---|
| `l` | the locale |

## Return value

The narrow symbol.

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
    for (auto code : {"USD", "AUD", "PLN", "CHF"}) {
        println("{} {}", code, txt::currency(code).narrow_symbol(txt::locale("pl")));
    }
}
```

Output:

```text
USD $
AUD $
PLN zł
CHF CHF
```

## See also

- [symbol](symbol.md)
- [sgcl::txt::currency](README.md)
