[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [server](README.md)

# sgcl::net::pop3::server::serve, async_serve

```cpp
expected<void, io::error> serve(const string& address) const;                                 // (1)
async::task<expected<void, io::error>> async_serve(const string& address) const noexcept;     // (2)
expected<void, io::error> serve(const net::listener& l) const;                                // (3)
async::task<expected<void, io::error>> async_serve(const net::listener& l) const noexcept;    // (4)
```

- (1–2) Listens on the address (`":110"`, `"127.0.0.1:0"`) and serves its connections, each session a task of
  the scheduler, until [shutdown](shutdown.md) or [close](close.md).
- (3–4) The connections of a listener the program made; a TLS listener's are taken as TLS from the start.

`serve` blocks the calling thread; a task awaits `async_serve`.

## Parameters

| Parameter | Description |
|---|---|
| `address` | where to listen |
| `l` | the listener to serve |

## Return value

`net::errc::server_closed` after a shutdown or a close; the listen's error; an accept's error.

## Complexity

Each connection a task.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

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
    srv.close();
    auto r = serving.wait();
    println("{}", r.error().code() == net::errc::server_closed);
}
```

Output:

```text
true
```

## See also

- [serve_tls](serve_tls.md)
- [server](README.md)
