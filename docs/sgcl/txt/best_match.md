[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::best_match

```cpp
locale best_match(const slice<const locale>& desired,                // (1)
                  const slice<const locale>& supported) noexcept;
locale best_match(const string& accept_language,                     // (2)
                  const slice<const locale>& supported) noexcept;
```

Returns the supported locale closest to what the user wants, by CLDR's language matching (TR35 §4.4, the distances
of `languageInfo.xml`): both sides [maximized](locale/maximize.md), then a distance of their languages, their
scripts and their regions summed — `de-CH` is close to `de`, `en-AU` closer to `en-GB` than to `en`, `es-AR` to
`es-419`, `sr` (Cyrillic) is 5 from `sr-Latn`, a different language or script is 50 or more. Each desired locale
after the first is a little further (5 a place), so the user's order counts; the best under 50 wins — of equals the
first, unless another is the desired locale exactly (`en-US` over `en`, which maximizes the same) — and when none is
that close the first supported locale is the answer, the program's default.

1. The desired locales in the user's order.
2. The value of an Accept-Language header (RFC 9110 §12.5.4): its languages in the order of their weights, those of
   weight 0 and `*` left out, the first 32 read; a header with none of them is the default.

## Parameters

| Parameter | Description |
|---|---|
| `desired` | the locales the user wants, the most wanted first |
| `accept_language` | an Accept-Language header: `"pl, en-GB;q=0.8, en;q=0.5"` |
| `supported` | the locales the program has, its default first |

## Return value

The chosen supported locale; the root locale when `supported` is empty.

## Complexity

Linear in the product of the counts of the desired and the supported locales.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::locale supported[] = {txt::locale("en"), txt::locale("de"), txt::locale("en-GB"),
                               txt::locale("es-419")};
    for (auto header : {"de-CH", "en-AU, en;q=0.9", "es-AR", "fr", "pl, de;q=0.5"}) {
        println("{} -> {}", header, txt::best_match(header, supported).to_string());
    }
}
```

Output:

```text
de-CH -> de
en-AU, en;q=0.9 -> en-GB
es-AR -> es-419
fr -> en
pl, de;q=0.5 -> de
```

## See also

- [locale::maximize](locale/maximize.md)
- [locale](locale/README.md)
- [sgcl::txt](README.md)
