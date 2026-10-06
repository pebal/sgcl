[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::status, async_status

```cpp
expected<mailbox_status, io::error> status() const;
async::task<expected<mailbox_status, io::error>> async_status() const noexcept;
```

STAT (RFC 1939 §5): the messages of the maildrop not marked deleted, and their size in bytes.

`status` waits on the calling thread; a task awaits `async_status`.

## Parameters

None.

## Return value

The [mailbox_status](../mailbox_status.md); a refusal is the error (`errc::err` before the login).

## Complexity

One round trip.

## Exceptions

- `status`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_status`: none.

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
    net::pop3::mailbox_status st = c.status().value();
    println("{} messages, {} bytes", st.messages, st.size);
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
2 messages, 108 bytes
```

## See also

- [list](list.md)
- [client](README.md)
