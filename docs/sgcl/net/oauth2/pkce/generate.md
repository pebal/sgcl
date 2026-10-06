[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [pkce](README.md)

# sgcl::net::oauth2::pkce::generate

```cpp
static pkce generate();
```

A new verifier, 256 bits of the system's random generator ([crypto::random](../../../crypto/random/README.md)) in base64url, and
its S256 challenge, `BASE64URL(SHA256(verifier))` (RFC 7636 §4.2). One per authorization request.

## Parameters

None.

## Return value

The verifier and its challenge.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    net::oauth2::pkce p = net::oauth2::pkce::generate();
    auto h = crypto::sha256::of(p.verifier);
    println("{}", encoding::base64::raw_url.encode(h) == p.challenge);
}
```

Output:

```text
true
```

## See also

- [config::authorization_url](../config/authorization_url.md)
- [pkce](README.md)
