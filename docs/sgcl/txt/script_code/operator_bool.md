[sgcl](../../README.md) › [txt](../README.md) › [script_code](README.md)

# sgcl::txt::script_code::operator bool

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
    println("{} {}", bool(txt::script_code("Latn")), bool(txt::script_code()));
}
```

Output:

```text
true false
```

## See also

- [(constructor)](script_code.md)
- [sgcl::txt::script_code](README.md)
