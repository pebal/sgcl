[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md)

# sgcl::net::ldap::entry

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    struct entry {
        string dn;
        vector<attribute> attributes;

        string get(const string& name) const;
        vector<string> get_all(const string& name) const;

        friend bool operator==(const entry&, const entry&) noexcept = default;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::entry` is an entry of a directory: its distinguished name and its [attributes](../attribute.md),
as a [search](../client/search.md) gives it and [add](../client/add.md) takes it. A plain value; `get` and
`get_all` find an attribute by its name without case, as LDAP compares names.

## Member objects

| Member | Description |
|---|---|
| `dn` | the distinguished name: "uid=alice,ou=people,dc=example,dc=com" |
| `attributes` | the attributes, in the server's order |

## Member functions

| Function | Description |
|---|---|
| [get](get.md) | the first value of an attribute |
| [get_all](get_all.md) | every value of an attribute |

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
    println("{}", alice.dn);
    for (auto& a : alice.attributes) {
        println("{}: {}", a.name, a.values.size());
    }
}
```

Output:

```text
uid=alice,ou=people,dc=example,dc=com
objectClass: 1
uid: 1
cn: 1
sn: 1
mail: 1
title: 1
userPassword: 1
```

## See also

- [attribute](../attribute.md)
- [search_result](../search_result.md)
