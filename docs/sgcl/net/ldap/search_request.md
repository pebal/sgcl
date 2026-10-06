[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::search_request

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    struct search_request {
        string base;
        ldap::scope scope = ldap::scope::subtree;
        string filter = string("(objectClass=*)");
        vector<string> attributes;
        bool types_only = false;
        size_t size_limit = 0;
        duration time_limit = {};
        ldap::deref deref = ldap::deref::never;
        uint32_t page_size = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::search_request` is what a [search](client/search.md) asks (RFC 4511 §4.5.1): where it starts, how far it looks, what it matches, what it gives back, and its limits.

## Member objects

| Member | Description |
|---|---|
| `base` | the DN the search starts at |
| `scope` | the [scope](scope.md); `subtree` by default |
| `filter` | RFC 4515's text; `(objectClass=*)` by default: every entry |
| `attributes` | the attributes asked for; empty by default: all user attributes ("*"); "+" the operational ones |
| `types_only` | the attributes' names without their values; `false` by default |
| `size_limit` | entries at most; 0 by default: the server's limit |
| `time_limit` | the server's time for the search, in whole seconds; zero by default: the server's limit |
| `deref` | when aliases are followed ([deref](deref.md)); `never` by default |
| `page_size` | pages of this size asked for until the last (RFC 2696); 0 by default: one request |

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
    q.scope = net::ldap::scope::one;
    q.filter = "(title=Engineer)";
    q.attributes = {"cn"};
    q.page_size = 1;
    net::ldap::search_result r = c.search(q).value();
    for (auto& e : r.entries) {
        println("{}", e.get("cn"));
    }
}
```

Output:

```text
Alice Liddell
Carol Danvers
```

## See also

- [search](client/search.md)
- [search_result](search_result.md)
