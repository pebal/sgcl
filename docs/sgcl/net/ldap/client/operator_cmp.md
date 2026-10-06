[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::operator== (sgcl::net::ldap::client)

```cpp
friend bool operator==(const client& a, const client& b) noexcept;
```

Whether two handles are the same session; `!=` is its negation.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles |

## Return value

Whether they are the same session.

## Complexity

Constant.

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
    net::ldap::client same = c;
    net::ldap::client other = net::ldap::client::connect("ldap://localhost:3890", o).value();
    println("{} {}", same == c, other == c);
}
```

Output:

```text
true false
```

## See also

- [client](README.md)
