[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [public_key](../ed25519-public_key.md)

# sgcl::crypto::ed25519::public_key::to_pkix_der

```cpp
vector<byte> to_pkix_der() const noexcept;
```

Returns the key as a SubjectPublicKeyInfo (RFC 8410, OID 1.3.101.112), 44 bytes, byte for byte as OpenSSL and Go's
`x509.MarshalPKIXPublicKey` write it: what a certificate, an SSH tool or a file of public keys holds. A public key is
not a secret, so the DER is a managed `vector<byte>`.

## Parameters

None.

## Return value

The DER, 44 bytes.

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
    // RFC 8032's TEST 1 key
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    auto der = key->public_key().to_pkix_der();
    println("{}", encoding::hex::encode(der));
    print("{}", encoding::pem("PUBLIC KEY", der).to_string());
}
```

Output:

```text
302a300506032b6570032100d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
-----BEGIN PUBLIC KEY-----
MCowBQYDK2VwAyEA11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=
-----END PUBLIC KEY-----
```

## See also

- [from_pkix_der](from_pkix_der.md): the reverse
- [bytes](bytes.md): the 32 bytes alone
- [sgcl::crypto::ed25519::public_key](../ed25519-public_key.md)
