[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::scope

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    enum class scope : uint8_t {
        base = 0,
        one = 1,
        subtree = 2
    };
}
```

How far a [search](client/search.md) looks under its base (RFC 4511 §4.5.1.2).

| Value | Description |
|---|---|
| `base` | the base entry alone |
| `one` | its children, not the base |
| `subtree` | the base and everything under it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    net::ldap::search_request q;
    q.base = "dc=example,dc=com";
    for (auto s : {net::ldap::scope::base, net::ldap::scope::one, net::ldap::scope::subtree}) {
        q.scope = s;
        println("{}", c.search(q)->entries.size());
    }
}
```

Output:

```text
1
2
7
```

## See also

- [search_request](search_request.md)
