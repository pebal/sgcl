[sgcl](../../README.md) › [txt](../README.md) › [runtime_pattern](README.md)

# sgcl::txt::runtime_pattern::text

```cpp
const string& text() const noexcept;
```

The string the pattern keeps, the one it was made of: for a program that logs a translation which did not fit, or
keeps the pattern beyond it.

## Parameters

None.

## Return value

The string of the pattern.

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
    auto pattern = txt::runtime("{} of {}");
    if (!txt::format(pattern, 7)) {
        println("does not fit one value: {:?}", pattern.text());
    }
    return 0;
}
```

Output:

```text
does not fit one value: "{} of {}"
```

## See also

- [view](view.md): the characters as a view
- [sgcl::txt::runtime_pattern](README.md)
