[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::maximize

```cpp
locale maximize() const noexcept;
```

Returns the locale with its likely subtags added (TR35 §4.3, CLDR's `likelySubtags.xml`): the script and the region
a language is most often written with and in, the language of a region or a script alone. `"sr"` is `sr-Cyrl-RS`,
`"zh-TW"` `zh-Hant-TW`, `"und-PL"` `pl-Latn-PL`, the root locale `en-Latn-US`; the unknown script `Zzzz` and region
`ZZ` count as none, and a language CLDR does not know stays as it is.

## Parameters

None.

## Return value

The maximized locale; `-u-nu-latn` kept.

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
    for (auto tag : {"pl", "sr", "sr-ME", "zh-TW", "und-PL", "pa-PK"}) {
        println("{} {}", tag, txt::locale(tag).maximize().to_string());
    }
}
```

Output:

```text
pl pl-Latn-PL
sr sr-Cyrl-RS
sr-ME sr-Latn-ME
zh-TW zh-Hant-TW
und-PL pl-Latn-PL
pa-PK pa-Arab-PK
```

## See also

- [minimize](minimize.md)
- [parent](parent.md)
- [sgcl::txt::locale](README.md)
