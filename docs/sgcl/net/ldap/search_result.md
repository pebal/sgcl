[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::search_result

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    struct search_result {
        vector<entry> entries;
        vector<string> referrals;
        bool truncated = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::search_result` is what a [search](client/search.md) gives: the entries, the references to other servers it met (SearchResultReference, RFC 4511 §4.5.3; not followed), and whether a limit stopped it.

## Member objects

| Member | Description |
|---|---|
| `entries` | the [entries](entry/README.md) matched, in the server's order |
| `referrals` | the URLs of the references, as the server gave them |
| `truncated` | the size or the time limit reached (result 4 or 3): the entries are those found until then |

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
    q.size_limit = 2;
    auto r = c.search(q).value();
    println("{} {}", r.entries.size(), r.truncated);
}
```

Output:

```text
2 true
```

## See also

- [search](client/search.md)
- [search_request](search_request.md)
