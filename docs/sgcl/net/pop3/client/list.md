[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::list, async_list

```cpp
expected<vector<message_info>, io::error> list() const;                                       // (1)
async::task<expected<vector<message_info>, io::error>> async_list() const noexcept;           // (2)
expected<message_info, io::error> list(uint32_t number) const;                                // (3)
async::task<expected<message_info, io::error>> async_list(uint32_t number) const noexcept;    // (4)
```

- (1–2) LIST joined with UIDL (RFC 1939 §5, §7): every message not marked deleted, its number, size and unique
  id; both commands sent at once when the server says PIPELINING, UIDL left out when the server's CAPA lacks it.
- (3–4) The same of one message; `errc::no_such_message` for a number not in the maildrop or marked deleted.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the message's number in the session, from 1 |

## Return value

The [message_info](../message_info.md) of each message, in the order of their numbers, or of the one.

## Complexity

One or two round trips; linear in the messages.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/pop3.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: bob@example.com\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
    mail.append("alice", "INBOX", "From: carol@example.com\r\nSubject: Report\r\n\r\nLine 1\r\nLine 2\r\n");
    net::pop3::server srv;
    srv.backend = mail;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    net::pop3::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.security = net::pop3::security::none;  // the loopback, no TLS
    net::pop3::client c = net::pop3::client::connect(l.local_endpoint().to_string(), o).value();
    auto messages = c.list().value();
    for (auto& m : messages) {
        println("{} {} {}", m.number, m.size, !m.uid.empty());
    }
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
1 48 true
2 60 true
```

## See also

- [retrieve](retrieve.md), [status](status.md)
- [client](README.md)
