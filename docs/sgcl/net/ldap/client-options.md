[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md) › [client](client/README.md) › options

# sgcl::net::ldap::client::options

```cpp
#include "sgcl/net/ldap/client.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    class client {
    public:
        struct options {
            ldap::security security = ldap::security::automatic;
            net::tls::config tls;
            duration timeout = std::chrono::seconds(30);
            async::stop_token stop;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::client::options` is how a [client](client/README.md) talks to its server, the argument of
[connect](client/connect.md): how the connection is protected and how long it waits.

## Member objects

| Member | Description |
|---|---|
| `security` | the [security](security.md); `automatic` by default: TLS for `ldaps://` and port 636, StartTLS for the rest |
| `tls` | the [tls::config](../tls/config.md) of `ldaps://` and StartTLS: the roots, a client certificate; the server's name the URL's host when none is set |
| `timeout` | the dial and TLS's handshake, then each operation's wait; 30 s by default; zero: none |
| `stop` | ends the dial with `ECANCELED` |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    o.timeout = std::chrono::seconds(5);
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    println("{}", c.search("dc=example,dc=com", "(objectClass=organizationalUnit)")->entries.size());
}
```

Output:

```text
2
```

## See also

- [security](security.md)
- [connect](client/connect.md)
