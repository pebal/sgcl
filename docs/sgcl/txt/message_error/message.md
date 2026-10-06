[sgcl](../../README.md) › [txt](../README.md) › [message_error](README.md)

# sgcl::txt::message_error::message

```cpp
string message() const noexcept;
```

Returns why, in a few words.

## Parameters

None.

## Return value

The reason.

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
    println("{}", m.error().message());
}
```

Output:

```text
a plural or select argument without its other case
```

## See also

- [message_format::parse](../message_format/parse.md)
- [sgcl::txt::message_error](README.md)
