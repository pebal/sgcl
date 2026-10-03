[sgcl](../../README.md) › [encoding](../README.md) › [pem](../pem.md)

# sgcl::encoding::pem::bytes

```cpp
const vector<byte>& bytes() const noexcept;
```

The bytes the block's base64 holds, decoded: the DER of a certificate or a key, as the program gives it to the
reader of that format. Go's `Block.Bytes`. They are in a managed [vector](../../core/vector.md), which is no place
for a secret: a private key's PEM is read by the `from_pem` of the crypto module's keys.

## Parameters

None.

## Return value

The bytes.

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
    auto block = encoding::pem::parse("-----BEGIN DATA-----\nAQID/w==\n-----END DATA-----\n");
    for (byte b : block->bytes()) {
        print("{} ", int(b));
    }
    println();
}
```

Output:

```text
1 2 3 255 
```

## See also

- [type](type.md): what the bytes are
- [base64](../base64.md): the encoding of the bytes in the text
- [sgcl::encoding::pem](../pem.md)
