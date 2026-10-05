[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md)

# sgcl::net::smtp::client

```cpp
#include "sgcl/net/smtp/client.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::client` is a session with an SMTP server, for several messages over one connection: Python's
`smtplib.SMTP`, Go's `smtp.Client`. [connect](connect.md) opens it — the connection, TLS for `smtps://`, the
greeting, EHLO (HELO for a server that knows no EHLO), STARTTLS, AUTH — and [send](send.md) sends a message, a
message to an envelope of its own, or the bytes of a message the program made; [quit](quit.md) ends it. What the
server offers is asked with [has_extension](has_extension.md) and [extension](extension.md).

## Rules

- A handle of one word, as a [connection](../../connection/README.md) is: a copy is the same session
  ([operator==](operator_cmp.md)). A default-constructed one holds none, and an operation on it is a contract
  violation.
- A session is a conversation in turn: one operation at a time, from one thread or task.
- A refusal of the server leaves the session usable (the transaction reset by RSET); a broken connection, a
  malformed reply or a stop end it, and every later call is `io::errc::closed`.
- The waits are RFC 5321 §4.5.3.2's unless [options](../options.md) give one; `options::stop` ends the operation
  in progress with `ECANCELED` and the session with it.
- The blocking forms run on the scheduler and wait for it on the calling thread; a task awaits the `async_` forms.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | a client that holds no session, or a copy that shares one |
| `(destructor)` | drops the handle; the connection is closed when the session is collected |
| `operator=` | the handle of another session |
| [connect, async_connect](connect.md) | a session opened with a server (static) |

#### Messages

| Function | Description |
|---|---|
| [send, async_send](send.md) | a message, to its recipients or an envelope |
| [verify, async_verify](verify.md) | VRFY: what the server says of an address |
| [reset, async_reset](reset.md) | RSET: the transaction dropped |
| [noop, async_noop](noop.md) | NOOP: the session kept alive |
| [quit, async_quit](quit.md) | QUIT, and the connection closed |
| [close](close.md) | the connection closed without QUIT |

#### Observers

| Function | Description |
|---|---|
| [greeting](greeting.md) | the text of the server's greeting |
| [has_extension](has_extension.md) | whether EHLO named an extension |
| [extension](extension.md) | an extension's parameters |
| [max_size](max_size.md) | the largest message SIZE allows |
| [is_tls](is_tls.md) | whether the session runs over TLS |
| [operator bool](operator_bool.md) | whether the handle holds a session |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two handles share one session |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
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
    for (int i : range(3)) {
        string subject = string::concat("Message ", to_string(i));
        encoding::email m("alice@example.com", "bob@example.org", subject, "Hi.");
        c.send(m);
    }
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
alice@example.com -> bob@example.org: Message 0
alice@example.com -> bob@example.org: Message 1
alice@example.com -> bob@example.org: Message 2
```

## See also

- [send](../send.md): one message in one line
- [options](../options.md)
- [smtp](../README.md)
