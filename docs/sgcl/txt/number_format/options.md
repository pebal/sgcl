[sgcl](../../README.md) › [txt](../README.md) › [number_format](README.md)

# sgcl::txt::number_format::options

```cpp
const number_options& options() const noexcept;
```

Returns the options the format was made with, as they were given.

## Parameters

None.

## Return value

The options.

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
    txt::number_format f(txt::locale("en"), {.style = txt::number_style::percent});
    println("{}", f.options().style == txt::number_style::percent);
}
```

Output:

```text
true
```

## See also

- [where](where.md)
- [sgcl::txt::number_format](README.md)
