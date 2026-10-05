[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::fetch_message, async_fetch_message

```cpp
expected<string, io::error> fetch_message(uint32_t uid) const;                                // (1)
async::task<expected<string, io::error>> async_fetch_message(uint32_t uid) const noexcept;    // (2)
```

Returns the whole message of a UID (BODY.PEEK[]), its `\Seen` left as it was.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `uid` | the UID |

## Return value

The message's bytes; or the error, `errc::expunged` when the server has no message of the UID.

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
    session.select("INBOX");
    print("{}", *session.fetch_message(1));
    println("{}", session.fetch_message(9).error().code() == net::imap::errc::expunged);
    srv.close();
}
```

Output:

```text
From: Bob <bob@example.com>
Subject: Lunch

Noon?
true
```

## See also

- [fetch](fetch.md)
- [sgcl::net::imap::client](README.md)
