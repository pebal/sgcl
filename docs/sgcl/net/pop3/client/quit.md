[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::quit, async_quit

```cpp
expected<void, io::error> quit() const;
async::task<expected<void, io::error>> async_quit() const noexcept;
```

QUIT (RFC 1939 §6): the server removes the messages marked deleted and ends the session; the connection is closed either way, and the client takes no more calls.

`quit` waits on the calling thread; a task awaits `async_quit`.

## Parameters

None.

## Return value

Nothing; the server's refusal (it could not remove every marked message: `errc::sys_temp`) is the error.

## Complexity

One round trip.

## Exceptions

- `quit`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_quit`: none.

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
    mail.append("alice", "INBOX", "From: carol@example.com\r\nSubject: Report\r\n\r\nLine 1\r\nLine 2\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::pop3::security::none;  // the loopback, no TLS
    net::pop3::client c = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    c.remove(2);
    println("{}", c.quit().has_value());
    println("{}", c.noop().has_value());
    srv.close();
    serving.wait();
}
```

Output:

```text
true
false
```

## See also

- [close](close.md), [remove](remove.md)
- [client](README.md)
