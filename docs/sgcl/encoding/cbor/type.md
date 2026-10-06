[sgcl](../../README.md) › [encoding](../README.md) › [cbor](README.md)

# sgcl::encoding::cbor::type

```cpp
kind type() const noexcept;
```

The [kind](../cbor-kind.md) of the value.

## Parameters

None.

## Return value

The kind.

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
    encoding::cbor c = encoding::cbor::array({1, 1.0, "1", encoding::cbor::bytes(vector<byte>(1))});
    for (const auto& e : c.elements()) {
        println("{} {}", e.to_string(), int(e.type()));
    }
}
```

Output:

```text
1 3
1.0 4
"1" 6
h'00' 5
```

## See also

- [cbor::kind](../cbor-kind.md)
- [sgcl::encoding::cbor](README.md)
