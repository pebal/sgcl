[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::is_tls

```cpp
bool is_tls() const noexcept;
```

Whether the session runs over TLS: `ldaps://`, or after [start_tls](start_tls.md).

## Parameters

None.

## Return value

Whether it does.

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
    println("{}", c.is_tls());
}
```

Output:

```text
false
```

## See also

- [start_tls](start_tls.md)
- [client](README.md)
