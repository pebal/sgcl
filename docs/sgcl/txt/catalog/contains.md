[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::contains

```cpp
bool contains(const string& id) const noexcept;                           // (1)
bool contains(const string& id, const string& context) const noexcept;    // (2)
```

Checks whether `id`, with no context (1) or in `context` (2), has a translation here.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the message's id |
| `context` | its context |

## Return value

`true` when it has one.

## Complexity

Constant on average.

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
    println("{} {} {}", pl.contains("Open"), pl.contains("File"), pl.contains("File", "menu"));
}
```

Output:

```text
true false true
```

## See also

- [translate](translate.md)
- [sgcl::txt::catalog](README.md)
