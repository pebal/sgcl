[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::client

```cpp
#include "sgcl/net/imap/client.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class client {
    public:
        struct options;
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::imap::client` is a connection to an IMAP server (RFC 9051, IMAP4rev2, and IMAP4rev1 servers), connected
and logged in by [connect](connect.md), the commands of the protocol its methods. It is what Python's imaplib is, with
the responses read into values: a fetch gives [messages](../message/README.md) with their flags,
[envelopes](../envelope.md) and [MIME structures](../body_structure/README.md), a search takes
[criteria](../criteria/README.md) and gives UIDs, a select gives the [state of the mailbox](../selected.md). Messages
are named by their UIDs, which stay while the numbers of the messages move with every expunge.

A client is a handle of one word, its copies the same connection. Independent commands from several tasks go out
pipelined (RFC 9051 §5.5), each waiting for its own answer; the responses nobody asked for — a new message, an
expunge, flags another session changed, the server's alert — update the state [mailbox](mailbox.md) gives and go to
the options' `on_update`, and [idle](idle.md) waits for them. The client turns on what it speaks when the server
offers it: IMAP4rev2 or UTF8=ACCEPT (names in UTF-8, modified UTF-7 otherwise), QRESYNC (expunges as UIDs, the
resynchronization of [select](select.md)), and COMPRESS=DEFLATE when the options ask.

## Rules

- **Two forms.** Every command blocks the calling thread (the exchange runs on the scheduler, the thread waits: for a
  thread of the program, never a worker) or, as `async_x`, returns a task for a task to `co_await`.
- **TLS.** `imaps://` and port 993 are TLS from the first byte; anything else requires STARTTLS unless the options'
  [security](../security.md) says otherwise, and a server that does not offer it is refused before the credentials
  go. Bytes the server sent in the clear behind its STARTTLS answer are dropped. The server's name is the address's
  host, its chain checked against the options' `tls.roots` (the system's by default).
- **Errors.** A NO or a BAD is the error of its response code ([errc](../errc.md): `nonexistent`, `already_exists`,
  `over_quota` ...), its operation the command, its path the server's text; a response that breaks the grammar is
  `errc::malformed_response` and ends nothing but the command. A connection the server closed, or [close](close.md),
  makes every command after it fail with io's `errc::closed` (or `errc::bye` with the server's text).
