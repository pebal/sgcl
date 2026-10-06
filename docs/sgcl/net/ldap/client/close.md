[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::close

```cpp
expected<void, io::error> close() const noexcept;
```

The connection closed without UnbindRequest; the operations waiting fail with `io::errc::closed`.

## Parameters

None.

## Return value

Nothing, or the close's error.

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
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    println("{}", bool(c.close()));
    println("{}", c.who_am_i().error().code() == io::errc::closed);
}
```

Output:

```text
true
true
```

## See also

- [unbind](unbind.md)
- [client](README.md)
