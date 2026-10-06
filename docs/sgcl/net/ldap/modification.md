[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::modification

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    struct modification {
        modify_op op = modify_op::replace;
        string attribute;
        vector<string> values;

        friend bool operator==(const modification&, const modification&) noexcept = default;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::modification` is one change of a [modify](client/modify.md) (RFC 4511 §4.6): what is done to which attribute with which values.

## Member objects

| Member | Description |
|---|---|
| `op` | the [modify_op](modify_op.md); `replace` by default |
| `attribute` | the attribute changed |
| `values` | the values added, removed or put in place; none with `remove` removes them all, with `replace` the attribute |

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
    net::ldap::modification m{net::ldap::modify_op::add, "mail", {"alice@wonderland.example"}};
    c.modify("uid=alice,ou=people,dc=example,dc=com", {m}).value();
    auto alice = c.search("ou=people,dc=example,dc=com", "(uid=alice)")->entries[0];
    println("{}", alice.get_all("mail").size());
}
```

Output:

```text
2
```

## See also

- [modify](client/modify.md)
- [modify_op](modify_op.md)
