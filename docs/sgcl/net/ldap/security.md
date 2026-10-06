[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::security

```cpp
#include "sgcl/net/ldap/types.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    enum class security : uint8_t { automatic, tls, starttls, none };
}
```

How a [client](client/README.md) protects its connection, a field of its [options](client-options.md).

| Value | Description |
|---|---|
| `automatic` | TLS from the first byte for `ldaps://` and port 636, StartTLS for the rest: a server that refuses StartTLS is an error |
| `tls` | TLS from the first byte |
| `starttls` | StartTLS before anything else (RFC 4511 §4.14) |
| `none` | neither: a loopback, a unix socket |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    net::ldap::client::options strict;
    strict.security = net::ldap::security::starttls;  // this slapd has no TLS
    println("{}", bool(net::ldap::client::connect("ldap://localhost:3890", strict)));
    println("{}", c.is_tls());
}
```

Output:

```text
false
false
```

## See also

- [options](client-options.md)
- [start_tls](client/start_tls.md)
