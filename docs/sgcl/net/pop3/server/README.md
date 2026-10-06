[sgcl](../../../README.md) › [net](../../README.md) › [pop3](../README.md)

# sgcl::net::pop3::server

```cpp
#include "sgcl/net/pop3/server.h"   // or "sgcl/net/pop3.h"

namespace sgcl::net::pop3 {
    class server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::pop3::server` is a POP3 server of the INBOX of each user of an
[imap::backend](../../imap/backend/README.md): [serve](serve.md) listens, each session runs as a task, and what a
session reads, marks and removes is the backend's mail — the mail an [imap::server](../../imap/server/README.md)
of the same backend serves too. RFC 1939's commands with TOP and UIDL, CAPA, RESP-CODES and PIPELINING (every
command of a batch answered, the replies written together), STLS with `tls` set, AUTH PLAIN, APOP with
`apop_secret` set; [serve_tls](serve_tls.md) listens with TLS from the first byte.

## Rules

- A handle of one word: copies share the connections. The fields are read when `serve` is called.
- A session sees the INBOX as it was at its login: its messages numbered 1 to n, their sizes as the backend
  keeps them, their unique ids `<uidvalidity>.<uid>`. DELE marks; QUIT removes what was marked; a connection that
  ends otherwise removes nothing (RFC 1939 §6).
- A user has one session at a time: a second login is `-ERR [IN-USE]`, until the first ends.
- With `tls` set, USER, PASS, APOP and AUTH are refused before STLS unless `allow_insecure_auth` says so, and
  what a client pipelined after STLS in clear text is thrown away.
- The limits end a session that abuses them: `max_auth_failures` failed logins, a wait past `idle_timeout`, a
  line past 4 KB; `max_connections` refuses a connection past it with `-ERR [SYS/TEMP]`.
- [shutdown](shutdown.md) ends the sessions gracefully, [close](close.md) at once; `serve` returns
  `net::errc::server_closed` after either.

## Member objects

| Member | Description |
|---|---|
| `backend` | the [imap::backend](../../imap/backend/README.md) whose users' INBOX is served; a memory_backend of the server's own by default |
| `check_password` | a `function<bool(const string& user, const string& password)>`: USER/PASS and AUTH PLAIN checked by it; none by default: the backend's `authenticate` (a memory_backend's `add_user`) |
| `apop_secret` | a `function<optional<string>(const string& user)>`: the user's shared secret, APOP offered (the greeting's timestamp) while it is set; none by default |
| `tls` | an `optional<tls::config>`: STLS offered with it; none by default |
| `allow_insecure_auth` | credentials before STLS all the same; `false` by default |
| `idle_timeout` | a silent connection closed, removing nothing; 10 minutes by default (RFC 1939 §3) |
| `max_connections` | the connections served at once; zero by default: no limit |
| `max_auth_failures` | failed logins before the connection is closed; 3 by default; zero: none |
| `greeting` | the greeting's text; `"POP3 server ready"` by default |
| `hostname` | the host of APOP's timestamp; empty by default: this host's name |
| `on_error` | a `function<void(const string&)>`: a backend's failure, an accept's; a line on stderr by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | a server, or a copy that shares one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another server |
| [serve, async_serve](serve.md) | listens on an address, or serves a listener |
| [serve_tls, async_serve_tls](serve_tls.md) | listens with TLS from the first byte |
| [shutdown, async_shutdown](shutdown.md) | gracefully: the sessions end after their command |
| [close](close.md) | at once: every listener and connection closed |
| [connections](connections.md) | the connections being served |

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
    c.remove(1);
    c.quit();
    println("{}", mail.open("alice", "INBOX")->messages.size());
    srv.close();
    serving.wait();
}
```

Output:

```text
0
```

## See also

- [client](../client/README.md)
- [imap::server](../../imap/server/README.md): the same mail over IMAP
- [pop3](../README.md)
