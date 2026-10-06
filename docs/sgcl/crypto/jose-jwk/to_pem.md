[sgcl](../../README.md) › [crypto](../README.md) › [jose](../jose.md) › [jwk](README.md)

# sgcl::crypto::jose::jwk::to_pem

```cpp
secret_bytes to_pem() const;
```

A private key's PKCS #8 in PEM (`PRIVATE KEY`), in plain memory, as the key types' own `to_pem` write it.

## Parameters

None.

## Return value

The PEM, in a secret_bytes.

## Complexity

Linear in the size of the key.

## Exceptions

`std::logic_error` for a public key and an oct key.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::eddsa);
    auto pem = key.to_pem();
    println("{}", crypto::jose::jwk::from_pem(pem)->thumbprint() == key.thumbprint());
}
```

Output:

```text
true
```

## See also

- [from_pem](from_pem.md): the other way
- [sgcl::crypto::jose::jwk](README.md)
