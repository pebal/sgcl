[sgcl](../../README.md) › [crypto](../README.md) › [p256](../p256.md) › [public_key](README.md)

# sgcl::crypto::p256::public_key::bytes_compressed

```cpp
array<byte, compressed_size> bytes_compressed() const noexcept;
```

The point in SEC 1's compressed form, `02` or `03` (the parity of Y) `‖ X`, Go's `elliptic.MarshalCompressed`: half
the size of the uncompressed form, and read back by [from_bytes](from_bytes.md), which computes Y. `compressed_size`
is 33 on P-256; `p384::public_key::bytes_compressed` gives 49 bytes.

## Parameters

None.

## Return value

The `compressed_size` bytes of the point, in an array.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::p256::public_key::from_bytes(encoding::hex::decode(
        "0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
        "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299"));
    auto compressed = key->bytes_compressed();
    println("{}", encoding::hex::encode(compressed));
    println("{}", crypto::p256::public_key::from_bytes(compressed) == key);
}
```

Output:

```text
0360fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6
true
```

## See also

- [bytes](bytes.md): the uncompressed form
- [sgcl::crypto::p256::public_key](README.md)
