[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::minimize

```cpp
locale minimize() const noexcept;
```

Returns the locale with its likely subtags removed (TR35 §4.3): the shortest of the language, the language and
region, the language and script that maximizes to what this locale maximizes to, in that order of preference.
`zh-Hant-TW` is `zh-TW`, `sr-Cyrl-RS` is `sr`.

## Parameters

None.

## Return value

The minimized locale; `-u-nu-latn` kept.

## Complexity

Logarithmic in the size of CLDR's table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto tag : {"zh-Hant-TW", "sr-Cyrl-RS", "sr-Latn-RS", "pl-Latn-PL", "pa-Arab-PK"}) {
        println("{} {}", tag, txt::locale(tag).minimize().to_string());
    }
}
```

Output:

```text
zh-Hant-TW zh-TW
sr-Cyrl-RS sr
sr-Latn-RS sr-Latn
pl-Latn-PL pl
pa-Arab-PK pa-PK
```

## See also

- [maximize](maximize.md)
- [sgcl::txt::locale](README.md)
