[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](../hkdf.md) › [prk](../hkdf-prk.md)

# sgcl::crypto::hkdf\<H\>::prk::operator=

```cpp
prk& operator=(prk&& other) noexcept;    // (1)
prk& operator=(const prk&) = delete;     // (2)
```

1. Takes the key's bytes from `other` in place of this object's own and zeroes them in `other`. An assignment to itself
   changes nothing.
2. No copy: a second object with the same key is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the PRK moved from |

## Return value

`*this`.

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
    auto current = crypto::hkdf_sha256::extract("an old salt", ikm);
    current = crypto::hkdf_sha256::extract("", ikm);  // RFC 5869, test case 3
    println(encoding::hex::encode(current.bytes()));
}
```

Output:

```text
19ef24a32c717b167f33a91d6f648bdf96596776afdb6377ac434c1c293ccb04
```

## See also

- [(constructor)](hkdf-prk.md): the move constructor
- [sgcl::crypto::hkdf\<H\>::prk](../hkdf-prk.md)
