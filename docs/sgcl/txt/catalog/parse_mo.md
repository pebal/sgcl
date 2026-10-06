[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::parse_mo

```cpp
static expected<catalog, catalog_error> parse_mo(const slice<const byte>& data) noexcept;
```

Reads a .mo file, as msgfmt writes it: either byte order, revision 0 or 1 (a system-dependent string of revision 1
is not read). Its own hash table is not used. A key held twice keeps one of the two; a Plural-Forms that does not
parse is the Germanic rule, as libintl takes it.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the .mo file |

## Return value

The catalog, or a [catalog_error](../catalog_error/README.md) (its line and column 0) when the magic number or the
revision is not one this reads, or a table or a string reaches past the end.

## Complexity

n log n in the number of messages, and linear in the size of the file.

## Exceptions

None that the program can cause; running out of memory ends it.

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
    auto mo = txt::catalog::parse_po(po)->to_mo();
    auto c = txt::catalog::parse_mo(mo);
    println("{} | {}", c->translate("Open"), c->translate("{} file", "{} files", 5));
    auto bad = txt::catalog::parse_mo(slice<const byte>(mo.data(), 10));
    println("{}", bad.error().message());
}
```

Output:

```text
Otwórz | {} plików
too short for a .mo header
```

## See also

- [parse_po](parse_po.md)
- [to_mo](to_mo.md)
- [sgcl::txt::catalog](README.md)
