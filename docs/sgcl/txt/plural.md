[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::plural

```cpp
#include "sgcl/txt/plural.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    enum class plural : uint8_t {
        zero,
        one,
        two,
        few,
        many,
        other,
    };
}
```

The form of a word a number takes in a language: CLDR's plural categories (TR35 Part 3 §5). A language uses some of
them and always `other` — English `one` and `other`, Polish `one`, `few`, `many` and `other`, Arabic all six — and
what decides is the number as it is written: in English "1 file" is `one` and "1.0 files" `other`.
[plural_of](plural_of.md) and [ordinal_of](ordinal_of.md) answer it.

| Value | Description |
|---|---|
| `zero` | Arabic 0, Welsh 0, Latvian 0 and 10–20 |
| `one` | English 1, Polish 1, French 0 and 1 |
| `two` | Arabic 2, Slovenian 2 and 102, Welsh 2; the English ordinal 2nd |
| `few` | Polish 2–4 and 22–24, Czech 2–4; the English ordinal 3rd |
| `many` | Polish 5–21 and 25–31, Russian 5–20, French a million and more |
| `other` | everything else; the only one a language without forms (Japanese, Chinese) uses |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    auto pl = txt::locale("pl");
    for (int n : {1, 2, 5, 22, 25}) {
        println("{} {}", n, int(txt::plural_of(n, pl)));
    }
}
```

Output:

```text
1 1
2 3
5 4
22 3
25 4
```

## See also

- [plural_of](plural_of.md)
- [ordinal_of](ordinal_of.md)
- [sgcl::txt](README.md)
