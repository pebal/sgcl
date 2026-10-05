[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::server

```cpp
#include "sgcl/net/imap/server.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::imap::server` is an IMAP server (RFC 9051, IMAP4rev2, with IMAP4rev1's clients) over a
[backend](../backend/README.md) that keeps the mail: the protocol and its extensions are the server's, the users,
mailboxes, messages, UIDs, flags and mod-sequences the backend's. Sessions run as tasks, one per connection. A mailbox
open in any session has a hub in the server that holds its messages' data while it is open, makes every change through
the backend and tells the other sessions that have it selected: each announces it at its next command (EXISTS, EXPUNGE
or VANISHED, FETCH of flags) in its own numbering, at once while it idles. A delivery made through the backend by the
program (a memory_backend's or a maildir_backend's `append`, from an SMTP server's handler) wakes an idling session at
once; a change made to Maildir directories by another process is found by the idling sessions' poll and by every NOOP.

A server is a handle of one word: copies share the connections and the open mailboxes. The settings are public fields,
read when serve() is called, as [http::server](../../http/server/README.md)'s are.

## Rules

- **Capabilities.** IMAP4rev1 IMAP4rev2 LITERAL+ SASL-IR ENABLE IDLE NAMESPACE UIDPLUS UNSELECT MOVE CONDSTORE QRESYNC
  ESEARCH SEARCHRES SPECIAL-USE CREATE-SPECIAL-USE LIST-EXTENDED LIST-STATUS CHILDREN ID UTF8=ACCEPT SORT
  SORT=DISPLAY THREAD=ORDEREDSUBJECT THREAD=REFERENCES WITHIN BINARY STATUS=SIZE MULTIAPPEND APPENDLIMIT; QUOTA when the
  backend has quotas, COMPRESS=DEFLATE after the login, STARTTLS with a `tls`, AUTH=PLAIN and LOGIN, and XOAUTH2 and
  OAUTHBEARER with a `check_token`.
- **Logins.** `check_password` decides LOGIN, AUTHENTICATE PLAIN and LOGIN; without it the backend's `authenticate`
  does (a [memory_backend](../memory_backend/README.md)'s users), and a backend without one refuses every login.
  `check_token` decides XOAUTH2 and OAUTHBEARER. After `max_auth_failures` failures the connection is told BYE.
  With a `tls`, LOGIN and AUTHENTICATE are refused before STARTTLS (LOGINDISABLED, PRIVACYREQUIRED), and what a
  client sent in the clear behind STARTTLS is dropped, never read as the first bytes of the protected stream.
- **Limits.** A command line past `max_command_bytes` ends the connection with BYE [TOOBIG]; a literal past
  `max_literal_bytes` is refused with BAD [TOOBIG] before the client sends it (a synchronizing one), or ends the
  connection (one of LITERAL+, which the client sent anyway). A connection past `max_connections` is told BYE.
  A connection silent for `idle_timeout` (30 minutes, RFC 9051 §5.4) is closed, `login_timeout` before the login.
- **Searches** are the server's, over the backend's messages: flags, numbers, sizes and dates at once, the header and
  the text read for a key that needs them, the encoded words of a header decoded, a body's transfer encoding and
  charset undone, strings matched without regard to case by Unicode's full case folding. SORT and THREAD keep what
  they read of a message while the mailbox is open; so do ENVELOPE and BODYSTRUCTURE.
- **Pipelining.** Commands that come pipelined are answered in one write while the next is already buffered whole.
- **Shutdown.** [shutdown](shutdown.md) closes the listeners, tells the sessions waiting for a command BYE and closes
  them, lets each other end after its command, and returns when all have; [close](close.md) closes everything at once.

## Member objects

| Member | Description |
|---|---|
| `imap::backend backend` | where the mail is ([backend](../backend/README.md)); a memory_backend of the server's own by default |
| `function<bool(const string&, const string&)> check_password` | the user and the password of LOGIN and AUTHENTICATE PLAIN or LOGIN: whether they log in; empty, the default, the backend's `authenticate` |
| `function<bool(const string&, const string&)> check_token` | the user and the bearer token of XOAUTH2 and OAUTHBEARER (RFC 7628); empty, the default: neither offered |
| `optional<net::tls::config> tls` | STARTTLS offered with this [config](../../tls/config.md), the logins refused before it; none by default |
| `duration idle_timeout` | an authenticated connection silent this long is closed; 30 minutes by default (RFC 9051 §5.4: at least 30) |
| `duration login_timeout` | the same before the login; 60 s by default |
| `duration poll_interval` | how often an idling session asks the backend for changes made outside the server; 10 s by default, zero never |
| `size_t max_literal_bytes` | the largest literal, the largest message appended (APPENDLIMIT); 64 MiB by default |
| `size_t max_command_bytes` | the longest command line, literals apart; 64 KiB by default |
| `size_t max_connections` | past it a connection is told BYE; zero, the default, none |
| `int max_auth_failures` | failed logins before the connection is closed; 3 by default, zero none |
| `bool compress` | COMPRESS=DEFLATE (RFC 4978) offered; `true` by default |
| `vector<pair<string, string>> id` | the server's answer to ID (RFC 2971); `{{"name", "sgcl"}}` by default |
| `string greeting` | the text of the greeting; `"IMAP4rev2 server ready"` by default |
| `function<void(const string&)> on_error` | a backend's failure, an accept's; a line on stderr by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | constructs a server over a memory_backend of its own |
| `(destructor)` | drops the handle; the server goes on while a copy or a serve() holds it |
| `operator=` | the handle of another server, the settings copied |
| [serve, async_serve](serve.md) | listens on an address, or takes a listener's connections, and serves |
| [serve_tls, async_serve_tls](serve_tls.md) | listens over TLS from the first byte (port 993) and serves |
| [shutdown, async_shutdown](shutdown.md) | ends gracefully |
| [close](close.md) | ends at once |
| [connections](connections.md) | the number of connections being served |

## Example

A server of a user's mail in memory, and a client of the same program reading it:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "From: Bob <bob@example.com>\r\nSubject: Lunch\r\n\r\nNoon?\r\n");
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
    auto s = session.status("INBOX");
    println("{} message, {} unseen", s->messages, s->unseen);
    srv.close();
}
```

Output:

```text
1 message, 1 unseen
```

A Maildir server on port 993, TLS from the first byte, the users' passwords checked by the program:

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::imap::server srv;
    srv.backend = net::imap::maildir_backend("/var/mail");
    srv.check_password = [](const string& user, const string& password) {
        return user == "alice" && password == "secret";
    };
    net::tls::config tls;
    tls.identities = {net::tls::identity(io::read_text("/etc/ssl/mail.pem"),
                                         crypto::read_secret("/etc/ssl/mail.key"))};
    auto r = srv.serve_tls(":993", tls);
    println("{}", r.error().message());
}
```

## See also

- [backend](../backend/README.md), [memory_backend](../memory_backend/README.md), [maildir_backend](../maildir_backend/README.md)
- [client](../client/README.md): the other side
- [http::server](../../http/server/README.md): the same model of fields and serve()
- `tests/net/imap/server.cpp` (raw sessions: every command, literals, limits, STARTTLS, two sessions),
  `tests/net/imap/interop.cpp` (Python's imaplib and curl as clients)
