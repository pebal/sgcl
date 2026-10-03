[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::is_space

```cpp
#include "sgcl/txt/properties.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    inline constexpr auto is_space = unicode::is_space;   // called as is_space(c)
}
```

Core's [unicode::is_space](../core/unicode/is_space.md) under this module's name, so that `txt::is_space` stands beside `txt::is_alpha`: the same object, called as a function, `txt::is_space(c)`, and passed as a predicate. It checks whether the code point `c` has the White_Space property: the six of the C locale, U+0085, U+00A0, U+1680, U+2000 to U+200A, U+2028, U+2029, U+202F, U+205F and U+3000; not the zero-width space; a `char`, an `int` or another character type is refused.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the code point |

## Return value

`true` when `c` has the White_Space property.

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
    println("{} {} {}", txt::is_space(U' '), txt::is_space(char32_t(0x3000)),
            txt::is_space(char32_t(0x200B)));
}
```

Output:

```text
true true false
```

## See also

- [unicode::is_space](../core/unicode/is_space.md)
- [sgcl::txt](README.md)
