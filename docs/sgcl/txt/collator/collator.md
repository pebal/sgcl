[sgcl](../../README.md) › [txt](../README.md) › [collator](README.md)

# sgcl::txt::collator::collator

```cpp
collator() noexcept = default;                                                         // (1)
explicit collator(locale where, strength level = strength::tertiary) noexcept;         // (2)
explicit collator(strength level) noexcept;                                            // (3)
explicit collator(const options& how) noexcept;                                        // (4)
collator(locale where, const options& how) noexcept;                                   // (5)
explicit collator(const string& tag, strength level = strength::tertiary) noexcept;    // (6)
collator(const string& tag, const options& how) noexcept;                              // (7)
```

Constructs a collator of the root order or of a language's.

1. The root order, the DUCET, at tertiary strength, with no setting.
2. The order of the language of `where`, at the strength `level`, with the settings the language asks for.
3. The root order at the strength `level`.
4. The root order with the strength and the settings of `how`.
5. The order of the language of `where`, with the strength of `how`; each setting `how` gives overrules the
   language's, and each it leaves unset takes the language's.
6. (2) for the language the tag `tag` names, read as [locale](../locale/README.md)`(tag)` reads it: `txt::collator("pl")`,
   a BCP-47 tag or a POSIX name (`pl-PL`, `pl_PL.UTF-8`); an unknown tag is the root order.
7. (5) for the language the tag `tag` names.

- (6–7) A constructor of its own rather than a locale made of a text by itself: every function that takes a locale
  would then take any text, and a misspelt tag would pass as the root locale without a word.

What the language asks for is what the collator starts with, and what the caller writes is what it takes instead:
Danish sorts its capitals first and Thai shifts its punctuation aside; a caller who wants the Danish order with the
small letters first says so and the language is overruled, and one who says nothing gets the language's own answer
rather than silence. A language the library has no order for takes the root order ([tailored](tailored.md) says
which). Everything is settled here: whether anything given changes what an element weighs or how a weight is read,
and where nothing does, the comparison, the key and the walk that feeds them are those of the plain root order.

## Parameters

| Parameter | Description |
|---|---|
| `where` | the locale; its language subtag alone is read, so `locale("pl-PL")` is `locale("pl")` |
| `tag` | the language's tag, BCP-47 or POSIX, read as `locale(tag)` reads it |
| `level` | how much of a difference counts, a [strength](../strength.md) |
| `how` | the strength and the settings, [options](../collator-options.md) |

## Complexity

Constant: the language is looked up by a bisection over 88.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::collator danish(txt::locale("da"));
    txt::collator own(txt::locale("da"), {.case_order = txt::case_order::lower_first});
    println("{} {}", danish.capitals_first(), own.capitals_first());
    println("{} {}", danish("A", "a"), own("A", "a"));
    println("{}", txt::collator(txt::locale("th")).shifts_punctuation());

    txt::collator polish("pl");
    println("{} {}", polish("lody", "łódź"), txt::collator("pl_PL.UTF-8").where() == polish.where());
}
```

Output:

```text
true false
true false
true
true true
```

## See also

- [options](../collator-options.md): the settings
- [tailored](tailored.md): whether the language has an order of its own
- [sgcl::txt::collator](README.md)
