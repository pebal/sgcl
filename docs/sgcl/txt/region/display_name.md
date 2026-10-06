[sgcl](../../README.md) › [txt](../README.md) › [region](README.md)

# sgcl::txt::region::display_name

```cpp
string display_name(const locale& in) const;
```

Returns the region's name in the language of `in`, from the display names of `in`'s header (`sgcl/txt/names/<in>.h`,
[display names](../names.md)) — or of a parent's: `de-AT` reads `de.h` — CLDR 46's as ICU's `uloc_getDisplayCountry`
writes it. Where no header of `in`'s chain is included the code comes back, and a Debug build says once on stderr
which header would give the name.

## Parameters

| Parameter | Description |
|---|---|
| `in` | the locale whose language the name is in |

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
    txt::region x("DE");
    println("{} | {}", x.display_name(txt::locale("pl")), x.display_name(txt::locale("de-AT")));
}
```

Output:

```text
Niemcy | Deutschland
```

## See also

- [locale::display_name](../locale/display_name.md)
- [display names](../names.md)
- [sgcl::txt::region](README.md)
