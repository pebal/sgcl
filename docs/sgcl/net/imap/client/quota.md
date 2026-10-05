[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::quota, async_quota

```cpp
expected<vector<imap::quota>, io::error> quota(const string& mailbox = string("INBOX")) const;                         // (1)
async::task<expected<vector<imap::quota>, io::error>> async_quota(string mailbox = string("INBOX")) const noexcept;    // (2)
```

Sends GETQUOTAROOT (RFC 9208): the quota roots of a mailbox with their storage and messages, used and limit.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the mailbox; INBOX by default |

## Return value

The [quotas](../quota.md); or the error, `errc::not_supported` for a server without QUOTA.

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
    mail.set_quota("alice", 1024, 100);
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::imap::security::none;   // the loopback, no TLS
    string address = "127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    auto roots = session.quota();
    println("{} KiB of {}, {} messages of {}", (*roots)[0].storage_used, (*roots)[0].storage_limit,
            (*roots)[0].messages_used, (*roots)[0].messages_limit);
    srv.close();
}
```

Output:

```text
1 KiB of 1024, 1 messages of 100
```

## See also

- [quota](../quota.md)
- [sgcl::net::imap::client](README.md)
