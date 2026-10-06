[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::locale

```cpp
constexpr locale() noexcept = default;          // (1)
explicit locale(const string& tag) noexcept;    // (2)
```

Constructs a locale.

1. The root locale: no language.
2. The locale of the BCP-47 tag or POSIX locale name `tag`, read in any case, its subtags separated by `-` or `_`.
   The language is the first subtag, two or three ASCII letters; anything else there (an empty tag, `"C"`,
   `"POSIX"`, `"root"`, a longer word) makes the whole the root locale. After it an extended language subtag
   (`zh-yue`) and a variant (`de-1996`) are read past, four letters are the script, two letters or three digits the
   region, and the extensions are walked for `-u-nu-latn`, the one key kept; private use (`-x-`) ends the tag, and
   so does a subtag that is none of these, with what was read so far kept. A POSIX name ends at the `.` of its
   codeset, and its modifier `@latin` or `@cyrillic` is the script. The codes CLDR replaces are replaced: `iw` is
   `he`, `in` `id`, `ji` `yi`, `jw` `jv`, `mo` `ro`, `tl` `fil`, and `sh` is `sr-Latn`.

## Parameters

| Parameter | Description |
|---|---|
| `tag` | a language tag or a POSIX locale name: `"pl"`, `"de-CH"`, `"zh-Hant-TW"`, `"pl_PL.UTF-8"` |

## Complexity

Linear in the length of the tag.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto tag : {"pl", "de_CH", "sr-latn-rs", "zh-yue-HK", "pl_PL.UTF-8", "sr@latin",
                     "en-US-u-nu-latn", "C"}) {
        println("{} -> {}", tag, txt::locale(tag).to_string());
    }
}
```

Output:

```text
pl -> pl
de_CH -> de-CH
sr-latn-rs -> sr-Latn-RS
zh-yue-HK -> zh-HK
pl_PL.UTF-8 -> pl-PL
sr@latin -> sr-Latn
en-US-u-nu-latn -> en-US-u-nu-latn
C -> und
```

## See also

- [to_string](to_string.md)
- [system](system.md)
- [root](root.md)
- [sgcl::txt::locale](README.md)
