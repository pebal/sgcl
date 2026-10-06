[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::plural_forms

```cpp
uint32_t plural_forms() const noexcept;
```

Returns the number of plural forms the catalog's rule has (nplurals), 2 without a rule.

## Parameters

None.

## Return value

The number of forms.

## Complexity

Constant.

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
    println("{} {}", pl.plural_forms(), txt::catalog().plural_forms());
}
```

Output:

```text
3 2
```

## See also

- [plural_index](plural_index.md)
- [sgcl::txt::catalog](README.md)
