[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::errc

```cpp
#include "sgcl/net/imap/error.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    enum class errc {
        no = 1,
        bad,
        bye,
        malformed_response,
        authentication_failed,
        starttls_unavailable,
        not_supported,
        nonexistent,
        already_exists,
        over_quota,
        too_big,
        cannot,
        limit,
        in_use,
        no_perm,
        expunged,
        unavailable,
        privacy_required
    };
}

template<>
struct std::is_error_code_enum<sgcl::net::imap::errc> : std::true_type {};
```

The failures of IMAP that neither `errno` nor [io](../../io/errc.md) names: what a server answered (a tagged NO or
BAD, a BYE), told apart by the response code it gave (RFC 9051 §7.1, RFC 5530), a response that breaks the grammar, a
login refused, a STARTTLS the client requires and the server does not offer, a capability a call needs and the server
lacks. They form the category `"imap"` ([category](category.md)). An error of the client is an
[io::error](../../io/error/README.md) whose operation names the command (`SELECT Archive`) and whose path is the
server's text with its code (`[NONEXISTENT] No such mailbox`), so that `message()` reads `SELECT Archive [NONEXISTENT]
No such mailbox: no such mailbox`.

A [backend](backend/README.md) reports with the same codes, and the server answers each with the response code it
stands for: a backend's `errc::over_quota` is the client's `NO [OVERQUOTA]`. Errors of other categories go out as `NO
[SERVERBUG]`, but a missing file (`NONEXISTENT`), one that exists (`ALREADYEXISTS`), a permission (`NOPERM`) and a
full disk (`OVERQUOTA`). `std::is_error_code_enum` is specialized: `e.code() == net::imap::errc::nonexistent` compares
directly.

| Value | Description |
|---|---|
| `no` | "the server refused the command": a tagged NO without a response code of its own |
| `bad` | "the server rejected the command as invalid": a tagged BAD, the command not understood or not allowed in the state |
| `bye` | "the server closed the connection": a BYE, the server's text in the error's path |
| `malformed_response` | "malformed IMAP response": bytes that break RFC 9051's grammar |
| `authentication_failed` | "authentication failed": LOGIN or AUTHENTICATE refused (AUTHENTICATIONFAILED, AUTHORIZATIONFAILED, EXPIRED) |
| `starttls_unavailable` | "the server does not offer STARTTLS": a client that requires it ([security](security.md)) |
| `not_supported` | "the server does not support the operation": a capability the call needs (SORT, THREAD, QUOTA, UID EXPUNGE) |
| `nonexistent` | "no such mailbox": NONEXISTENT, TRYCREATE |
| `already_exists` | "the mailbox already exists": ALREADYEXISTS |
| `over_quota` | "over quota": OVERQUOTA |
| `too_big` | "too big": TOOBIG, a message or a literal past a limit |
| `cannot` | "the operation cannot succeed": CANNOT, a name the store refuses |
| `limit` | "past a limit of the server": LIMIT |
| `in_use` | "the mailbox is in use": INUSE |
| `no_perm` | "permission denied": NOPERM |
| `expunged` | "the message was expunged": EXPUNGEISSUE, and a UID the store has no message of |
| `unavailable` | "the server is unavailable": UNAVAILABLE |
| `privacy_required` | "TLS required before the credentials": PRIVACYREQUIRED, a server's LOGINDISABLED |

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
    auto missing = session.select("Archive");
    println("{}", missing.error().code() == net::imap::errc::nonexistent);
    println("{}", missing.error().message());
    srv.close();
}
```

Output:

```text
true
SELECT Archive [NONEXISTENT] No such mailbox: no such mailbox
```

## See also

- [category](category.md), [make_error_code](make_error_code.md)
- [io::error](../../io/error/README.md): what carries the code
- [sgcl::net::imap](README.md)
