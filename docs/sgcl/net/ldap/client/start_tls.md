[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::start_tls, async_start_tls

```cpp
expected<void, io::error> start_tls(const net::tls::config& c = {}) const;                         // (1)
async::task<expected<void, io::error>> async_start_tls(net::tls::config c = {}) const noexcept;    // (2)
```

StartTLS (RFC 4511 §4.14): the session goes on over TLS from here, the server's name and certificate checked as
the config says (the URL's host when it names none). No other operation may be in flight, and none starts
until it ends. [connect](connect.md) does it by itself for `security::automatic` and `starttls`; a session made
with `security::none` takes it here. A server that refuses leaves the session as it was, in clear text.

`start_tls` waits on the calling thread; a task awaits `async_start_tls`.

## Parameters

| Parameter | Description |
|---|---|
| `c` | the roots, a client certificate, the server's name |


## Return value

Nothing; the server's refusal as its code, TLS's error (which ends the session).

## Complexity

Two round trips and TLS's handshake.

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
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    auto r = c.start_tls();  // this slapd has no TLS
    println("{} {}", bool(r), c.is_tls());
    println("{}", c.who_am_i().value());
}
```

Output:

```text
false false
dn:cn=admin,dc=example,dc=com
```

## See also

- [is_tls](is_tls.md)
- [security](../security.md)
- [client](README.md)
