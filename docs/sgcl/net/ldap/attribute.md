[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::attribute

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    struct attribute {
        string name;
        vector<string> values;

        friend bool operator==(const attribute&, const attribute&) noexcept = default;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::attribute` is an attribute of an [entry](entry/README.md): its description and its values. A value is bytes in a string, a binary one (a certificate, a photo) as it is.

## Member objects

| Member | Description |
|---|---|
| `name` | the attribute's description: "cn", "userCertificate;binary" |
| `values` | its values, in the server's order |

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
    auto group = c.search("ou=groups,dc=example,dc=com", "(cn=engineers)", {"member"})->entries[0];
    for (auto& a : group.attributes) {
        println("{} has {} values", a.name, a.values.size());
    }
}
```

Output:

```text
member has 2 values
```

## See also

- [entry](entry/README.md)
- [ldap](README.md)
