[sgcl](../../README.md) › [io](../README.md) › [glob_pattern](README.md)

# sgcl::io::glob_pattern::is_literal

```cpp
bool is_literal() const noexcept;
```

Checks whether the pattern names one path only: no wildcard (`*`, `?`, `[`, `**`), no escape and no braces that
expand. A program that takes paths and patterns alike opens a literal one directly, without a walk.

## Parameters

None.

## Return value

`true` when the pattern is a plain path.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    for (const char* text : {"etc/hosts", "etc/*.conf", "etc/{a,b}", "etc/{a}"}) {
        println("{} {}", text, io::glob_pattern(text).is_literal());
    }
}
```

Output:

```text
etc/hosts true
etc/*.conf false
etc/{a,b} false
etc/{a} true
```

## See also

- [text](text.md): the pattern as given
- [sgcl::io::glob_pattern](README.md)
