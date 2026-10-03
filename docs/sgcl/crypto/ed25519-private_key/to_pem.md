[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](README.md)

# sgcl::crypto::ed25519::private_key::to_pem

```cpp
secret_bytes to_pem() const;
```

Returns the key as PEM: a `PRIVATE KEY` block over its PKCS #8, the base64 in lines of 64 characters and `"\n"` after
each line, byte for byte as Go's `pem.Encode` of `x509.MarshalPKCS8PrivateKey` and OpenSSL's `genpkey` write it.
It holds the seed, so it is written straight into a [secret_bytes](../secret_bytes/README.md) of the exact size, never
into managed memory.

## Parameters

None.

## Return value

The PEM text, 119 bytes.

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // the Ed25519 key of the tree's TLS tests, written back as it was read
    crypto::secret_bytes file = crypto::read_secret("tests/net/tls_testdata/ed25519.key");
    auto key = crypto::ed25519::private_key::from_pem(file);
    crypto::secret_bytes pem = key->to_pem();
    println("{} {}", pem.size(), pem == file);
}
```

Output:

```text
119 true
```

## See also

- [from_pem](from_pem.md): the reverse
- [to_pkcs8_der](to_pkcs8_der.md): the DER inside
- [sgcl::crypto::ed25519::private_key](README.md)
