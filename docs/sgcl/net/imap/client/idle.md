[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::idle, async_idle

```cpp
expected<vector<update>, io::error> idle(duration timeout = std::chrono::minutes(29)) const;                                // (1)
expected<vector<update>, io::error> idle(const async::stop_token& stop,                                                     // (2)
                                         duration timeout = std::chrono::minutes(29)) const;
async::task<expected<vector<update>, io::error>> async_idle(duration timeout = std::chrono::minutes(29)) const noexcept;    // (3)
async::task<expected<vector<update>, io::error>> async_idle(async::stop_token stop,                                         // (4)
                                                            duration timeout = std::chrono::minutes(29)) const noexcept;
```

Sends IDLE (RFC 2177) and waits for the server's first updates of the selected mailbox — a new message, an expunge,
flags another session changed — then ends it with DONE. The timeout is 29 minutes by default, under the 30 after which
RFC 9051 lets servers drop a silent connection; a loop of idle calls is how a client waits for mail. To a server
without IDLE: a NOOP after the wait (30 s at most).

- (1, 3) Until updates or the timeout.
- (2, 4) Or the stop.
- (1, 2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of
  the program, never a worker.
- (3, 4) Return a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `stop` | ends the wait |
| `timeout` | the longest wait |

## Return value

The [updates](../update.md) that came (the options' `on_update` was told of each, [mailbox](mailbox.md) has their
counts); empty at the timeout or the stop. Or the error.

## Complexity

A round trip, and the wait.

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
    mail.append("alice", "INBOX", "Subject: news\r\n\r\nx\r\n");   // a delivery of the program's
    auto updates = session.idle();
    println("{} {}", (*updates)[0].kind == net::imap::update::kind::exists, (*updates)[0].number);
    println("{}", session.idle(std::chrono::milliseconds(100))->size());
    srv.close();
}
```

Output:

```text
true 1
0
```

## See also

- [update](../update.md), [noop](noop.md)
- [client::options](../client-options.md)'s `on_update`
- [sgcl::net::imap::client](README.md)
