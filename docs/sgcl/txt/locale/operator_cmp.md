[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::operator==

```cpp
constexpr bool operator==(const locale&) const noexcept = default;
```

Checks whether two locales are one locale: the same language, script and region, and both or neither asking for the
Latin digits. `!=` is its negation. Two locales of one language in two countries are two locales — `de-CH` and
`de-DE` write numbers differently — and what asks about the language alone asks [subtag](subtag.md),
[dotted_i](dotted_i.md) or [keeps_dot](keeps_dot.md).

## Parameters

None.

## Return value

`true` when both are the same locale, or both the root locale.

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
    println("{} {} {}", txt::locale("tr-TR") == txt::locale("tr_TR.UTF-8"),
            txt::locale("tr-TR") == txt::locale("tr-CY"),
            txt::locale("tr-TR").subtag() == txt::locale::turkish().subtag());
}
```

Output:

```text
true false true
```

## See also

- [subtag](subtag.md)
- [sgcl::txt::locale](README.md)
