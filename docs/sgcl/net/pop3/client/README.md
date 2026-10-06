[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md)

# sgcl::net::pop3::client

```cpp
#include "sgcl/net/pop3/client.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::pop3::client` is a session with a POP3 server: [connect](connect.md) dials, takes TLS as the
[options](../client-options.md) say and logs in; then the maildrop's messages are [listed](list.md) with their sizes
and unique ids, [retrieved](retrieve.md) whole or their [heads](top.md), [marked](remove.md) for deletion, and
removed at [quit](quit.md). Python's `poplib.POP3` in one object, the replies parsed.

## Rules

- A handle of one word, as a connection is: a copy is the same session.
- One call at a time: a session is a conversation in turn. A batch of [retrieve](retrieve.md) is pipelined when
  the server says PIPELINING, and [list](list.md) sends LIST and UIDL together.
- After [quit](quit.md) or [close](close.md), or a failure of the connection, every call is `io::errc::closed`.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client talks to its server |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no session, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another session |
| [connect, async_connect](connect.md) | a session with a server, logged in |
| [greeting](greeting.md) | the greeting's text |
| [capabilities](capabilities.md) | CAPA's lines |
| [has](has.md) | whether CAPA named a capability |
| [is_tls](is_tls.md) | whether the session runs over TLS |
| [status, async_status](status.md) | STAT: the messages and their size |
| [list, async_list](list.md) | LIST and UIDL: the messages with their sizes and ids |
| [retrieve, async_retrieve](retrieve.md) | RETR: messages whole |
| [top, async_top](top.md) | TOP: a message's head and first lines |
| [remove, async_remove](remove.md) | DELE: a message marked for deletion |
| [reset, async_reset](reset.md) | RSET: the marks taken off |
| [noop, async_noop](noop.md) | NOOP: the session kept alive |
| [quit, async_quit](quit.md) | QUIT: the marked messages removed, the session ended |
| [close](close.md) | the connection closed without QUIT |
| [operator bool](operator_bool.md) | whether the handle holds a session |
| [operator==](operator_cmp.md) | whether two handles are the same session |

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
        string text = c.retrieve(m.number).value();
        println("{}: {} bytes, {}", m.number, text.size(), text.contains("Lunch"));
        c.remove(m.number);
    }
    c.quit();
    println("{}", mail.open("alice", "INBOX")->messages.size());
    srv.close();
    serving.wait();
}
```

Output:

```text
1: 48 bytes, true
0
```

## See also

- [server](../server/README.md)
- [pop3](../README.md)
