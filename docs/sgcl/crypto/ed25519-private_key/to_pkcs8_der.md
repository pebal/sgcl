[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](README.md)

# sgcl::crypto::ed25519::private_key::to_pkcs8_der

```cpp
secret_bytes to_pkcs8_der() const;
```

Returns the key as a PKCS #8 PrivateKeyInfo (RFC 8410, OID 1.3.101.112), the seed inside, 48 bytes, version 0, byte
for byte as Go's `x509.MarshalPKCS8PrivateKey` and OpenSSL write it. It holds the seed, so it comes as a
[secret_bytes](../secret_bytes/README.md), 48 bytes in the object itself, never in managed memory.

## Parameters

None.

## Return value

The DER, 48 bytes.

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 seed: the DER is a fixed header and the seed
    auto seed =
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    auto key = crypto::ed25519::private_key::from_seed(seed);
    crypto::secret_bytes der = key->to_pkcs8_der();

    auto header = encoding::hex::decode("302e020100300506032b657004220420");
    println("{}", der.size());
    println("{}", crypto::constant_time::equal(der.as_slice().subslice(0, 16), header));
    println("{}", crypto::constant_time::equal(der.as_slice().subslice(16), seed));
}
```

Output:

```text
48
true
true
```

## See also

- [from_pkcs8_der](from_pkcs8_der.md): the reverse
- [to_pem](to_pem.md): the same in PEM
- [sgcl::crypto::ed25519::private_key](README.md)
