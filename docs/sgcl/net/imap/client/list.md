[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::list, async_list

```cpp
expected<vector<list_entry>, io::error> list(const string& pattern = string("*")) const;                         // (1)
expected<vector<list_entry>, io::error> list(const string& pattern, const list_options& o) const;                // (2)
async::task<expected<vector<list_entry>, io::error>> async_list(string pattern = string("*")) const noexcept;    // (3)
async::task<expected<vector<list_entry>, io::error>> async_list(string pattern,                                  // (4)
                                                                list_options o) const noexcept;
```

Sends LIST (RFC 9051 §6.3.9, LIST-EXTENDED): the mailboxes whose names match the pattern, `*` any number of
characters, `%` any but the delimiter (`"%"` the top level, `"Archive/*"` everything under Archive). Names are UTF-8,
INBOX first; parents that are no mailboxes come with `\NonExistent` or `\Noselect`.

- (1, 3) Every mailbox of the pattern.
- (2, 4) As the options ask: the subscribed ones, the special-use ones, each with its status (LIST-STATUS).
- (1, 2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (3, 4) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern, UTF-8 |
| `o` | the [options](../list_options.md) |

## Return value

The [entries](../list_entry/README.md), or the error.

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
    session.create("Archive/2025");
    session.create("Archive/2026");
    auto top = session.list("%");
    for (const net::imap::list_entry& e : *top) {
        println("{} {}", e.name, e.has_attribute("\\HasChildren"));
    }
    srv.close();
}
```

Output:

```text
INBOX false
Archive true
```

## See also

- [list_options](../list_options.md), [list_entry](../list_entry/README.md)
- [status](status.md)
- [sgcl::net::imap::client](README.md)
