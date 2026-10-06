[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::locale

```cpp
#include "sgcl/txt/locale.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class locale;
}
```

`sgcl::txt::locale` is a language and where and how it is written: the value every function of the module that
depends on a language takes — the case mappings, the collator, the numbers, the plural rules, the lists, relative
time, and in [time](../../time/README.md) the dates. It reads a BCP-47 tag or the name of a POSIX locale (what
`LANG` holds) and keeps three of its parts, the language, the script and the region, and one key of the Unicode
extension, `-u-nu-latn`; the rest of a tag is read past. It knows CLDR 46's likely subtags of every language
([maximize](maximize.md)), the parents of its locales ([parent](parent.md)), and each locale's name in its own
language ([autonym](autonym.md)); [best_match](../best_match.md) chooses among locales by CLDR's language matching.
Where Go has `language.Tag` and ICU `Locale`, this is eight bytes and no allocation.

## Rules

- Eight bytes, trivially copyable, `constexpr`: the language's letters, the script, the region and the one key, packed.
  It lives anywhere.
- Nothing throws and nothing fails: a tag that cannot be read is the root locale, and a tag read in part keeps the part
  read. A language CLDR has no data for is still that language; what formats in it formats in the root locale's
  patterns and its rules in root's.
- Two locales are equal when all three parts and the key are ([operator==](operator_cmp.md)): `de-CH` and `de-DE`
  write numbers differently. What depends on the language alone asks [subtag](subtag.md),
  [dotted_i](dotted_i.md) and [keeps_dot](keeps_dot.md).
- The default, [root](root.md), is no language: the mappings of Unicode without a tailoring and the data of CLDR's
  root.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](locale.md) | constructs the root locale, or the one a tag names |

#### Languages

| Function | Description |
|---|---|
| [root](root.md) | the root locale, no language (static) |
| [system](system.md) | the user's locale, from `LC_ALL`, `LC_MESSAGES` or `LANG` (static) |
| [turkish](turkish.md) | Turkish, `tr` (static) |
| [azerbaijani](azerbaijani.md) | Azerbaijani, `az` (static) |
| [lithuanian](lithuanian.md) | Lithuanian, `lt` (static) |

#### Observers

| Function | Description |
|---|---|
| [language](language.md) | the language subtag: `"sr"` |
| [script](script.md) | the script subtag: `"Latn"` |
| [region](region.md) | the region subtag: `"RS"`, `"419"` |
| [to_string](to_string.md) | the tag in BCP-47's canonical form: `"sr-Latn-RS"` |
| [latin_digits](latin_digits.md) | checks whether the tag asked for Latin digits, `-u-nu-latn` |
| [subtag](subtag.md) | the language subtag in four bytes |
| [dotted_i](dotted_i.md) | checks whether the language writes an i the Turkish way |
| [keeps_dot](keeps_dot.md) | checks whether the language keeps the dot above an i under an accent |
| [operator==](operator_cmp.md) | checks whether two locales are one locale |

#### CLDR

| Function | Description |
|---|---|
| [maximize](maximize.md) | the likely subtags added: `sr` is `sr-Cyrl-RS` |
| [minimize](minimize.md) | the likely subtags removed: `zh-Hant-TW` is `zh-TW` |
| [parent](parent.md) | the locale CLDR inherits from: `es-MX` is `es-419` |
| [autonym](autonym.md) | the locale's name in its own language: `"polski"` |
| [display_name](display_name.md) | the locale's name in another language, from the [display names](../names.md): `"niemiecki (Szwajcaria)"` |
| [has_names](has_names.md) | checks whether the display names of the locale are included |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto l = txt::locale("zh-TW");
    println("{} {} {}", l.to_string(), l.maximize().to_string(), l.autonym());
    auto tr = txt::locale("tr-TR");
    println("{} | {}", txt::to_lower_full("ISTANBUL", tr), txt::to_lower_full("ISTANBUL"));
    println("{} {}", tr == txt::locale::turkish(), tr.subtag() == txt::locale::turkish().subtag());
}
```

Output:

```text
zh-TW zh-Hant-TW 繁體中文
ıstanbul | istanbul
false true
```

## See also

- [best_match](../best_match.md): the supported locale closest to what a user wants
- [to_lower_full](../to_lower_full.md), [to_upper_full](../to_upper_full.md), [to_title](../to_title.md): the mappings that
  take it
- [collator](../collator/README.md): the order of a language
- [number_format](../number_format/README.md), [plural_of](../plural_of.md), [format_list](../format_list.md),
  [format_relative](../format_relative.md), [date_format](../../time/date_format/README.md): what formats in it
- [sgcl::txt](../README.md)
