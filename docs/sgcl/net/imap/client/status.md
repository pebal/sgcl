[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::status, async_status

```cpp
expected<imap::status, io::error> status(const string& mailbox) const;                         // (1)
async::task<expected<imap::status, io::error>> async_status(string mailbox) const noexcept;    // (2)
```

Sends STATUS: the counts of a mailbox without selecting it, every item the server has (MESSAGES, UIDNEXT, UIDVALIDITY,
UNSEEN, DELETED and SIZE where it has them, HIGHESTMODSEQ with CONDSTORE).

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the mailbox |

## Return value

The [status](../status.md); or the error, `errc::nonexistent` for no such mailbox.

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
    auto s = session.status("INBOX");
    println("{} {} {}", s->messages, s->unseen, s->uid_next);
    println("{}", session.status("Nowhere").error().code() == net::imap::errc::nonexistent);
    srv.close();
}
```

Output:

```text
1 1 2
true
```

## See also

- [select](select.md)
- [list_options](../list_options.md)'s `status`
- [sgcl::net::imap::client](README.md)
