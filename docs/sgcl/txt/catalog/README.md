[sgcl](../../README.md) › [txt](../README.md)

# sgcl::txt::catalog

```cpp
#include "sgcl/txt/catalog.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class catalog;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::txt::catalog` is one language's translations the way GNU gettext keeps them: read from a .po file (the text
translators edit) or a .mo file (what msgfmt compiles it to), looked up by the program's own text — the message id —
with an optional context, the plural forms chosen by the catalog's own rule, the C expression of its Plural-Forms.
What gettext answers is what it answers, GNU libintl the oracle of its tests: an id with no translation comes back
as it is.

## Rules

- A handle of one word to an immutable table: copies share it, any number of threads read it.
- A translation comes back as a string the catalog holds: [translate](translate.md) allocates nothing.
- Which catalog a locale reads is the program's to choose (a file a language, [best_match](../best_match.md) among
  them); a message with placeholders is written by [format](../format.md) with a
  [runtime_pattern](../runtime_pattern/README.md), or is a [message_format](../message_format/README.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](catalog.md) | constructs the empty catalog |
| [parse_po](parse_po.md) | reads a .po file (static) |
| [parse_mo](parse_mo.md) | reads a .mo file (static) |

#### Lookup

| Function | Description |
|---|---|
| [translate](translate.md) | gettext, pgettext, ngettext, npgettext |
| [contains](contains.md) | checks whether an id has a translation |

#### Observers

| Function | Description |
|---|---|
| [size](size.md) | the messages with a translation |
| [empty](empty.md) | checks whether there is none |
| [header](header.md) | a field of the header entry |
| [plural_forms](plural_forms.md) | the number of plural forms of the rule |
| [plural_index](plural_index.md) | the form the rule gives a number |
| [to_mo](to_mo.md) | the catalog as a .mo file |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

const char *po = R"(msgid ""
msgstr ""
"Language: pl\n"
"Plural-Forms: nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);\n"

msgid "Open"
msgstr "Otwórz"

msgctxt "menu"
msgid "File"
msgstr "Plik"

msgid "{} file"
msgid_plural "{} files"
msgstr[0] "{} plik"
msgstr[1] "{} pliki"
msgstr[2] "{} plików"
)";

int main() {
    auto pl = txt::catalog::parse_po(po).value();
    for (uint64_t n : {1, 3, 25}) {
        println("{}",
                *txt::format(txt::runtime_pattern(pl.translate("{} file", "{} files", n)), n));
    }
}
```

Output:

```text
1 plik
3 pliki
25 plików
```

## See also

- [catalog_error](../catalog_error/README.md)
- [message_format](../message_format/README.md)
- [Benchmarks](../benchmarks.md)
- [sgcl::txt](../README.md)
