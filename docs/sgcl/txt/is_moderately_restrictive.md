[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_moderately_restrictive

```cpp
#include "sgcl/txt/identifier.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    bool is_moderately_restrictive(const string& text) noexcept;
}
```

Checks whether the text stands on the rung `moderately_restrictive` of
[UTS #39](https://www.unicode.org/reports/tr39/) §5.2 or a narrower one
([restriction_level_of](restriction_level_of.md)): what [is_highly_restrictive](is_highly_restrictive.md) accepts,
and Latin with one other recommended script that is not Cyrillic, Greek or Cherokee. The rung the specification
suggests for a registry open to the world. An empty text is not.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text, UTF-8 |

## Return value

`true` when `restriction_level_of(text) <= restriction_level::moderately_restrictive`.

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
    for (auto s : {"wartość", "abcא", "abcΑ", "раypal"}) {
        println("{}: {}", s, txt::is_moderately_restrictive(s));
    }
}
```

Output:

```text
wartość: true
abcא: true
abcΑ: false
раypal: false
```

## See also

- [is_highly_restrictive](is_highly_restrictive.md): the rung below
- [restriction_level_of](restriction_level_of.md): the rung of a text
- [txt](README.md)
