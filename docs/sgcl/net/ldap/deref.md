[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::deref

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    enum class deref : uint8_t { never = 0, searching = 1, finding = 2, always = 3 };
}
```

When a [search](client/search.md) follows an alias entry to the entry it names (RFC 4511 §4.5.1.3).

| Value | Description |
|---|---|
| `never` | aliases are entries like the others |
| `searching` | followed under the base, not at the base |
| `finding` | followed at the base, not under it |
| `always` | followed everywhere |

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
    q.base = "ou=people,dc=example,dc=com";
    q.deref = net::ldap::deref::always;
    println("{}", c.search(q)->entries.size());
}
```

Output:

```text
4
```

## See also

- [search_request](search_request.md)
