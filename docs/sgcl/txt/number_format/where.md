[sgcl](../../README.md) › [txt](../README.md) › [number_format](README.md)

# sgcl::txt::number_format::where

```cpp
txt::locale where() const noexcept;
```

Returns the locale the format was made for.

## Parameters

None.

## Return value

The locale.

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
    println("{}", txt::number_format(txt::locale("pl-PL")).where().to_string());
}
```

Output:

```text
pl-PL
```

## See also

- [options](options.md)
- [sgcl::txt::number_format](README.md)
