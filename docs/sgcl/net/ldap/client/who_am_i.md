[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::who_am_i, async_who_am_i

```cpp
expected<string, io::error> who_am_i() const;                                // (1)
async::task<expected<string, io::error>> async_who_am_i() const noexcept;    // (2)
```

The identity the session acts as (RFC 4532's extended operation): "dn:" and a DN, "u:" and a user name, empty
for an anonymous session.

`who_am_i` waits on the calling thread; a task awaits `async_who_am_i`.

## Parameters

None.

## Return value

The identity; the server's refusal as its code where it does not know the operation.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    println("[{}]", c.who_am_i().value());
    c.bind("cn=admin,dc=example,dc=com", "secret");
    println("{}", c.who_am_i().value());
}
```

Output:

```text
[]
dn:cn=admin,dc=example,dc=com
```

## See also

- [bind](bind.md)
- [extended](extended.md)
- [client](README.md)
