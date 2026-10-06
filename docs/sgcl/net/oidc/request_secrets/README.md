[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md)

# sgcl::net::oidc::request_secrets

```cpp
#include "sgcl/net/oidc/oidc.h"   // or "sgcl/net/oidc.h"

namespace sgcl::net::oidc {
    struct request_secrets;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The state and the nonce of one authorization request, kept in the user's session until the redirect comes back: the
state, compared with the redirect's, ties the redirect to the session (an attacker's code is refused); the nonce, put
in the authorization URL and held to the ID token's by [verify](../provider/verify.md), ties the token to the request
(a token replayed from another sign-in is refused).

## Member objects

| Member | Description |
|---|---|
| `state` | 128 random bits, base64url: 22 characters |
| `nonce` | 128 random bits, base64url: 22 characters |

## Member functions

| Function | Description |
|---|---|
| [generate](generate.md) | a new state and nonce (static) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oidc.h"

using namespace sgcl;

int main() {
    net::oidc::request_secrets s = net::oidc::request_secrets::generate();
    println("{} {} {}", s.state.size(), s.nonce.size(), s.state == s.nonce);
}
```

Output:

```text
22 22 false
```

## See also

- [provider::config](../provider/config.md)
- [provider::verify](../provider/verify.md)
