[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::autonym

```cpp
string autonym() const;
```

Returns the name of the locale in its own language, as CLDR gives it, for a menu of languages: `"polski"`,
`"Deutsch"`, `"Schweizer Hochdeutsch"` for `de-CH`, `"British English"` for `en-GB`, `"español de México"`. The name
of the most specific of the locale's tag, its language and region, its language and script and its language that
CLDR names; the language subtag for a language CLDR has no data for, and an empty text for the root locale. Names of
languages in other languages are display names, in the optional headers of each locale.

## Parameters

None.

## Return value

The name.

## Complexity

Logarithmic in the number of locales.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto tag : {"pl", "de", "de-CH", "en-GB", "es-MX", "ja", "sr-Latn"}) {
        println("{}", txt::locale(tag).autonym());
    }
}
```

Output:

```text
polski
Deutsch
Schweizer Hochdeutsch
British English
español de México
日本語
srpski
```

## See also

- [to_string](to_string.md)
- [sgcl::txt::locale](README.md)
