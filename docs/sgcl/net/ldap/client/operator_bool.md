[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle holds a session; an ended session is still held.

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
    net::ldap::client none;
    println("{} {}", bool(none), bool(c));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](client.md)
- [client](README.md)
