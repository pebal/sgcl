[sgcl](../../README.md) › [net](../README.md) › pop3

# sgcl::net::pop3

```cpp
#include "sgcl/net/pop3.h"   // namespace sgcl::net::pop3
```

POP3 (RFC 1939) both ways: what Python has in `poplib` for a client and Go leaves to libraries. A
[client](client/README.md) logs in, lists the maildrop's messages with their sizes and unique ids, reads them whole
or their heads, marks them for deletion and removes them at [quit](client/quit.md); a [server](server/README.md)
serves the INBOX of each user of an [imap::backend](../imap/backend/README.md) — a
[memory_backend](../imap/memory_backend/README.md), a [maildir_backend](../imap/maildir_backend/README.md), a
program's own — the store an [imap::server](../imap/server/README.md) serves too, so that the same mail is read
over both.

The extensions a client of today expects are spoken without being asked: CAPA, RESP-CODES and PIPELINING (RFC
2449), STLS (RFC 2595) and TLS from the first byte on port 995 (RFC 8314) over [net::tls](../tls/README.md), AUTH
PLAIN (RFC 5034), APOP, TOP and UIDL.

## The rules

1. An address names the server: `host[:port]` (110 by default, 995 TLS from the first byte), or
   `pop3://user:password@host` and `pop3s://...` with the credentials in it (percent-encoded) when the
   [options](client-options.md) have none.
2. Credentials are not sent in clear text by surprise: `security::automatic` takes STLS and refuses a server that
   offers none (`errc::starttls_unavailable`); the server refuses USER, PASS, APOP and AUTH before STLS when it has
   a TLS config, unless `allow_insecure_auth` says so, and drops what a client pipelined after STLS in clear text.
3. A session sees the maildrop as it was at its login: the numbers of its messages stay what they were, a
   message marked deleted is gone for the session, and only [quit](client/quit.md) removes it; a session that ends
   otherwise removes nothing (RFC 1939 §6). A user has one session at a time; a second is `errc::in_use`.
4. Every call that waits has two forms, `list()` on a thread and `co_await async_list()` in a task.
5. A refusal is an error of the category `"pop3"` ([errc](errc.md)), told apart by the response code the server
   gave (`[IN-USE]`, `[SYS/TEMP]`, `[AUTH]`) or by the command (`no_such_message`); its path is the server's text.
6. A [client](client/README.md) and a [server](server/README.md) are handles of one word; the structures hold
   strings.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"pop3"` |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |

## Classes

| Class | Header | Description |
|---|---|---|
| [client](client/README.md) | `client.h` | a session with a server: list, retrieve, top, remove, quit |
| [client::options](client-options.md) | `client.h` | the credentials, the mechanism, the security, TLS, the timeout, the stop |
| [mailbox_status](mailbox_status.md) | `types.h` | STAT: the messages and their size |
| [message_info](message_info.md) | `types.h` | a message of the maildrop: its number, size and unique id |
| [server](server/README.md) | `server.h` | a POP3 server of the INBOX of each user of an imap::backend |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the failures of POP3 |
| [mechanism](mechanism.md) | `types.h` | how a client logs in: SASL PLAIN, USER and PASS, APOP |
| [security](security.md) | `types.h` | TLS from the first byte, STLS, or none |

## See also

- [imap](../imap/README.md): the same mail over IMAP
- [smtp](../smtp/README.md): sending it
- [Benchmarks](../benchmarks.md)
- RFC 1939, RFC 2449, RFC 2595, RFC 5034; `tests/net/pop3/` (Python's `poplib` against the server)
