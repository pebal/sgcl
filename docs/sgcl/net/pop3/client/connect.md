[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& address);                                               // (1)
static async::task<expected<client, io::error>> async_connect(string address) noexcept;                          // (2)
static expected<client, io::error> connect(const string& address, const options& o);                             // (3)
static async::task<expected<client, io::error>> async_connect(string address, options o) noexcept;               // (4)
static expected<client, io::error> connect(const net::connection& transport, const options& o);                  // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport, options o) noexcept;    // (6)
```

A session with a POP3 server: the connection, TLS from the first byte for `pop3s://`, port 995 or
`security::tls`; the greeting; CAPA; STLS when the security asks for it (`automatic` and `starttls`; a server
without STLS is `errc::starttls_unavailable`), CAPA again over TLS; the login when there is a user — SASL PLAIN
when the server offers it, else USER and PASS, APOP when the options ask for it. A session without credentials
is connected and not logged in.

- (1–4) The server of the address: `host[:port]`, `[v6]:port`, or a `pop3://` or `pop3s://` URL whose user and
  password are the credentials when the options have none.
- (5–6) Over a connection the program made (a tunnel, a pipe in memory).

The options' timeout bounds the dial, the TLS handshake and the login together, then each reply's wait; their stop
ends the dial with `ECANCELED`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where the server is |
| `transport` | a connection to it |
| `o` | the credentials, the mechanism, the security, TLS, the timeout, the stop |

## Return value

The session; the dial's error, TLS's, `errc::authentication_failed` for credentials refused, `errc::in_use` for a
maildrop held by another session, `errc::malformed_response` for a server that is no POP3 server,
`net::errc::invalid_url` for a URL that is not one.

## Complexity

A few round trips.

## Exceptions

- (1), (3), (5) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4), (6) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: bob@example.com\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.security = net::pop3::security::none;  // the loopback, no TLS
    string url = string::concat("pop3://alice:secret@", l.local_endpoint().to_string());
    net::pop3::client c = net::pop3::client::connect(url, o).value();
    println("{}", c.status()->messages);
    auto refused = net::pop3::client::connect(string::concat("pop3://alice:wrong@", l.local_endpoint().to_string()), o);
    println("{}", refused.error().code() == net::pop3::errc::authentication_failed);
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
1
true
```

## See also

- [options](../client-options.md)
- [client](README.md)
