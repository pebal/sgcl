[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](../hkdf.md) › [prk](../hkdf-prk.md)

# sgcl::crypto::hkdf\<H\>::prk::prk

```cpp
prk(prk&& other) noexcept;    // (1)
prk(const prk&) = delete;     // (2)
```

1. Takes the key's bytes from `other` and zeroes them there. The object moved from holds zeros, a key of no use.
2. No copy: a second object with the same key is [clone](clone.md).

A PRK is made by [extract](../hkdf/extract.md) only; its other constructor is private.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the PRK moved from |

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
    vector<byte> ikm(22, byte(0x0b));
    auto master = crypto::hkdf_sha256::extract("", ikm);  // RFC 5869, test case 3
    crypto::hkdf_sha256::prk moved = std::move(master);
    println(encoding::hex::encode(moved.bytes()));
}
```

Output:

```text
19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04
```

## See also

- [operator=](operator_assign.md): the move assignment
- [clone](clone.md): a second object with the same key
- [sgcl::crypto::hkdf\<H\>::prk](../hkdf-prk.md)
