[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::sort, async_sort

```cpp
expected<vector<uint32_t>, io::error> sort(const vector<order>& by, const criteria& c = criteria(),         // (1)
                                           bool descending = false) const;
async::task<expected<vector<uint32_t>, io::error>> async_sort(vector<order> by, criteria c = criteria(),    // (2)
                                                              bool descending = false) const noexcept;
```

Sends UID SORT (RFC 5256): the UIDs the criteria take, in the order of the keys, ties by arrival; descending reverses
every key.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `by` | the [keys](../order.md), the first deciding first; none: arrival |
| `c` | the criteria |
| `descending` | REVERSE of every key |

## Return value

The UIDs in order; or the error, `errc::not_supported` for a server without SORT.

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
    mail.append("alice", "INBOX", "Subject: long\r\n\r\nmore text here\r\n");
    mail.append("alice", "INBOX", "Subject: short\r\n\r\nx\r\n");
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
    println("{}", *session.sort({net::imap::order::size}));
    println("{}", *session.sort({net::imap::order::size}, net::imap::criteria(), true));
    srv.close();
}
```

Output:

```text
[2, 1]
[1, 2]
```

## See also

- [order](../order.md)
- [search](search.md)
- [sgcl::net::imap::client](README.md)
