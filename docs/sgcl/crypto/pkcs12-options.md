[sgcl](../README.md) › [crypto](README.md) › [pkcs12](pkcs12/README.md)

# sgcl::crypto::pkcs12::options

```cpp
#include "sgcl/crypto/pkcs12.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    class pkcs12 {
    public:
        struct options {
            string friendly_name;
            uint32_t iterations = 2048;
            uint32_t max_iterations = 1000000;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::crypto::pkcs12::options` is what a file is written and read with. Every field has a default:
`pkcs12::encode(key, chain, password, {.friendly_name = "server"})`.

## Member objects

| Member | Description |
|---|---|
| `friendly_name` | [encode](pkcs12/encode.md)'s: the name of the key and the leaf, what a key store lists the entry under; empty, the default: none |
| `iterations` | [encode](pkcs12/encode.md)'s: the iterations of PBKDF2 and of the MAC's derivation; 2048 by default, OpenSSL 3's; 0 is `std::invalid_argument` |
| `max_iterations` | [parse](pkcs12/parse.md)'s: the most iterations a file's derivation may ask for, `errc::unsupported` past them (a file cannot make a reader spin); a million by default |

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
    auto file = crypto::pkcs12::encode(key, chain, "password", {.friendly_name = "server", .iterations = 10000});
    auto refused = crypto::pkcs12::parse(file, "password", {.max_iterations = 5000});
    println("{}", refused.error().code() == crypto::errc::unsupported);
}
```

Output:

```text
true
```

## See also

- [encode](pkcs12/encode.md), [parse](pkcs12/parse.md)
- [sgcl::crypto::pkcs12](pkcs12/README.md)
