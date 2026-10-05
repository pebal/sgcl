[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md)

# sgcl::net::smtp::server

```cpp
#include "sgcl/net/smtp/server.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::smtp {
    class server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::smtp::server` is an SMTP server with a handler per message, in the manner of
[http::server](../../http/server/README.md): [serve](serve.md) listens, each session runs as a task, and the
[handler](handle.md) is called with each [message](../message/README.md) — its envelope and its bytes — once the
message is whole; what it answers is the reply to the message's end. What is done with a message is the
handler's: there is no relaying and no queue. `on_sender` and `on_recipient` decide MAIL and RCPT as they come;
`auth` checks AUTH PLAIN and LOGIN; `starttls` offers STARTTLS, [serve_tls](serve_tls.md) listens with TLS from the
first byte.

A session offers PIPELINING (every command of a batch answered in its turn, the replies written together), SIZE,
8BITMIME, SMTPUTF8, CHUNKING with BINARYMIME, DSN (RET, ENVID, NOTIFY, ORCPT given to the handler in the envelope)
and ENHANCEDSTATUSCODES; every command of RFC 5321 with its error replies, VRFY answered 252 and EXPN refused.

## Rules

- A handle of one word: copies share the handler and the connections. The fields are read when `serve` is called.
- A message is read whole before the handler runs, at most `max_message_bytes` (announced in SIZE); one past it is
  read to its end and refused with 552. The handler runs in the session's task: a handler that waits is a task.
- What a client pipelined after STARTTLS in clear text is thrown away, and the session starts again over TLS:
  EHLO first, nothing of before kept (CVE-2011-0411).
- AUTH is offered over TLS only unless `allow_insecure_auth` says so; asked for without it, 538.
- The limits end a session that abuses them: `max_errors` refused commands, `max_junk_commands` commands that
  carry no message (NOOP, RSET, VRFY, HELP, EHLO), a wait past `timeout` — each with a 421 and the end.
- A handler that throws answers 451 for its message, `on_error` hears of it, and the session goes on.
- [shutdown](shutdown.md) ends the sessions gracefully, [close](close.md) at once; `serve` returns
  `net::errc::server_closed` after either.

## Member objects

| Member | Description |
|---|---|
| `hostname` | the name of the greeting and of EHLO; empty by default: this host's name |
| `starttls` | an `optional<tls::config>`: STARTTLS offered with it; none by default |
| `auth` | a `function<bool(const string& user, const string& password)>`: AUTH PLAIN and LOGIN offered and checked by it; none by default |
| `allow_insecure_auth` | AUTH offered without TLS too; `false` by default |
| `on_sender` | a `function<reply(const envelope&)>`: MAIL decided, the envelope holding its address and parameters; a reply of 0 takes it, one of 400 or more refuses it; none by default: every sender taken |
| `on_recipient` | a `function<reply(const envelope&, const string&)>`: RCPT decided the same way; none by default |
| `max_message_bytes` | the most bytes of a message, announced in SIZE; 32 MB by default |
| `max_recipients` | the most recipients of a message, RCPT past it 452; 100 by default (RFC 5321 §4.5.3.1.8) |
| `max_line_bytes` | the longest command, past it 500; 2048 by default |
| `max_errors` | refused commands before 421 and the end; 10 by default |
| `max_junk_commands` | commands without a message between two messages before 421; 100 by default |
| `timeout` | the wait for each command and each piece of data; 5 minutes by default (RFC 5321 §4.5.3.2.7) |
| `on_error` | a `function<void(const string&)>`: a handler's exception, an accept's failure; a line on stderr by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | a server with no handler, or a copy that shares one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another server |
| [handle](handle.md) | the handler of each message |
| [serve, async_serve](serve.md) | listens on an address, or serves a listener |
| [serve_tls, async_serve_tls](serve_tls.md) | listens with TLS from the first byte |
| [shutdown, async_shutdown](shutdown.md) | gracefully: the sessions end after their command |
| [close](close.md) | at once: every listener and connection closed |

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
    srv.on_recipient = [](const net::smtp::envelope&, const string& to) {
        if (to.view().ends_with("@example.org")) {
            return net::smtp::reply();
        }
        return net::smtp::reply{550, "5.7.1", "Relaying denied"};
    };
    srv.handle([](net::smtp::message m) {
        auto mail = m.email();
        println("{} for {}: {}", m.envelope().from, m.envelope().to[0], mail->text().trim());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());

    net::smtp::send(url, encoding::email("a@example.com", "bob@example.org", "Hi", "Hello, Bob."));
    encoding::email elsewhere("a@example.com", "x@elsewhere.example", "Hi", "x");
    auto refused = net::smtp::send(url, elsewhere);
    println("{}", net::smtp::reply_of(refused.error())->to_string());
    srv.close();
    serving.wait();
}
```

Output:

```text
a@example.com for bob@example.org: Hello, Bob.
550 5.7.1 Relaying denied
```

## See also

- [message](../message/README.md): what a handler gets
- [http::server](../../http/server/README.md): the same shape for HTTP
- [smtp](../README.md)
