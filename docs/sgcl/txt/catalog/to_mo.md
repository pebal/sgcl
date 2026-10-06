[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::to_mo

```cpp
vector<byte> to_mo() const noexcept;
```

Returns the catalog as msgfmt writes it: a .mo of revision 0 in this machine's byte order, the messages sorted by id
(context first), gettext's hash table after the tables. For a catalog read from a .po the bytes are msgfmt's own,
which the tests hold against GNU gettext 1.0.

## Parameters

None.

## Return value

The bytes of the .mo file.

## Complexity

n log n in the number of messages.

## Exceptions

None.

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
    auto mo = pl.to_mo();
    println("{} bytes, {}", mo.size(), txt::catalog::parse_mo(mo)->translate("Open"));
}
```

Output:

```text
301 bytes, Otwórz
```

## See also

- [parse_mo](parse_mo.md)
- [sgcl::txt::catalog](README.md)
