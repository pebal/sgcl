[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [server](README.md)

# sgcl::net::imap::server::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (2)
expected<void, io::error> serve(const net::listener& l) const;                                // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

Serves IMAP until [shutdown](shutdown.md) or [close](close.md), each connection a task of its own on the scheduler.
STARTTLS is offered when the field `tls` is set.

- (1, 2) Listens on the address (`":143"`, `"127.0.0.1:1143"`).
- (3, 4) Takes the connections of a listener the program made; a TLS listener's
  ([tls::listen](../../tls/listen.md)) are taken as TLS from the first byte.
- (1, 3) Block the calling thread (main's), as Go's `ListenAndServe` does.
- (2, 4) Return a task that does the same, for a task to `co_await` or `spawn`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the address to listen on |
| `l` | the listener |

## Return value

`net::errc::server_closed` after shutdown or close; or the error of the listen or of an accept that will not pass.

## Complexity

That of the connections served.

## Exceptions

- (1, 3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2, 4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    net::imap::server srv;
    srv.backend = mail;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    println("{} connection", srv.connections());
    srv.close();
}
```

Output:

```text
1 connection
```

## See also

- [serve_tls](serve_tls.md)
- [shutdown](shutdown.md), [close](close.md)
- [sgcl::net::imap::server](README.md)
