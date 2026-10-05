[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [client](README.md)

# sgcl::net::imap::client::append, async_append

```cpp
expected<uint32_t, io::error> append(const string& mailbox, const string& message, const vector<string>& flags = {},    // (1)
                                     const optional<time::datetime>& date = nullopt) const;
async::task<expected<uint32_t, io::error>> async_append(string mailbox, string message, vector<string> flags = {},      // (2)
                                                        optional<time::datetime> date = nullopt) const noexcept;
```

Sends APPEND: a message added to a mailbox, with its flags and its internal date. The message goes as a literal
(without a round trip of its own where the server has LITERAL+), as literal8 for one with NUL bytes (BINARY), in UTF8
( ) for 8-bit headers over UTF8=ACCEPT.

1. Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
   program, never a worker.
2. Returns a task that does the same, for a task to `co_await`.

## Parameters

| Parameter | Description |
|---|---|
| `mailbox` | the mailbox |
| `message` | the message, RFC 5322 with CRLF line ends |
| `flags` | its flags |
| `date` | its internal date; the server's now when none |

## Return value

Its UID there (APPENDUID); 0 when the server gave none. Or the error: `errc::nonexistent` (TRYCREATE),
`errc::over_quota`, `errc::too_big`, `std::errc::invalid_argument` for a flag that is not an atom.

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
    uint32_t uid = session.append("INBOX", "Subject: draft\r\n\r\nTo be written.\r\n", {net::imap::flag::draft});
    println("{}", uid);
    session.select("INBOX");
    println("{}", (*session.fetch(uid))[0].flags);
    srv.close();
}
```

Output:

```text
1
["\\Draft"]
```

## See also

- [memory_backend::append](../memory_backend/append.md): a delivery straight into the store
- [sgcl::net::imap::client](README.md)
