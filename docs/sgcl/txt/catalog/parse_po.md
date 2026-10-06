[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::parse_po

```cpp
static expected<catalog, catalog_error> parse_po(const string& text) noexcept;
```

Reads a .po file, as GNU gettext writes it: its messages (with msgctxt, msgid_plural and msgstr[n]), the header
entry and the rule of its Plural-Forms. The strings are C strings across lines, with C's escapes; a byte-order mark,
CRLF lines, comments, flags and obsolete `#~` entries are read. An entry marked `fuzzy` or with an empty first form
is not a translation, as msgfmt leaves it out (the header is read even when fuzzy). The bytes are kept as they are,
whatever the header's charset (UTF-8 expected).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text of the .po file |

## Return value

The catalog, or a [catalog_error](../catalog_error/README.md) where msgfmt refuses the file: a string or an escape
that is not one (a NUL among them), a keyword out of place, a message defined twice (translated or not), a plural
without its forms or with them out of order, a translation whose line feeds at either end are not its id's, a byte 4
in an id or a context (a .mo keeps it for the context), and, as `msgfmt --check` refuses it, a Plural-Forms that
does not parse.

## Complexity

Linear in the length of the text, and n log n in the number of messages (the check of messages defined twice).

## Exceptions

None that the program can cause; running out of memory ends it.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto text : {"msgid \"a\"\nmsgstr \"A\"\n", "msgid \"a\"\nmsgstr \"A\"\nmsgstr \"B\"\n"}) {
        auto c = txt::catalog::parse_po(text);
        if (c) {
            println("{} message, a is {}", c->size(), c->translate("a"));
        } else {
            println("line {}, column {}: {}", c.error().line(), c.error().column(),
                    c.error().message());
        }
    }
}
```

Output:

```text
1 message, a is A
line 3, column 1: msgstr twice
```

## See also

- [parse_mo](parse_mo.md)
- [to_mo](to_mo.md)
- [catalog_error](../catalog_error/README.md)
- [sgcl::txt::catalog](README.md)
