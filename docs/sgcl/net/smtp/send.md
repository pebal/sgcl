[sgcl](../../README.md) › [net](../README.md) › [smtp](README.md)

# sgcl::net::smtp::send, async_send

```cpp
expected<receipt, io::error> send(const string& url, const encoding::email& m,
                                  const options& o = {});
async::task<expected<receipt, io::error>> async_send(string url, encoding::email m,
                                                     options o = {}) noexcept;
```

The message sent in one line: a session opened with the server of `url` (the connection, TLS for `smtps://`, the
greeting, EHLO, STARTTLS when offered, AUTH with the credentials), the message sent, QUIT.

The envelope is the message's: From for the reverse path (Sender when there is one), the addresses of To, Cc and
Bcc for the recipients, each once. Bcc is never in what is sent. The message is written for what the server
offers: 8bit content where it says 8BITMIME, UTF-8 in the head where an address past ASCII needs SMTPUTF8 and it
offers it; MAIL says SIZE, BODY and SMTPUTF8; the transaction is pipelined where it says PIPELINING and the data
goes by BDAT where it says CHUNKING.

`send` runs the session on the scheduler and waits for it on the calling thread; a task awaits `async_send`.

## Parameters

| Parameter | Description |
|---|---|
| `url` | `smtp://[user:password@]host[:port]` or `smtps://...` |
| `m` | the message |
| `o` | the credentials, TLS, the waits, the stop ([options](options.md)) |

## Return value

The [receipt](receipt.md): the reply to the message's end, the recipients refused while others were taken. Or the
[io::error](../../io/error/README.md): of the connection or of TLS; `net::errc::smtp_reply` for a refusal (the
greeting, MAIL, every recipient — the last refusal —, the data), `smtp_auth_failed`, `smtp_tls_required`,
`smtp_unsupported` (an address past ASCII and no SMTPUTF8, a message past SIZE, no AUTH mechanism of both sides),
`malformed_smtp_reply`; `EINVAL` for a message without recipients; `invalid_url`, `unsupported_scheme`;
`ETIMEDOUT`, `ECANCELED`.

## Complexity

Linear in the size of the message; one round trip a command (one for the whole transaction with PIPELINING).

## Exceptions

- `send`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_send`: none.

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
    encoding::email m("alice@example.com", "bob@example.org", "Hello", "Hi, Bob.");

    m.add_bcc("hidden@example.org");
    auto r = net::smtp::send(url, m);
    println("{}", r->reply.to_string());
    srv.close();
    serving.wait();
}
```

Output:

```text
alice@example.com -> bob@example.org: Hello
250 2.0.0 OK: message accepted
```

## See also

- [client](client/README.md): several messages over one session
- [deliver](deliver.md): without a server of one's own
- [smtp](README.md)
