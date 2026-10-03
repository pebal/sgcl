[sgcl](../../README.md) › [txt](../README.md) › [locale](../locale.md)

# sgcl::txt::locale::locale

```cpp
constexpr locale() noexcept = default;          // (1)
explicit locale(const string& tag) noexcept;    // (2)
```

Constructs a locale.

1. The root locale: no language.
2. The language of the BCP-47 tag or POSIX locale name `tag`: its first subtag, up to a `-` or a `_`, or the `.`
   of a codeset or the `@` of a modifier (`"pl_PL.UTF-8"`, `"tr.UTF-8"`, `"sr@latin"`), read in any case. A subtag of
   two or three ASCII letters is kept, whatever language it names; anything else (an empty tag, a longer subtag, a
   digit, `"C.UTF-8"`, `"POSIX"`) is the root locale. Only the language is kept: a region, a script and a modifier
   are read past.

## Parameters

| Parameter | Description |
|---|---|
| `tag` | a language tag or a POSIX locale name: `"tr"`, `"tr-TR"`, `"az-Latn-AZ"`, `"pl_PL.UTF-8"`, `"tr.UTF-8"` |

## Complexity

Linear in the length of the first subtag.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto tag : {"tr", "TR-tr", "tr_TR.UTF-8", "turkish", ""}) {
        println("\"{}\" {}", tag, txt::locale(tag) == txt::locale::turkish());
    }
    println("{}", txt::locale("pl") == txt::locale());
}
```

Output:

```text
"tr" true
"TR-tr" true
"tr_TR.UTF-8" true
"turkish" false
"" false
false
```

## See also

- [root](root.md)
- [subtag](subtag.md)
- [sgcl::txt::locale](../locale.md)
