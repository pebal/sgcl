[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_lower

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr auto is_lower = unicode::is_lower;   // called as is_lower(c)
}
```

Core's [unicode::is_lower](../core/unicode/is_lower.md) under this module's name, so that `txt::is_lower` stands beside `txt::is_alpha`: the same object, called as a function, `txt::is_lower(c)`, and passed as a predicate. It checks whether the code point `c` has an upper case form other than itself, by the simple mapping: `ß` has none, its upper case `SS` being two letters; a `char`, an `int` or another character type is refused.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` has an upper case form other than itself, by the simple mapping.

## Complexity

Constant: a test for ASCII, otherwise a lookup in a table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{} {} {}", txt::is_lower(U'ł'), txt::is_lower(U'Ł'), txt::is_lower(U'ß'));
}
```

Output:

```text
true false false
```

## See also

- [unicode::is_lower](../core/unicode/is_lower.md)
- [sgcl::txt](README.md)
