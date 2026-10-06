[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::key_pkcs8

```cpp
secret_bytes key_pkcs8() const;
```

The file's key as its PKCS #8 PrivateKeyInfo, in plain memory: what the typed key of its [kind](key_kind.md) reads, `p256::private_key::from_pkcs8_der(p.key_pkcs8())`, and what [identity](../../net/tls/identity/README.md) and [jose::jwk](../jose-jwk/README.md) take.

## Parameters

None.

## Return value

The DER, in a [secret_bytes](../secret_bytes/README.md).

## Complexity

Linear in the size of the key.

## Exceptions

`std::logic_error` for a file without a key.

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
    auto typed = crypto::p256::private_key::from_pkcs8_der(read->key_pkcs8());
    println("{}", typed->public_key() == key.public_key());
}
```

Output:

```text
true
```

## See also

- [signing_key](signing_key.md)
- [sgcl::crypto::pkcs12](README.md)
