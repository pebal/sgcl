[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::friendly_name

```cpp
string friendly_name() const noexcept;
```

The friendlyName of the key's bag, else of the leaf's: the name a key store lists the entry under (`openssl pkcs12 -name`).

## Parameters

None.

## Return value

The name; empty when the file names none.

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
    println("{}", crypto::pkcs12::parse(file, "password")->friendly_name());
}
```

Output:

```text
example
```

## See also

- [options](../pkcs12-options.md)
- [sgcl::crypto::pkcs12](README.md)
