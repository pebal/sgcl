[sgcl](../../README.md) › [crypto](../README.md) › [hkdf](../hkdf/README.md) › [prk](README.md)

# sgcl::crypto::hkdf\<H\>::prk::bytes

```cpp
slice<const byte> bytes() const noexcept;
```

The key's bytes, where they lie in this object: no copy, valid while the object lives. For a protocol that writes the
PRK down or feeds it on — TLS 1.3 takes one secret from another — and for [expand](../hkdf/expand.md) (2), which
takes a PRK as bytes. The slice holds no owner: it must not outlive the object, which zeroes the bytes when it dies or
is moved from.

## Parameters

None.

## Return value

A slice of `size` bytes, the key.

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
    // RFC 5869, test case 1
    vector<byte> ikm(22, byte(0x0b));
    auto salt = encoding::hex::decode("000102030405060708090a0b0c");
    auto master = crypto::hkdf_sha256::extract(salt, ikm);
    println(encoding::hex::encode(master.bytes()));
}
```

Output:

```text
077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5
```

## See also

- [expand](../hkdf/expand.md): takes the PRK, as the object or as bytes
- [sgcl::crypto::hkdf\<H\>::prk](README.md)
