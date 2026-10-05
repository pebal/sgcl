[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::remove, async_remove

```cpp
expected<void, io::error> remove(const string& mailbox) const;                         // (1)
async::task<expected<void, io::error>> async_remove(string mailbox) const noexcept;    // (2)
```

Sends DELETE: a mailbox and its messages removed; the mailboxes under it stay. INBOX cannot be deleted.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the mailbox |

## Return value

Nothing; or the error, `errc::nonexistent` for no such mailbox.

## Complexity

One round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
    session.create("Old");
    session.remove("Old");
    println("{}", session.status("Old").error().code() == net::imap::errc::nonexistent);
    srv.close();
}
```

Output:

```text
true
```

## See also

- [create](create.md)
- [sgcl::net::imap::client](README.md)