- **Timeouts.** The options' `timeout` bounds the connect and the login together, then each command's wait for its
  answer (an IDLE's own timeout apart).
- **Limits.** A literal past the options' `max_literal_bytes` (256 MiB) ends the command with `errc::too_big`; a
  response line past 1 MB is malformed.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client connects and logs in |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | constructs a client of no connection |
| `(destructor)` | drops the handle; the connection stays while a copy holds it, until [close](close.md) or [logout](logout.md) |
| `operator=` | the handle of another client: copies share the connection |
| [connect, async_connect](connect.md) | connects and logs in (static) |
| [operator bool](operator_bool.md) | whether the handle holds a connection |

#### The connection

| Function | Description |
|---|---|
| [capabilities](capabilities.md) | the server's capabilities |
| [close](close.md) | closes the connection now |
| [command, async_command](command.md) | a command the class has no method for |
| [enable, async_enable](enable.md) | ENABLE: capabilities turned on |
| [has](has.md) | whether the server has a capability |
| [is_closed](is_closed.md) | whether the connection ended |
| [is_compressed](is_compressed.md) | whether COMPRESS=DEFLATE is on |
| [is_tls](is_tls.md) | whether the connection is over TLS |
| [logout, async_logout](logout.md) | LOGOUT, then the connection closed |
| [noop, async_noop](noop.md) | NOOP: the server's pending updates read |
| [server_id](server_id.md) | the server's answer to ID |

#### Mailboxes

| Function | Description |
|---|---|
| [close_mailbox, async_close_mailbox](close_mailbox.md) | CLOSE: the deleted messages expunged, the mailbox closed |
| [create, async_create](create.md) | CREATE, with a special use |
| [examine, async_examine](examine.md) | EXAMINE: a mailbox opened read only |
| [list, async_list](list.md) | LIST: the mailboxes of a pattern |
| [mailbox](mailbox.md) | the state of the selected mailbox now |
| [namespaces, async_namespaces](namespaces.md) | NAMESPACE |
| [quota, async_quota](quota.md) | GETQUOTAROOT: the quotas of a mailbox |
| [remove, async_remove](remove.md) | DELETE |
| [rename, async_rename](rename.md) | RENAME |
| [select, async_select](select.md) | SELECT, with QRESYNC's state |
| [status, async_status](status.md) | STATUS of a mailbox |
| [subscribe, async_subscribe](subscribe.md) | SUBSCRIBE |
| [unselect, async_unselect](unselect.md) | UNSELECT: the mailbox closed, nothing expunged |
| [unsubscribe, async_unsubscribe](unsubscribe.md) | UNSUBSCRIBE |

#### Messages

| Function | Description |
|---|---|
| [add_flags, async_add_flags](add_flags.md) | flags added |
| [append, async_append](append.md) | APPEND: a message added to a mailbox |
| [copy, async_copy](copy.md) | UID COPY |
| [count, async_count](count.md) | how many messages criteria take |
| [expunge, async_expunge](expunge.md) | EXPUNGE, UID EXPUNGE |
| [fetch, async_fetch](fetch.md) | UID FETCH |
| [fetch_message, async_fetch_message](fetch_message.md) | the whole message of a UID |
| [fetch_unseen, async_fetch_unseen](fetch_unseen.md) | the unseen messages of a mailbox, whole, in one call |
| [idle, async_idle](idle.md) | IDLE: the server's next updates waited for |
| [move, async_move](move.md) | UID MOVE |
| [remove_flags, async_remove_flags](remove_flags.md) | flags removed |
| [search, async_search](search.md) | UID SEARCH |
| [set_flags, async_set_flags](set_flags.md) | flags replaced |
| [sort, async_sort](sort.md) | UID SORT |
| [store, async_store](store.md) | UID STORE, with CONDSTORE's UNCHANGEDSINCE |
| [threads, async_threads](threads.md) | UID THREAD |

## Example

The unseen messages of INBOX, against a server of the same program:

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
    mail.append("alice", "INBOX", "From: Carol <carol@example.com>\r\nSubject: Report\r\n\r\nAttached.\r\n");
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
    auto unseen = session.fetch_unseen();
    for (const net::imap::message& m : *unseen) {
        println("{} from {}: {}", m.uid, m.envelope->from[0].email(), m.envelope->subject);
    }
    srv.close();
}
```

Output:

```text
1 from bob@example.com: Lunch
2 from carol@example.com: Report
```

Over TLS: a server with the tree's test certificate offers STARTTLS, and the client, trusting the test CA, requires it:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    net::imap::server srv;
    srv.backend = mail;
    net::tls::config tls;
    tls.identities = {net::tls::identity(io::read_text("tests/net/tls_testdata/ecdsa.pem"),
                                         crypto::read_secret("tests/net/tls_testdata/ecdsa.key"))};
    srv.tls = tls;
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));

    net::imap::client::options o;
    o.user = "alice";
    o.password = "secret";
    o.tls.roots = crypto::x509::certificate_pool::from_pem(
        io::read_text("tests/net/tls_testdata/ca.pem"));
    string address = "localhost:" + to_string(listener.local_endpoint().port());
    net::imap::client session = net::imap::client::connect(address, o);
    println("{} {}", session.is_tls(), session.has("STARTTLS"));
    srv.close();
}
```

Output:

```text
true false
```

## See also

- [server](../server/README.md): the other side; [criteria](../criteria/README.md): what a search looks for
- [message](../message/README.md), [envelope](../envelope.md), [body_structure](../body_structure/README.md): what a fetch gives
- [tls](../../tls/README.md): the connection under STARTTLS and `imaps://`
- `tests/net/imap/client.cpp` (every method against the module's server over both backends),
  `tests/net/imap/response.cpp` (the responses of the RFCs' examples), `tests/net/imap/interop.cpp`
