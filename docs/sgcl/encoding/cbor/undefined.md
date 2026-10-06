[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::undefined

```cpp
static cbor undefined() noexcept;
```

undefined, `f7`: a value that is not there, which JSON has no form for.

## Parameters

None.

## Return value

The value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {}", encoding::cbor::undefined().to_string(), encoding::hex::encode(encoding::cbor::undefined().to_bytes()));
}
```

Output:

```text
undefined f7
```

## See also

- [type](type.md)
- [sgcl::encoding::cbor](README.md)
