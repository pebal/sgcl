[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& address);                                    // (1)
static async::task<expected<client, io::error>> async_connect(string address) noexcept;               // (2)
static expected<client, io::error> connect(const string& address, const options& o);                  // (3)
static async::task<expected<client, io::error>> async_connect(string address, options o) noexcept;    // (4)
static expected<client, io::error> connect(const net::connection& transport, const options& o);       // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport,              // (6)
                                                               options o) noexcept;
```

Connects to a server, reads its greeting and capabilities, protects the connection as the options say, logs in, and
turns on what the client speaks of what the server offers (IMAP4rev2 or UTF8=ACCEPT, QRESYNC or CONDSTORE,
COMPRESS=DEFLATE when asked, ID when given).

- (1, 2) `address` is `"host:port"` (port 143 when there is none, 993 for `security::tls`) or an `imap://` or
  `imaps://` URL: its user name and password log in (percent-decoded), and its path names a mailbox to select
  (`imaps://alice:secret@mail.example.com/INBOX`). The default options require STARTTLS on a plain port.
- (3, 4) The same with the options.
- (5, 6) Over a connection there is (one through a proxy, a pipe in memory): TLS over it or STARTTLS as the options
  say. The transport is closed when the connect fails.
- (1, 3, 5) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (2, 4, 6) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | `"host:port"`, `"host"`, or an `imap://` or `imaps://` URL |
| `o` | the [options](../client-options.md) |
| `transport` | the connection to speak IMAP over |

## Return value

The client, logged in when the options or the URL gave credentials; or the error: the dial's or TLS's,
`errc::starttls_unavailable` for a server without STARTTLS that the client requires it of,
`errc::authentication_failed` for credentials refused, `errc::bye` for a server that refuses the connection,
`net::errc::invalid_url`.

## Complexity

That of the round trips: the greeting, CAPABILITY, STARTTLS and its handshake, the login, ENABLE.

## Exceptions

- (1, 3, 5) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2, 4, 6) None.

## Example

By a URL that logs in and selects INBOX:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: hi\r\n\r\nx\r\n");
    net::imap::server srv;
    srv.backend = mail;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.security = net::imap::security::none;
    string url = "imap://alice:secret@127.0.0.1:" + to_string(listener.local_endpoint().port()) + "/INBOX";
    net::imap::client session = net::imap::client::connect(url, o);
    println("{} {}", session.mailbox().name, session.mailbox().exists);
    auto refused = net::imap::client::connect(url);   // the default options require STARTTLS
    println("{}", refused.error().code() == net::imap::errc::starttls_unavailable);
    srv.close();
}
```

Output:

```text
INBOX 1
true
```

## See also

- [client::options](../client-options.md), [security](../security.md)
- [logout](logout.md), [close](close.md)
- [sgcl::net::imap::client](README.md)
