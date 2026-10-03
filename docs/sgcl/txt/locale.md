[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::locale

```cpp
#include "sgcl/txt/case.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class locale;
}
```

`sgcl::txt::locale` is the language a case mapping or a collation may depend on. Three languages change the case of
a letter — Turkish, Azerbaijani and Lithuanian — and the tables of Unicode name no others; the type takes a BCP-47
tag all the same, so that a language out of an HTTP header or out of the system needs no table of its own at the
caller, and so that a [collator](collator.md) takes the same type. Only the language subtag is read: `"tr"`,
`"tr-TR"`, `"TR"` and `"tr_TR.UTF-8"` are Turkish, and `"az-Latn-AZ"` is Azerbaijani. A POSIX name, what `LANG`
holds, is read the same way: the language ends at the `.` of a codeset and at the `@` of a modifier as well, so
`"tr.UTF-8"` and `"tr@euro"` are Turkish and `"pl_PL.UTF-8"` is Polish. A modifier names nothing the type keeps:
`"sr@latin"` is Serbian, its script not kept, as `"sr-Latn"` is.

## Rules

- Four bytes, trivially copyable, `constexpr`: the language subtag lower-cased, packed. It lives anywhere.
- An unknown tag, or one longer or stranger than a subtag of two or three letters, is the root locale, and nothing
  throws.
- The default, [root](locale/root.md), is no language: the mappings of Unicode without a tailoring.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](locale/locale.md) | constructs the root locale, or the one a tag names |

#### Languages

| Function | Description |
|---|---|
| [root](locale/root.md) | the root locale, no language (static) |
| [turkish](locale/turkish.md) | Turkish, `tr` (static) |
| [azerbaijani](locale/azerbaijani.md) | Azerbaijani, `az` (static) |
| [lithuanian](locale/lithuanian.md) | Lithuanian, `lt` (static) |

#### Observers

| Function | Description |
|---|---|
| [dotted_i](locale/dotted_i.md) | checks whether the language writes an i the Turkish way |
| [keeps_dot](locale/keeps_dot.md) | checks whether the language keeps the dot above an i under an accent |
| [subtag](locale/subtag.md) | the language subtag in four bytes |
| [operator==](locale/operator_cmp.md) | checks whether two locales are one language |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto tr = txt::locale("tr-TR");
    println("{} | {}", txt::to_lower_full("ISTANBUL", tr), txt::to_lower_full("ISTANBUL"));
    println("{} {}", tr == txt::locale::turkish(), txt::locale("unknown") == txt::locale());
}
```

Output:

```text
ıstanbul | istanbul
true true
```

## See also

- [to_lower_full](to_lower_full.md), [to_upper_full](to_upper_full.md), [to_title](to_title.md): the mappings that
  take it
- [collator](collator.md): the order of a language, which takes it too
- [sgcl::txt](README.md)
