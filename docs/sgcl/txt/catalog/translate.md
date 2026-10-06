[sgcl](../../README.md) › [txt](../README.md) › [catalog](README.md)

# sgcl::txt::catalog::translate

```cpp
string translate(const string& id) const noexcept;                                         // (1)
string translate(const string& id, const string& context) const noexcept;                  // (2)
string translate(const string& id, const string& plural_id, uint64_t n) const noexcept;    // (3)
string translate(const string& id, const string& plural_id, uint64_t n,
                 const string& context) const noexcept;                                    // (4)
```

Returns a message's translation, what gettext's four calls answer.

1. gettext: the translation of `id`, or `id`. Of a plural, its first form.
2. pgettext: the translation of `id` in `context`, or `id`. An empty context is a context (msgctxt `""`), apart
   from none.
3. ngettext: the form the catalog's rule gives `n` (its first when the translation lacks it); with no
   translation, `id` when `n` is 1 and `plural_id` otherwise.
4. npgettext: the same in `context`.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the message's id, the program's own text |
| `context` | what tells two uses of one id apart |
| `plural_id` | the program's own plural |
| `n` | the number the form is chosen for |

## Return value

The translation: a string the catalog holds, nothing copied.

## Complexity

Constant on average: one hash of `id` (kept in the string after the first) and of `context`.

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
    println("{}", pl.translate("Open"));
    println("{} | {}", pl.translate("File", "menu"), pl.translate("File"));
    for (uint64_t n : {1, 3, 25}) {
        println("{}",
                *txt::format(txt::runtime_pattern(pl.translate("{} file", "{} files", n)), n));
    }
}
```

Output:

```text
Otwórz
Plik | File
1 plik
3 pliki
25 plików
```

## See also

- [contains](contains.md)
- [plural_index](plural_index.md)
- [runtime_pattern](../runtime_pattern/README.md)
- [sgcl::txt::catalog](README.md)
