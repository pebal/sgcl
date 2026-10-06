[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::bind_plain, async_bind_plain

```cpp
expected<void, io::error> bind_plain(const string& user, const string& password,              // (1)
                                     const string& authz = {}) const;
async::task<expected<void, io::error>> async_bind_plain(string user, string password,         // (2)
                                                        string authz = {}) const noexcept;
```

A SASL bind of PLAIN (RFC 4616): a user name as the server knows it (often not a DN: "alice", "u:alice") and its
password, and the identity to act as when it is another (authz, empty: the user's own). PLAIN sends the
password as it is: over TLS only, as the server usually demands. A server that does not offer PLAIN answers
`errc::auth_method_not_supported`.

`bind_plain` waits on the calling thread; a task awaits `async_bind_plain`.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the authentication identity |
| `password` | its password |
| `authz` | the authorization identity to act as; empty: the user's own |


## Return value

Nothing; `errc::invalid_credentials` for credentials refused, `errc::auth_method_not_supported` where PLAIN is not offered.

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
    auto r = c.bind_plain("alice", "secret");  // this slapd offers no PLAIN
    println("{}", r.error().code() == net::ldap::errc::auth_method_not_supported);
}
```

Output:

```text
true
```

## See also

- [bind](bind.md)
- [bind_external](bind_external.md)
- [client](README.md)
