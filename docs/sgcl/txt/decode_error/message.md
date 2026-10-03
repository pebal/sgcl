[sgcl](../../README.md) › [txt](../README.md) › [decode_error](../decode_error.md)

# sgcl::txt::decode_error::message

```cpp
string message() const noexcept;
```

Returns the error as a sentence: `not` and the name of the encoding ([name_of](../name_of.md)), `not utf-8`. The offset is [offset](offset.md)'s, apart.

## Parameters

None.

## Return value

The message.

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
    byte bytes[] = {byte(0xFF)};
    println("{}", txt::decode(bytes, txt::encoding::utf8, txt::strict).error().message());
}
```

Output:

```text
not utf-8
```

## See also

- [offset](offset.md)
- [sgcl::txt::decode_error](../decode_error.md)
