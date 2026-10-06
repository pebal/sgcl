[sgcl](../../README.md) › [io](../README.md) › [glob_pattern](README.md)

# sgcl::io::glob_pattern::text

```cpp
const string& text() const noexcept;
```

The pattern as it was given, its braces unexpanded.

## Parameters

None.

## Return value

The text of the pattern.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::glob_pattern p("docs/**/*.{md,txt}");
    println("{}", p.text());
}
```

Output:

```text
docs/**/*.{md,txt}
```

## See also

- [sgcl::io::glob_pattern](README.md)
