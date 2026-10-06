[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [entry](README.md)

# sgcl::net::ldap::entry::get

```cpp
string get(const string& name) const;
```

The first value of the attribute, its name compared without case ("CN" finds "cn").

## Parameters

| Parameter | Description |
|---|---|
| `name` | the attribute's name |

## Return value

The value; empty for an attribute the entry lacks or one without values.

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
    println("{} [{}]", alice.get("MAIL"), alice.get("telephoneNumber"));
}
```

Output:

```text
alice@example.com []
```

## See also

- [entry](README.md)
