[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::top, async_top

```cpp
expected<string, io::error> top(uint32_t number, size_t lines = 0) const;
async::task<expected<string, io::error>> async_top(uint32_t number, size_t lines = 0) const noexcept;
```

TOP (RFC 1939 §7): the head of the message, the empty line after it, and the first lines of its body — what a client shows before it downloads a message whole.

`top` waits on the calling thread; a task awaits `async_top`.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the message's number |
| `lines` | the lines of the body; 0 for the head alone |

## Return value

The text, CRLF line breaks; `errc::not_supported` when the server's CAPA lacks TOP, `errc::no_such_message` for a number not there.

## Complexity

One round trip; linear in what comes.

## Exceptions

- `top`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_top`: none.

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
    string head = c.top(2, 1).value();
    for (auto line : head.split("\r\n")) {
        println("[{}]", line);
    }
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
[From: carol@example.com]
[Subject: Report]
[]
[Line 1]
[]
```

## See also

- [retrieve](retrieve.md)
- [client](README.md)
