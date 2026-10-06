[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& url);                                                   // (1)
static async::task<expected<client, io::error>> async_connect(string url) noexcept;                              // (2)
static expected<client, io::error> connect(const string& url, const options& o);                                 // (3)
static async::task<expected<client, io::error>> async_connect(string url, options o) noexcept;                   // (4)
static expected<client, io::error> connect(const net::connection& transport, const options& o);                  // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept;    // (6)
```

A session with an LDAP server: the connection; TLS from the first byte for `ldaps://`, port 636 or
`security::tls`; StartTLS when the security asks for it (`automatic` on `ldap://`, `starttls`), a refusal of it
the error; a simple bind when the URL has a user or a password. A session without them is anonymous until a
[bind](bind.md).

- (1–4) The server of the URL: `ldap://host[:port]`, `ldaps://host[:port]` or `host[:port]`; its user (a DN,
  percent-encoded where it needs to be) and password a simple bind's.
- (5–6) Over a connection the program made (a unix socket, a tunnel); TLS, StartTLS and no bind as the options
  say.

The options' timeout bounds the dial and TLS's handshake, then each operation's wait; their stop ends the dial
with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | where the server is |
| `transport` | a connection to it |
| `o` | the security, TLS, the timeout, the stop |


## Return value

The session; the dial's error, TLS's, the refusal of StartTLS, `errc::invalid_credentials` for a bind refused,
`net::errc::invalid_url` for a URL that is not one.

## Complexity

A round trip for each step: the dial, TLS, StartTLS, the bind.

## Exceptions

- (1), (3), (5) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4), (6) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    auto c = net::ldap::client::connect("ldap://cn=admin,dc=example,dc=com:secret@localhost:3890", o);
    println("{}", c->who_am_i().value());
    auto refused = net::ldap::client::connect("ldap://cn=admin,dc=example,dc=com:nope@localhost:3890", o);
    println("{}", refused.error().code() == net::ldap::errc::invalid_credentials);
}
```

Output:

```text
dn:cn=admin,dc=example,dc=com
true
```

## See also

- [options](../client-options.md)
- [bind](bind.md)
- [client](README.md)
