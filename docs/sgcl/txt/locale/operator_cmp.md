[sgcl](../../README.md) › [txt](../README.md) › [locale](../locale.md)

# sgcl::txt::locale::operator==

```cpp
constexpr bool operator==(const locale&) const noexcept = default;
```

Checks whether two locales are one language: their subtags are equal. `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `(unnamed)` | the other locale |

## Return value

`true` when both name the same language, or both are the root locale.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {}", txt::locale("tr-TR") == txt::locale("tr-CY"),
            txt::locale("pl") != txt::locale("cs"));
}
```

Output:

```text
true true
```

## See also

- [subtag](subtag.md)
- [sgcl::txt::locale](../locale.md)
