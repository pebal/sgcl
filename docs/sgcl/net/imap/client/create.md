[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::create, async_create

```cpp
expected<void, io::error> create(const string& mailbox, const string& use = string()) const;    // (1)
async::task<expected<void, io::error>> async_create(string mailbox,                             // (2)
                                                    string use = string()) const noexcept;
```

Sends CREATE: a new mailbox, its parents implied (`"Archive/2026"` makes Archive a parent without being a mailbox).
With a special use (CREATE-SPECIAL-USE, RFC 6154) the mailbox is marked `\Sent`, `\Trash` ...

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the name, UTF-8 |
| `use` | a special use, [net::imap::special_use](../README.md#objects-and-types)`::sent` ...; empty for none |

## Return value

Nothing; or the error, `errc::already_exists` for a name taken.

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
    session.create("Sent", net::imap::special_use::sent);
    println("{}", session.create("Sent").error().code() == net::imap::errc::already_exists);
    auto sent = session.list("Sent");
    println("{}", (*sent)[0].has_attribute(net::imap::special_use::sent));
    srv.close();
}
```

Output:

```text
true
true
```

## See also

- [remove](remove.md), [rename](rename.md)
- [list](list.md)
- [sgcl::net::imap::client](README.md)
