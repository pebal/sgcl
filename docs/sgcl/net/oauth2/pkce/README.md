[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md)

# sgcl::net::oauth2::pkce

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    struct pkce;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Proof Key for Code Exchange (RFC 7636): a secret verifier the client keeps and its challenge, which goes in the
[authorization_url](../config/authorization_url.md); the code is exchanged only with the verifier
([exchange](../config/exchange.md)), so a code someone else caught on its way to the redirect is of no use to them.
RFC 9700 asks for it in every client, public or confidential. Only S256 is made: `plain` is for a client that cannot
hash.

## Member objects

| Member | Description |
|---|---|
| `verifier` | 43 characters: 256 random bits in base64url without padding |
| `challenge` | the verifier's SHA-256 in base64url without padding |

## Member functions

| Function | Description |
|---|---|
| [generate](generate.md) | a new verifier and its challenge (static) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    net::oauth2::pkce p = net::oauth2::pkce::generate();
    println("{} {}", p.verifier.size(), p.challenge.size());
    println("{}", p.verifier == net::oauth2::pkce::generate().verifier);
}
```

Output:

```text
43 43
false
```

## See also

- [config::authorization_url](../config/authorization_url.md)
- [config::exchange](../config/exchange.md)
