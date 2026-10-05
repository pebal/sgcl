[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::command, async_command

```cpp
expected<vector<string>, io::error> command(const string& line) const;                         // (1)
async::task<expected<vector<string>, io::error>> async_command(string line) const noexcept;    // (2)
```

Sends a command the class has no method for, as written after the tag (`"GETMETADATA ..."`, `"XLIST \"\" *"`), and
reads its answer: what imaplib's `xatom` is.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `line` | the command, without the tag and the CRLF |

## Return value

The untagged responses that came while it ran, as the server wrote them (literals inline); or the error of its NO or
BAD, `std::errc::invalid_argument` for a line with CR or LF.

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
    auto lines = session.command("STATUS INBOX (MESSAGES)");
    println("{}", (*lines)[0]);
    srv.close();
}
```

Output:

```text
* STATUS INBOX (MESSAGES 1)
```

## See also

- [search](search.md)'s text form
- [sgcl::net::imap::client](README.md)
