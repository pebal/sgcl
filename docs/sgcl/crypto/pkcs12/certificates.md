[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::certificates

```cpp
const x509::chain& certificates() const noexcept;
```

The file's certificates in the chain's order: the leaf (the key's) first, then each one's issuer the file holds, then the rest as the file holds them.

## Parameters

None.

## Return value

The chain; empty for a file without certificates.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // a key and its certificate, written to a file under a password
    auto key = crypto::p256::private_key::generate();
    crypto::x509::certificate_template t;
    t.common_name = "example.test";
    crypto::x509::chain chain;
    chain.push_back(crypto::x509::create_certificate(t, key));
    auto file = crypto::pkcs12::encode(key, chain, "password", {.friendly_name = "example"});
    auto read = crypto::pkcs12::parse(file, "password");
    println("{}", read->certificates()[0].subject().to_string());
}
```

Output:

```text
CN=example.test
```

## See also

- [signing_key](signing_key.md)
- [sgcl::crypto::pkcs12](README.md)
