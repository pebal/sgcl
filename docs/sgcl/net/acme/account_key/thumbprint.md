[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [account_key](README.md)

# sgcl::net::acme::account_key::thumbprint

```cpp
const string& thumbprint() const noexcept;
```

The JWK thumbprint (RFC 7638): the SHA-256 of [jwk](jwk.md), base64url, 43 characters. Every challenge's key
authorization ends with it.

## Parameters

None.

## Return value

The thumbprint.

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
    auto digest = crypto::sha256::of(key.jwk());
    println("{}", key.thumbprint() == encoding::base64::raw_url.encode(digest));
}
```

Output:

```text
true
```

## See also

- [key_authorization](key_authorization.md)
- [sgcl::net::acme::account_key](README.md)
