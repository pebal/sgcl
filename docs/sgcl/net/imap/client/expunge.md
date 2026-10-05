[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::expunge, async_expunge

```cpp
expected<void, io::error> expunge() const;                                                 // (1)
expected<void, io::error> expunge(const sequence_set& uids) const;                         // (2)
async::task<expected<void, io::error>> async_expunge() const noexcept;                     // (3)
async::task<expected<void, io::error>> async_expunge(sequence_set uids) const noexcept;    // (4)
```

Sends EXPUNGE: the messages of the selected mailbox marked `\Deleted` removed.

- (1, 3) Every one so marked.
- (2, 4) Those of the set alone (UID EXPUNGE, UIDPLUS): the deleted messages of another session's stay.
- (1, 2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (3, 4) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `uids` | the messages |

## Return value

Nothing; or the error, `errc::not_supported` for UID EXPUNGE to a server without UIDPLUS.

## Complexity

One round trip.

## Exceptions

- (1, 2) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (3, 4) None.

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
    session.select("INBOX");
    session.add_flags(1, {net::imap::flag::deleted});
    session.expunge(1);
    println("{}", session.mailbox().exists);
    srv.close();
}
```

Output:

```text
0
```

## See also

- [close_mailbox](close_mailbox.md)
- [add_flags](add_flags.md)
- [sgcl::net::imap::client](README.md)
