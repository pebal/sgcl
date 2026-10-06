[sgcl](../../README.md) › [net](../README.md) › [oauth2](README.md)

# sgcl::net::oauth2::endpoints

```cpp
#include "sgcl/net/oauth2/oauth2.h"   // or "sgcl/net/oauth2.h"

namespace sgcl::net::oauth2 {
    struct endpoints {
        string authorization;
        string token;
        string device_authorization;
        string revocation;
        string introspection;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The URLs of an authorization server, named as RFC 8414's metadata names them (`authorization_endpoint`, …): a
[config](config/README.md)'s `endpoints` member. A call whose endpoint is empty fails with `invalid_argument` in the
error's `transport()`, before anything is sent.

## Member objects

| Member | Description |
|---|---|
| `authorization` | where the user approves: the base of [authorization_url](config/authorization_url.md) |
| `token` | the token endpoint: every grant and refresh |
| `device_authorization` | the device authorization endpoint (RFC 8628 §3.1) |
| `revocation` | the revocation endpoint (RFC 7009) |
| `introspection` | the introspection endpoint (RFC 7662) |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/oauth2.h"

using namespace sgcl;

int main() {
    net::oauth2::config cfg;
    cfg.endpoints.token = "https://as.example/token";
    auto e = cfg.revoke("at-1");  // no revocation endpoint
    println("{}", e.error().transport().has_value());
}
```

Output:

```text
true
```

## See also

- [config](config/README.md)
