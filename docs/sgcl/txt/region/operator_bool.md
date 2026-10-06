[sgcl](../../README.md) › [txt](../README.md) › [region](README.md)

# sgcl::txt::region::operator bool

```cpp
constexpr explicit operator bool() const noexcept;
```

Checks whether the object holds a code.

## Parameters

None.

## Return value

`false` for a default-constructed one.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"
#include "sgcl/txt/names/de.h"
#include "sgcl/txt/names/pl.h"

using namespace sgcl;

int main() {
    println("{} {}", bool(txt::region("DE")), bool(txt::region()));
}
```

Output:

```text
true false
```

## See also

- [(constructor)](region.md)
- [sgcl::txt::region](README.md)
