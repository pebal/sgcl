[sgcl](../../README.md) › [txt](../README.md) › [decode_error](../decode_error.md)

# sgcl::txt::decode_error::from

```cpp
encoding from() const noexcept;
```

Returns the encoding the bytes were decoded from.

## Parameters

None.

## Return value

The [encoding](../encoding.md).

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
    byte bytes[] = {byte(0x98)};
    auto e = txt::decode(bytes, txt::encoding::windows1251, txt::strict).error();
    println("{}", e.from() == txt::encoding::windows1251);
}
```

Output:

```text
true
```

## See also

- [offset](offset.md)
- [sgcl::txt::decode_error](../decode_error.md)
