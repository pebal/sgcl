[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::dotted_i

```cpp
constexpr bool dotted_i() const noexcept;
```

Checks whether the language writes an `i` the Turkish way, the one question the case mappings ask of a language beside the Lithuanian dot: Turkish or Azerbaijani.

## Parameters

None.

## Return value

`true` for Turkish and Azerbaijani.

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
    for (auto tag : {"tr", "az", "lt", "pl"}) {
        println("{} {}", tag, txt::locale(tag).dotted_i());
    }
}
```

Output:

```text
tr true
az true
lt false
pl false
```

## See also

- [keeps_dot](keeps_dot.md)
- [sgcl::txt::locale](README.md)
