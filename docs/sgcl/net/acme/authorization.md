[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::authorization

```cpp
#include "sgcl/net/acme/types.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    struct authorization {
        string url;
        acme::identifier identifier;
        acme::status status = status::pending;
        optional<time::datetime> expires;
        vector<acme::challenge> challenges;
        bool wildcard = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

An authorization of an identifier (RFC 8555 §7.1.4): the account's proof that it controls a name, by one of the
challenges the CA offers. What [authorization](client/authorization.md) and
[wait_authorization](client/wait_authorization.md) give: Go's `acme.Authorization`.

## Member objects

| Object | Description |
|---|---|
| `url` | its URL |
| `identifier` | the name it is for ([identifier](identifier.md)); a wildcard's without its `*.`, `wildcard` saying so |
| `status` | `pending`, `valid`, `invalid`, `deactivated`, `expired` or `revoked` ([status](status.md)) |
| `expires` | when it stops being valid; none when the CA did not say |
| `challenges` | the ways it may be proved ([challenge](challenge.md)); one of them answered is enough |
| `wildcard` | whether it is for the wildcard of `identifier`, which dns-01 alone proves |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca;
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    acme.register_account({.terms_agreed = true});
    net::acme::order o = acme.new_order({"example.com", "*.example.com"});
    for (auto& url : o.authorizations) {
        net::acme::authorization az = acme.authorization(url);
        string status = net::acme::to_string(az.status);
        println("{} wildcard={} {}:", az.identifier.value, az.wildcard, status);
        for (auto& c : az.challenges) {
            println("  {}", c.type);
        }
    }
}
```

Output:

```text
example.com wildcard=false pending:
  http-01
  dns-01
  tls-alpn-01
example.com wildcard=true pending:
  dns-01
```

## See also

- [challenge](challenge.md), [order](order.md)
- [client::authorization](client/authorization.md)
- [net::acme](README.md)
