[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::header

```cpp
string header(const string& field) const noexcept;
```

Returns a field of the header entry (`"Language"`, `"Plural-Forms"`, `"Content-Type"`), its name in any case, the
value without the blanks at its ends.

## Parameters

| Parameter | Description |
|---|---|
| `field` | the field's name |

## Return value

The value; an empty text when the catalog has no such field.

## Complexity

Linear in the length of the header.

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
    println("{} | {}", pl.header("language"), pl.header("Plural-Forms"));
}
```

Output:

```text
pl | nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);
```

## See also

- [plural_forms](plural_forms.md)
- [sgcl::txt::catalog](README.md)
