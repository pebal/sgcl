[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::remove, async_remove

```cpp
expected<void, io::error> remove(uint32_t number) const;
async::task<expected<void, io::error>> async_remove(uint32_t number) const noexcept;
```

DELE (RFC 1939 §5): the message marked deleted. The server removes it when the session ends with [quit](quit.md); [reset](reset.md) takes the marks off, and a session that ends otherwise removes nothing.

`remove` waits on the calling thread; a task awaits `async_remove`.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the message's number |

## Return value

Nothing; `errc::no_such_message` for a number not there or marked already.

## Complexity

One round trip.

## Exceptions

- `remove`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_remove`: none.

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
    c.remove(1);
    println("{}", c.status()->messages);
    c.quit();
    net::pop3::client again = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    println("{}", again.status()->messages);
    again.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
1
1
```

## See also

- [reset](reset.md), [quit](quit.md)
- [client](README.md)
