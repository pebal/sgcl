[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_upper

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr auto is_upper = unicode::is_upper;   // called as is_upper(c)
}
```

Core's [unicode::is_upper](../core/unicode/is_upper.md) under this module's name, so that `txt::is_upper` stands beside `txt::is_alpha`: the same object, called as a function, `txt::is_upper(c)`, and passed as a predicate. It checks whether the code point `c` has a lower case form other than itself, by the simple mapping; a `char`, an `int` or another character type is refused.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` has a lower case form other than itself, by the simple mapping.

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
    println("{} {} {}", txt::is_upper(U'Ł'), txt::is_upper(U'ł'), txt::is_upper(U'1'));
}
```

Output:

```text
true false false
```

## See also

- [unicode::is_upper](../core/unicode/is_upper.md)
- [sgcl::txt](README.md)
