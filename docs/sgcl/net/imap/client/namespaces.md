[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::namespaces, async_namespaces

```cpp
expected<imap::namespaces, io::error> namespaces() const;                                // (1)
async::task<expected<imap::namespaces, io::error>> async_namespaces() const noexcept;    // (2)
```

Sends NAMESPACE (RFC 2342): the prefixes and delimiters of the user's own mailboxes, other users' and shared ones.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

None.

## Return value

The [namespaces](../namespaces.md), or the error.

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
    net::imap::namespaces n = session.namespaces();
    println("[{}] {} {}", n.personal[0].prefix, n.personal[0].delimiter, n.shared.size());
    srv.close();
}
```

Output:

```text
[] / 0
```

## See also

- [namespaces](../namespaces.md)
- [sgcl::net::imap::client](README.md)
