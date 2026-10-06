[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::modify_op

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    enum class modify_op : uint8_t {
        add = 0,
        remove = 1,
        replace = 2,
        increment = 3
    };
}
```

How a [modification](modification.md) changes its attribute (RFC 4511 §4.6, RFC 4525).

| Value | Description |
|---|---|
| `add` | the values added; one there already is `errc::attribute_or_value_exists` |
| `remove` | the values taken off; all of them when none are given |
| `replace` | the values become the attribute's; none removes the attribute |
| `increment` | the value added to the attribute's integer (RFC 4525) |

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
    c.modify("uid=carol,ou=people,dc=example,dc=com",
             {{net::ldap::modify_op::remove, "title", {}},
              {net::ldap::modify_op::add, "description", {"pilot"}}}).value();
    auto carol = c.search("ou=people,dc=example,dc=com", "(uid=carol)")->entries[0];
    println("[{}] {}", carol.get("title"), carol.get("description"));
}
```

Output:

```text
[] pilot
```

## See also

- [modification](modification.md)
- [modify](client/modify.md)
