[sgcl](../../README.md) › [txt](../README.md) › [message_format](README.md)

# sgcl::txt::message_format::parse

```cpp
static expected<message_format, message_error> parse(const string& pattern,
                                                       const locale& l) noexcept;
```

Reads a message from outside the program (a catalog, a file) for the locale `l`. The syntax is ICU's MessageFormat,
version 1: text with arguments `{name}`, `{name, type}`, `{name, type, style}` (a name, or a number for a position);
the types number, date, time, plural, selectordinal and select; apostrophes as ICU's default reads them (`''` is
one, an apostrophe before a brace, or a `#` inside a plural, quotes up to the next one, any other is a letter). The
styles are resolved once here: a number's keyword (integer, percent, currency), a skeleton after `::` or a pattern
of DecimalFormat; a date's short, medium, long, full, a skeleton or a pattern.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the message |
| `l` | the locale it writes for |

## Return value

The message, or a [message_error](../message_error/README.md) with the byte where it stopped: an argument without
its brace or name, a type it does not know — and the ones it does not have: spellout, ordinal and duration (ICU's
rule-based number formats) and the deprecated choice —, a plural or select without its `other` case, a number style
it does not read, messages nested deeper than 64.

## Complexity

Linear in the length of the pattern.

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto p : {"{n, plural, one {# file} other {# files}}", "{n, plural, one {# file}}",
                   "{n, spellout}"}) {
        auto m = txt::message_format::parse(p, txt::locale("en"));
        if (m) {
            println("{}", m->format(txt::object{{"n", 2}}));
        } else {
            println("at {}: {}", m.error().offset(), m.error().message());
        }
    }
}
```

Output:

```text
2 files
at 24: a plural or select argument without its other case
at 4: an argument type this formatter does not have (spellout, ordinal, duration, choice)
```

## See also

- [(constructor)](message_format.md)
- [format](format.md)
- [message_error](../message_error/README.md)
- [sgcl::txt::message_format](README.md)
