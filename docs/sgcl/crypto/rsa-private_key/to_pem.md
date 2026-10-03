[sgcl](../../README.md) › [crypto](../README.md) › [rsa](../rsa.md) › [private_key](../rsa-private_key.md)

# sgcl::crypto::rsa::private_key::to_pem

```cpp
secret_bytes to_pem() const;
```

Writes the key as PEM, a `PRIVATE KEY` block over its PKCS #8, as Go's `pem.Encode` of
`x509.MarshalPKCS8PrivateKey` and OpenSSL's `genpkey` write it: a [secret_bytes](../secret_bytes.md), never managed
memory, for the program to write to a file of its own.

## Parameters

None.

## Return value

The PEM text, lines of 64 characters between the `BEGIN` and `END` lines.

## Complexity

Linear in the bits of the modulus.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::rsa::private_key::generate(2048);
    auto pem = key.to_pem();
    auto again = crypto::rsa::private_key::from_pem(pem);
    println("{}", again->public_key() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [from_pem](from_pem.md): reads it back
- [sgcl::crypto::rsa::private_key](../rsa-private_key.md)
