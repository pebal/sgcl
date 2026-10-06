[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [entry](README.md)

# sgcl::net::ldap::entry::get_all

```cpp
vector<string> get_all(const string& name) const;
```

Every value of the attribute, in the server's order, its name compared without case.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the attribute's name |

## Return value

The values; none for an attribute the entry lacks.

## Complexity

Linear in the attributes.

## Exceptions

None.

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
    auto alice = c.search("ou=people,dc=example,dc=com", "(uid=alice)")->entries[0];
    for (auto& v : alice.get_all("objectClass")) {
        println("{}", v);
    }
}
```

Output:

```text
inetOrgPerson
```

## See also

- [entry](README.md)
