[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::signing_key

```cpp
x509::signing_key signing_key() const;
```

The file's key as the module signs with it, an [x509::signing_key](../x509-signing_key/README.md): what [x509::create_certificate](../x509-create_certificate.md), [cms::sign](../cms-sign.md) and [pkcs12::encode](encode.md) take, in one line. A view of the key the handle holds, valid while it or a copy lives.

## Parameters

None.

## Return value

The view.

## Complexity

Constant.

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
    // the file's key signs a certificate of its own
    crypto::x509::certificate_template u;
    u.common_name = "again";
    auto again = crypto::x509::create_certificate(u, read->signing_key());
    println("{}", again.subject().to_string());
}
```

Output:

```text
CN=again
```

## See also

- [key_pkcs8](key_pkcs8.md)
- [sgcl::crypto::pkcs12](README.md)
