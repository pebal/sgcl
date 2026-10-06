[sgcl](../../README.md) › [net](../README.md) › smtp

# sgcl::net::smtp

```cpp
#include "sgcl/net/smtp.h"   // namespace sgcl::net::smtp
```

SMTP (RFC 5321) both ways, on the messages of [encoding::email](../../encoding/email/README.md): what Go has in
`net/smtp` for a client and leaves to libraries for a server, Python's `smtplib` and `smtpd`. A message is sent in
one line, [send](send.md)`(url, message)`; a [client](client/README.md) keeps a session for several;
[deliver](deliver.md) sends a message to its recipients' own exchangers by their MX records. A
[server](server/README.md) takes messages through a handler per message, as an
[http::server](../http/server/README.md) takes requests; what is done with a message is the handler's — there is
no relaying and no queue.

The extensions a submission and a relay use are spoken without being asked: STARTTLS (RFC 3207) and TLS from the
first byte on port 465 (RFC 8314) over [net::tls](../tls/README.md), AUTH PLAIN, LOGIN and XOAUTH2 (RFC 4954,
RFC 4616, RFC 7628), PIPELINING (RFC 2920), 8BITMIME (RFC 6152), SMTPUTF8 (RFC 6531), SIZE (RFC 1870), DSN
(RFC 3461), CHUNKING with BDAT and BINARYMIME (RFC 3030), and the enhanced status codes of RFC 2034 and RFC 3463
in every [reply](reply/README.md).

A receiver checks where a message comes from: [check_sender](check_sender.md) makes the checks of
[SPF](../spf/README.md), [DKIM](../dkim/README.md) and [DMARC](../dmarc/README.md) for a message and its envelope, a
[server](server/README.md) makes them for every message when its `sender_checks` is set and writes what they found
as an [Authentication-Results](authentication_results/README.md) field (RFC 8601); a
[client](client/README.md) signs every message it sends with the [dkim::signer](../dkim/signer/README.md) its
[options](options.md) name.

## The rules

1. A URL names the server: `smtp://host[:port]` (25 by default) upgraded by STARTTLS when the server offers it,
   `smtps://host[:port]` (465 by default) TLS from the first byte; the user and the password in it
   (`smtp://user:password@host:587`, percent-encoded) are the credentials when [options](options.md) has none.
2. TLS is not left out quietly where it was asked for: `options::require_tls` refuses a server without STARTTLS
   (`errc::smtp_tls_required`), bytes after the 220 of STARTTLS in clear text end the session, and AUTH is never
   sent over a connection without TLS unless `allow_insecure_auth` says so. The server's side discards what a client
   pipelined after STARTTLS in clear text (CVE-2011-0411) and offers AUTH over TLS alone by default.
3. A refusal is an error whose path is the reply, `RCPT TO:<b@x> 550 5.1.1 No such user: the SMTP server refused`;
   [reply_of](reply_of.md) reads the [reply](reply/README.md) back. A recipient refused while others are taken is
   not an error: the [receipt](receipt.md) lists it.
4. Every call that waits has two forms, `send(...)` on a thread and `co_await async_send(...)` in a task, as net's
   calls do; the waits are RFC 5321 §4.5.3.2's unless the options give one, and a stop token ends any of them with
   `ECANCELED`.
5. The codes of the module's own are net's [errc](../errc.md): `smtp_reply`, `smtp_auth_failed`,
   `smtp_tls_required`, `smtp_unsupported`, `malformed_smtp_reply`, and `malformed_message` for an
   Authentication-Results value that does not parse.
6. A [client](client/README.md), a [server](server/README.md) and a [message](message/README.md) are handles of one
   word; the structures ([envelope](envelope.md), [options](options.md), [reply](reply/README.md),
   [receipt](receipt.md)) hold strings.

## Functions

| Function | Header | Description |
|---|---|---|
| [check_sender, async_check_sender](check_sender.md) | `sender_checks.h` | SPF, DKIM and DMARC of a message and its envelope |
| [deliver, async_deliver](deliver.md) | `client.h` | a message to its recipients' exchangers, by MX |
| [reply_of](reply_of.md) | `envelope.h` | the reply an error carries |
| [send, async_send](send.md) | `client.h` | a message sent in one line |

## Classes

| Class | Header | Description |
|---|---|---|
| [authentication_results](authentication_results/README.md) | `sender_checks.h` | an Authentication-Results field's value (RFC 8601) |
| [authentication_results::method_result](authentication_results-method_result.md) | `sender_checks.h` | one method's result in it |
| [client](client/README.md) | `client.h` | a session with a server, for several messages |
| [envelope](envelope.md) | `envelope.h` | the reverse path, the recipients and the parameters of a transaction |
| [message](message/README.md) | `server.h` | a message a server received, as its handler gets it |
| [options](options.md) | `envelope.h` | how a client talks to its server |
| [receipt](receipt.md) | `envelope.h` | what a sending gives: the reply and the refused recipients |
| [rejection](rejection.md) | `envelope.h` | a recipient refused, with its reply |
| [reply](reply/README.md) | `envelope.h` | a reply: the code, the enhanced code, the text |
| [sender_checks](sender_checks.md) | `sender_checks.h` | which checks a receiver makes and what it does with them |
| [sender_verdict](sender_verdict/README.md) | `sender_checks.h` | what the checks of one message found |
| [server](server/README.md) | `server.h` | an SMTP server with a handler per message |

## See also

- [encoding::email](../../encoding/email/README.md): the messages
- [net::tls](../tls/README.md): the TLS under STARTTLS and smtps://
- [dns::lookup_mx](../dns/lookup_mx.md): what [deliver](deliver.md) asks
- [Benchmarks](../benchmarks.md)
- RFC 5321 and its extensions; `tests/net/smtp_client.cpp`, `smtp_server.cpp`, `smtp_interop.cpp` (a server written
  by hand in Go, Go's `net/smtp` client, curl)
