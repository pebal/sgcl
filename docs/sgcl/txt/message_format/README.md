[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::message_format

```cpp
#include "sgcl/txt/message_format.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class message_format;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::message_format` is a message of ICU's MessageFormat, version 1 — the syntax ICU4C and ICU4J read and the
translation tools write (Crowdin, Lokalise, Weblate, FormatJS) — read once for a locale and written many times with
arguments: plurals by CLDR's rules, selects, numbers, dates and times in the locale's way. What ICU 78 answers is
what it answers, ICU the oracle of its tests, with the differences the rules name.

## Rules

- A handle of one word to an immutable message: copies share it, any number of threads write with it.
- Version 1 and not MessageFormat 2 (Unicode, CLDR 47): the syntax the catalogs in use are written in.
- Numbers are grouped as CLDR says (its minimum grouping digits: `1000` in Polish), as ICU's NumberFormatter and
  JavaScript's Intl group them; ICU's MessageFormat, through its older DecimalFormat, groups every number of four
  digits. A pattern of the message's own (`#,##0`) groups as it says.
- A locale without a region writes the currency of its likely region (`de`: euro), where ICU's MessageFormat writes
  `¤` or `XXX`. Dates are Gregorian in every locale (ICU writes the Persian calendar for `fa`).
- The zone names of `z` and `zzzz` and the long currency names come from the optional headers of the
  [display names](../names.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](message_format.md) | constructs the empty message, or the one a literal spells |
| [parse](parse.md) | reads a message from outside the program (static) |

#### Writing

| Function | Description |
|---|---|
| [format](format.md) | the message with the arguments |

#### Observers

| Function | Description |
|---|---|
| [where](where.md) | the locale it writes for |
| [pattern](pattern.md) | the text it was read from |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/time.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::message_format m("{who} wysłał {n, plural, one {# plik} few {# pliki} other {# plików}} "
                          "{d, date, ::d MMMM}.",
                          txt::locale("pl"));
    println("{}", m.format(txt::object{{"who", "Jan"}, {"n", 3}, {"d", 1759752300000}}));
    println("{}",
            m.format(txt::object{{"who", "Ewa"}, {"n", 25}, {"d", "2026-12-24T18:00:00+01:00"}}));
}
```

Output:

```text
Jan wysłał 3 pliki 6 października.
Ewa wysłał 25 plików 24 grudnia.
```

## See also

- [format_message](../format_message.md): the one-line form
- [message_error](../message_error/README.md)
- [catalog](../catalog/README.md)
- [plural_of](../plural_of.md)
- [number_format](../number_format/README.md)
- [Benchmarks](../benchmarks.md)
- [sgcl::txt](../README.md)
