[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::bind_external, async_bind_external

```cpp
expected<void, io::error> bind_external(const string& authz = {}) const;                         // (1)
async::task<expected<void, io::error>> async_bind_external(string authz = {}) const noexcept;    // (2)
```

A SASL bind of EXTERNAL (RFC 4422 Appendix A): the identity the transport proves — the client certificate of
TLS ([tls::config](../../tls/config.md)), the peer of a unix socket — or the one asked for in its name. A session
with no such identity is refused.

`bind_external` waits on the calling thread; a task awaits `async_bind_external`.

## Parameters

| Parameter | Description |
|---|---|
| `authz` | the authorization identity to act as; empty: the one the transport gives |


## Return value

Nothing; the server's refusal as its code (`errc::inappropriate_authentication`, `errc::auth_method_not_supported`).

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
    auto r = c.bind_external();  // TCP without a client certificate: nothing to prove
    println("{}", bool(r));
}
```

Output:

```text
false
```

## See also

- [bind](bind.md)
- [start_tls](start_tls.md)
- [client](README.md)
