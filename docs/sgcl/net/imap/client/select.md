[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::select, async_select

```cpp
expected<selected, io::error> select(const string& mailbox) const;                             // (1)
expected<selected, io::error> select(const string& mailbox, const select_options& o) const;    // (2)
async::task<expected<selected, io::error>> async_select(string mailbox) const noexcept;        // (3)
async::task<expected<selected, io::error>> async_select(string mailbox,                        // (4)
                                                        select_options o) const noexcept;
```

Sends SELECT (EXAMINE when the options ask for read only): the mailbox opened for the commands on messages, another
one closed first.

- (1, 3) Read and write.
- (2, 4) As the options ask: read only, or with QRESYNC's state of an earlier session (its UIDVALIDITY and highest
  mod-sequence), the server then answering with the UIDs expunged since and the messages changed since.
- (1, 2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (3, 4) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the mailbox |
| `o` | the [options](../select_options.md) |

## Return value

What the server said of the mailbox ([selected](../selected.md)); or the error, `errc::nonexistent`.

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
    net::imap::selected box = session.select("INBOX");
    println("{} {} {}", box.exists, box.uid_next, box.read_only);
    srv.close();
}
```

Output:

```text
1 2 false
```

## See also

- [examine](examine.md), [unselect](unselect.md)
- [select_options](../select_options.md), [mailbox](mailbox.md)
- [sgcl::net::imap::client](README.md)
