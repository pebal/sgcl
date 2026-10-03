[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [public_key](../x25519-public_key.md)

# sgcl::crypto::x25519::public_key::to_pkix_der

```cpp
vector<byte> to_pkix_der() const noexcept;
```

Returns the key as a SubjectPublicKeyInfo (RFC 8410, OID 1.3.101.110), 44 bytes, byte for byte as OpenSSL and Go's
`x509.MarshalPKIXPublicKey` write it: what a certificate or a file of public keys holds. A public key is not a
secret, so the DER is a managed `vector<byte>`.

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
    // RFC 7748 §6.1: Alice's private key and the public key it gives
    auto alice = crypto::x25519::private_key::from_bytes(
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"));
    auto der = alice->public_key().to_pkix_der();
    println("{}", encoding::hex::encode(der));
    print("{}", encoding::pem("PUBLIC KEY", der).to_string());
}
```

Output:

```text
302a300506032b656e0321008520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a
-----BEGIN PUBLIC KEY-----
MCowBQYDK2VuAyEAhSDwCYkwp1R0i33ctD73Wg2/Og0mOBr066SpjqqbTmo=
-----END PUBLIC KEY-----
```

## See also

- [from_pkix_der](from_pkix_der.md): the reverse
- [bytes](bytes.md): the 32 bytes alone
- [sgcl::crypto::x25519::public_key](../x25519-public_key.md)
