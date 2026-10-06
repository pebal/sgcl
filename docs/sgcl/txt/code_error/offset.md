[sgcl](../../README.md) › [txt](../README.md) › [code_error](README.md)

# sgcl::txt::code_error::offset

```cpp
constexpr size_t offset() const noexcept;
```

Returns the byte the reading stopped on: the first that is not a letter (or a digit, for a region), or where the
text ended or should have ended.

## Parameters

None.

## Return value

The offset in bytes from the start of the text.

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
    for (auto code : {"P1N", "PL", "PLNX"}) {
        println("{} {}", code, txt::currency::parse(code).error().offset());
    }
}
```

Output:

```text
P1N 1
PL 2
PLNX 3
```

## See also

- [what](what.md)
- [sgcl::txt::code_error](README.md)
