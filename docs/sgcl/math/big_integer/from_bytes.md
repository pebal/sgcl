[sgcl](../../README.md) › [math](../README.md) › [big_integer](README.md)

# sgcl::math::big_integer::from_bytes

```cpp
static big_integer from_bytes(const slice<const byte>& big_endian) noexcept;
```

The unsigned number the bytes write, most significant first, as Go's `SetBytes`: leading zero bytes add nothing,
and no bytes are zero. The bytes are never read as a sign; the two's complement form of ASN.1's INTEGER is the
business of `encoding`. [to_bytes](to_bytes.md) writes the bytes back.

## Parameters

| Parameter | Description |
|---|---|
| `big_endian` | the bytes, most significant first: a `vector<byte>`, an array of bytes, a slice of either |

## Return value

The number, never negative.

## Complexity

Linear in the number of bytes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/math.h"

using namespace sgcl;

int main() {
    vector<byte> bytes = {byte{0x00}, byte{0x01}, byte{0x00}, byte{0xff}};
    println("{}", math::big_integer::from_bytes(bytes));
    println("{}", math::big_integer::from_bytes(vector<byte>()));

    auto n = math::big_integer(1) << 100;
    println("{}", math::big_integer::from_bytes(n.to_bytes()) == n);
}
```

Output:

```text
65791
0
true
```

## See also

- [to_bytes](to_bytes.md): the bytes of a number
- [parse](parse.md): a number from text
- [sgcl::math::big_integer](README.md)
