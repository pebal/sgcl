[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::fetch, async_fetch

```cpp
expected<vector<message>, io::error> fetch(const sequence_set& uids) const;                            // (1)
expected<vector<message>, io::error> fetch(const sequence_set& uids, const fetch_options& o) const;    // (2)
async::task<expected<vector<message>, io::error>> async_fetch(sequence_set uids) const noexcept;       // (3)
async::task<expected<vector<message>, io::error>> async_fetch(sequence_set uids,                       // (4)
                                                              fetch_options o) const noexcept;
```

Sends UID FETCH: the messages of the selected mailbox whose UIDs are in the set, with what the options ask.

- (1, 3) Their UIDs and flags.
- (2, 4) What the [options](../fetch_options.md) ask: the envelope, the structure, the size, the date, the
  mod-sequence, sections whole or in part (BODY.PEEK, BINARY), only those changed since a mod-sequence.
- (1, 2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (3, 4) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `uids` | the UIDs: `7`, `net::imap::sequence_set(1, 10)`, `net::imap::sequence_set::all()`, what [search](search.md) gave |
| `o` | the [options](../fetch_options.md) |

## Return value

The [messages](../message/README.md) in the server's order, those of the set only (a FETCH another session's change
pushed meanwhile is an update, not a result); or the error, `errc::bad` with no mailbox selected. An empty set is
nothing sent and nothing returned.

## Complexity

One round trip; the messages read as they come.

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
    net::imap::fetch_options what;
    what.envelope = true;
    what.sections = {"TEXT"};
    auto messages = session.fetch(net::imap::sequence_set::all(), what);
    for (const net::imap::message& m : *messages) {
        print("{} {}: {}", m.uid, m.envelope->subject, *m.section("TEXT"));
    }
    srv.close();
}
```

Output:

```text
1 Lunch: Noon?
```

## See also

- [fetch_options](../fetch_options.md), [message](../message/README.md)
- [search](search.md), [fetch_message](fetch_message.md)
- [sgcl::net::imap::client](README.md)
