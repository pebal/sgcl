[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md) › [client](README.md)

# sgcl::net::pop3::client::retrieve, async_retrieve

```cpp
expected<string, io::error> retrieve(uint32_t number) const;                                                 // (1)
async::task<expected<string, io::error>> async_retrieve(uint32_t number) const noexcept;                     // (2)
expected<vector<string>, io::error> retrieve(const vector<uint32_t>& numbers) const;                         // (3)
async::task<expected<vector<string>, io::error>> async_retrieve(vector<uint32_t> numbers) const noexcept;    // (4)
```

- (1–2) RETR (RFC 1939 §5): the message of the number, the dots that stuffed its lines taken off, its line
  breaks CRLF; [encoding::email::parse](../../../encoding/email/parse.md) reads it.
- (3–4) The messages of the numbers, in their order: every RETR written at once when the server says
  PIPELINING (RFC 2449 §6.6), one after another otherwise. A message refused is the error of the call, the
  replies after it read all the same, so the session goes on.

## Parameters

| Parameter | Description |
|---|---|
| `number` | the message's number |
| `numbers` | the messages' numbers |

## Return value

The message, or the messages; `errc::no_such_message` for a number not in the maildrop or marked deleted.

## Complexity

One round trip per message, or one for all of them pipelined; linear in their size.

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
    string first = c.retrieve(1).value();
    for (auto line : first.split("\r\n")) {
        println("[{}]", line);
    }
    auto both = c.retrieve(vector<uint32_t>{1, 2}).value();
    println("{}", both.size());
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
[From: bob@example.com]
[Subject: Lunch]
[]
[Noon?]
[]
2
```

## See also

- [top](top.md), [list](list.md)
- [client](README.md)
