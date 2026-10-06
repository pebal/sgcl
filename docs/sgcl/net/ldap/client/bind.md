[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::bind, async_bind

```cpp
expected<void, io::error> bind(const string& dn, const string& password) const;                  // (1)
async::task<expected<void, io::error>> async_bind(string dn, string password) const noexcept;    // (2)
```

A simple bind (RFC 4513 §5.1): the session acts as the DN from now on. Both empty is an anonymous bind; a DN
without a password is refused by the client with `errc::invalid_credentials`: a server takes such an
unauthenticated bind as anonymous (RFC 4513 §5.1.2), and a program that lost the password would go on unnoticed.
A session may bind again as someone else.

`bind` waits on the calling thread; a task awaits `async_bind`.

## Parameters

| Parameter | Description |
|---|---|
| `dn` | the DN to act as: "cn=admin,dc=example,dc=com" |
| `password` | its password |


## Return value

Nothing; `errc::invalid_credentials` for a DN or a password refused, the server's other results as their codes.

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
    c.bind("uid=alice,ou=people,dc=example,dc=com", "secret").value();
    println("{}", c.who_am_i().value());
    auto refused = c.bind("uid=alice,ou=people,dc=example,dc=com", "wrong");
    println("{}", refused.error().message());
}
```

Output:

```text
[]
dn:uid=alice,ou=people,dc=example,dc=com
ldap bind: invalid credentials
```

## See also

- [bind_plain](bind_plain.md)
- [bind_external](bind_external.md)
- [who_am_i](who_am_i.md)
- [client](README.md)
