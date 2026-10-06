[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::plural_index

```cpp
uint32_t plural_index(uint64_t n) const noexcept;
```

Returns the form the catalog's rule gives `n`: the C expression of its Plural-Forms over unsigned 64-bit numbers, as
libintl evaluates it (`&&`, `||` and `?:` short). A form past nplurals, and a division by zero (a trap in libintl),
give form 0.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the number |

## Return value

The form, from 0.

## Complexity

Linear in the size of the rule.

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
    for (uint64_t n : {1, 2, 5, 22, 112}) {
        print("{} ", pl.plural_index(n));
    }
    println("");
}
```

Output:

```text
0 1 2 1 2 
```

## See also

- [plural_forms](plural_forms.md)
- [translate](translate.md)
- [sgcl::txt::catalog](README.md)
