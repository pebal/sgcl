[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_highly_restrictive

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool is_highly_restrictive(const string& text) noexcept;
}
```

Checks whether the text stands on the rung `highly_restrictive` of [UTS #39](https://www.unicode.org/reports/tr39/)
§5.2 or a narrower one ([restriction_level_of](restriction_level_of.md)): one script, or the scripts of Japanese,
Chinese or Korean with Latin beside them, every code point allowed. The rung the specification suggests where a
mistaken name costs something. An empty text is not.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`true` when `restriction_level_of(text) <= restriction_level::highly_restrictive`.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (auto s : {"wartość", "変数abc", "abcא", "раypal"}) {
        println("{}: {}", s, txt::is_highly_restrictive(s));
    }
}
```

Output:

```text
wartość: true
変数abc: true
abcא: false
раypal: false
```

## See also

- [is_moderately_restrictive](is_moderately_restrictive.md): the rung above
- [restriction_level_of](restriction_level_of.md): the rung of a text
- [txt](README.md)
