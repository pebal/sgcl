[sgcl](../../README.md) › [txt](../README.md) › [message_error](README.md)

# sgcl::txt::message_error::offset

```cpp
size_t offset() const noexcept;
```

Returns the byte the reading stopped on.

## Parameters

None.

## Return value

The offset, from 0.

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
    auto m = txt::message_format::parse("{n, plural, one {#}}", txt::locale("en"));
    println("{}", m.error().offset());
}
```

Output:

```text
19
```

## See also

- [message_format::parse](../message_format/parse.md)
- [sgcl::txt::message_error](README.md)
