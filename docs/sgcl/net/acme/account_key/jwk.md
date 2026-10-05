[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::jwk

```cpp
const string& jwk() const noexcept;
```

The public key as a JSON Web Key (RFC 7517), its members in the order RFC 7638 hashes them and without white space:
`{"crv":"P-256","kty":"EC","x":"…","y":"…"}`, `{"crv":"Ed25519","kty":"OKP","x":"…"}`, `{"e":"…","kty":"RSA","n":"…"}`.
What a new account's requests carry in their header, before the account has a URL.

## Parameters

None.

## Return value

The JWK's text.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::account_key key;
    auto jwk = encoding::json::parse(key.jwk());
    string kty = (*jwk)["kty"].as_string(string());
    string crv = (*jwk)["crv"].as_string(string());
    println("{} {} {}", kty, crv, (*jwk)["x"].as_string(string()).size());
}
```

Output:

```text
EC P-256 43
```

## See also

- [thumbprint](thumbprint.md)
- [sgcl::net::acme::account_key](README.md)
