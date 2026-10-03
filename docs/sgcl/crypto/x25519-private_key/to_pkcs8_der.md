[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](README.md)

# sgcl::crypto::x25519::private_key::to_pkcs8_der

```cpp
secret_bytes to_pkcs8_der() const;
```

Returns the key as a PKCS #8 PrivateKeyInfo (RFC 8410, OID 1.3.101.110), 48 bytes, version 0, byte for byte as Go's
`x509.MarshalPKCS8PrivateKey` and OpenSSL write it. It holds the secret, so it comes as a
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
    // RFC 7748 §6.1's private key of Alice: the DER is a fixed header and the 32 bytes
    auto given =
        encoding::hex::decode("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
    auto alice = crypto::x25519::private_key::from_bytes(given);
    crypto::secret_bytes der = alice->to_pkcs8_der();

    auto header = encoding::hex::decode("302e020100300506032b656e04220420");
    println("{}", der.size());
    println("{}", crypto::constant_time::equal(der.as_slice().subslice(0, 16), header));
    println("{}", crypto::constant_time::equal(der.as_slice().subslice(16), given));
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
- [sgcl::crypto::x25519::private_key](README.md)
