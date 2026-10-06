[sgcl](../../README.md) › [encoding](../README.md) › [asn1](README.md)

# sgcl::encoding::asn1::boolean

```cpp
static asn1 boolean(bool value) noexcept;
```

A BOOLEAN: the byte `FF` for `true` and `00` for `false`, as DER writes them.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value |

## Return value

The element, three bytes.

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
    for (bool b : {true, false}) {
        encoding::asn1 e = encoding::asn1::boolean(b);
        println("{} {}", encoding::hex::encode(e.bytes()), *e.as_bool());
    }
}
```

Output:

```text
0101ff true
010100 false
```

## See also

- [as_bool](as_bool.md): the value read
- [sgcl::encoding::asn1](README.md)
