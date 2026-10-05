[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::send, async_send

```cpp
expected<receipt, io::error> send(const encoding::email& m) const;                          // (1)
expected<receipt, io::error> send(const encoding::email& m, const envelope& e) const;       // (2)
expected<receipt, io::error> send(const envelope& e, const string& data) const;             // (3)
async::task<expected<receipt, io::error>> async_send(const encoding::email& m)              // (4)
    const noexcept;
async::task<expected<receipt, io::error>> async_send(const encoding::email& m,              // (5)
                                                     const envelope& e) const noexcept;
async::task<expected<receipt, io::error>> async_send(const envelope& e,                     // (6)
                                                     const string& data) const noexcept;
```

A transaction: MAIL, RCPT for each recipient, the data by DATA (its dots doubled, its line breaks CRLF) or by BDAT
where the server offers CHUNKING; pipelined where it offers PIPELINING. MAIL says SIZE, BODY=8BITMIME for data past
ASCII, SMTPUTF8 for an address past ASCII, RET and ENVID; RCPT says NOTIFY and ORCPT — the DSN fields only where
the server offers DSN.

- (1, 4) The message to the recipients of its To, Cc and Bcc, from its From (Sender when it has one); written for
  the server (8bit where it says 8BITMIME, UTF-8 in the head where SMTPUTF8 is needed and offered), its Bcc left out.
- (2, 5) The message to the envelope given.
- (3, 6) The bytes of a message the program made, to the envelope.
- (4–6) The same for a task.

A recipient refused while others are taken is in the receipt; none taken is the error of the last refusal, the
transaction reset (and, where DATA was pipelined and taken, ended by an empty message as RFC 2920 asks).

## Parameters

| Parameter | Description |
|---|---|
| `m` | the message |
| `e` | the envelope |
| `data` | the message's bytes |

## Return value

The [receipt](../receipt.md); or the error, as [send](../send.md)'s: `smtp_reply` with the reply, `smtp_unsupported` before anything is sent (an address past ASCII and no SMTPUTF8, a message past SIZE), `EINVAL` for an envelope without recipients or an address holding CR, LF, NUL, `<` or `>`, an error of the connection (the session broken).

## Complexity

Linear in the size of the message; a round trip a command, one for the transaction with PIPELINING.

## Exceptions

- (1–3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (4–6) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.hostname = "mx.example";
    srv.handle([](net::smtp::message m) {
        println("{} -> {}: {}", m.envelope().from, m.envelope().to[0], m.email()->subject());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());

    net::smtp::client c = net::smtp::client::connect(url);
    encoding::email m("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");
    c.send(m);

    net::smtp::envelope e;
    e.from = "alice@example.com";
    e.to = {"carol@example.net"};
    c.send(m, e);
    c.send(e, "Subject: raw\r\n\r\nmade by hand\r\n");
    println("{}", c.send(net::smtp::envelope(), "x").error().message());
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
alice@example.com -> bob@example.org: Hello
alice@example.com -> carol@example.net: Hello
alice@example.com -> carol@example.net: raw
smtp no recipients: Invalid argument
```

## See also

- [envelope](../envelope.md)
- [receipt](../receipt.md)
- [client](README.md)
