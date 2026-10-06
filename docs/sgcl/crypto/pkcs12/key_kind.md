[sgcl](../../README.md) › [crypto](../README.md) › [pkcs12](README.md)

# sgcl::crypto::pkcs12::key_kind

```cpp
x509::key_kind key_kind() const noexcept;
```

The kind of the file's key ([x509::key_kind](../x509-key_kind.md)): which typed key [key_pkcs8](key_pkcs8.md) reads into.

## Parameters

None.

## Return value

The kind; `key_kind::none` for a file of certificates alone.

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
    println("{}", crypto::pkcs12::parse(file, "password")->key_kind() == crypto::x509::key_kind::p256);
}
```

Output:

```text
true
```

## See also

- [key_pkcs8](key_pkcs8.md)
- [sgcl::crypto::pkcs12](README.md)
