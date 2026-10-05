[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::fetch_unseen, async_fetch_unseen

```cpp
expected<vector<message>, io::error> fetch_unseen(const string& mailbox = string("INBOX")) const;                         // (1)
async::task<expected<vector<message>, io::error>> async_fetch_unseen(string mailbox = string("INBOX")) const noexcept;    // (2)
```

The unseen messages of a mailbox whole, with their flags, envelopes, sizes and dates, in one call: SELECT, UID SEARCH
UNSEEN, UID FETCH. The messages stay unseen (BODY.PEEK); [add_flags](add_flags.md) with `\Seen` marks them read.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the mailbox; INBOX by default |

## Return value

The [messages](../message/README.md), the whole of each in [text](../message/text.md), parsed by
[email](../message/email.md); or the error.

## Complexity

Three round trips.

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
    auto unseen = session.fetch_unseen();
    for (const net::imap::message& m : *unseen) {
        println("{} {} {} bytes", m.uid, m.envelope->subject, m.text().size());
    }
    srv.close();
}
```

Output:

```text
1 Lunch 54 bytes
```

## See also

- [fetch](fetch.md)
- [search](search.md)
- [sgcl::net::imap::client](README.md)
