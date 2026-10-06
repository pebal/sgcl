[sgcl](../../README.md) › [net](../README.md) › ldap

# sgcl::net::ldap

```cpp
#include "sgcl/net/ldap.h"   // namespace sgcl::net::ldap
```

An LDAP client (RFC 4511): what Go leaves to `go-ldap` and Python to `ldap3`. A [client](client/README.md)
connects over TCP, TLS from the first byte (`ldaps://`) or StartTLS, binds — simple, SASL PLAIN or EXTERNAL —
and then searches, adds, modifies, renames, compares and removes entries, or sends an extended operation such as
[who_am_i](client/who_am_i.md). A filter is written as RFC 4515 writes it, `(&(objectClass=person)(mail=*@example.com))`,
and read into BER before anything is sent; the entries come back as plain values of strings.

The messages are BER read and written by [encoding::asn1](../../encoding/asn1/README.md). Several operations may be
in flight on one connection at once, from several tasks; each waits for the messages of its own id. Paged results
(RFC 2696) are asked for by a page size and read to the last page; a referral is reported, never followed.

## The rules

1. A URL names the server: `ldap://host[:389]`, `ldaps://host[:636]` (TLS from the first byte), or `host[:port]`;
   a user and password in it are a simple bind's DN and password, made at [connect](client/connect.md).
2. Passwords are not sent in clear text by surprise: `security::automatic` takes StartTLS on `ldap://` and fails
   when the server refuses it; `security::none` is for a loopback or a unix socket.
3. A refusal is an error of the category `"ldap"` whose value is the result code of RFC 4511 ([errc](errc.md));
   [result_of](result_of.md) gives the whole result back: the code, the matched DN, the server's message and the
   referrals.
4. Every call that waits has two forms, `search()` on a thread and `co_await async_search()` in a task.
5. Each operation waits at most the options' timeout for its answer; past it the operation is abandoned
   (AbandonRequest) and its error is `ETIMEDOUT`, the session goes on.
6. A [client](client/README.md) is a handle of one word; the structures hold strings.

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"ldap"` |
| [filter_escape](filter_escape.md) | `client.h` | a value escaped for a filter's text |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |
| [result_of](result_of.md) | `error.h` | the server's result an error carries |

## Classes

| Class | Header | Description |
|---|---|---|
| [attribute](attribute.md) | `types.h` | an attribute of an entry: its name and values |
| [client](client/README.md) | `client.h` | a session with an LDAP server: bind, search, add, modify, rename, compare, remove |
| [client::options](client-options.md) | `client.h` | the security, TLS, the timeout, the stop |
| [entry](entry/README.md) | `types.h` | an entry: its DN and its attributes |
| [modification](modification.md) | `types.h` | one change of a modify |
| [result](result.md) | `error.h` | a server's result: the code, the matched DN, the message, the referrals |
| [search_request](search_request.md) | `types.h` | what a search asks: base, scope, filter, attributes, limits, paging |
| [search_result](search_result.md) | `types.h` | what a search gives: the entries and the references |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [deref](deref.md) | `types.h` | when aliases are followed |
| [errc](errc.md) | `error.h` | the result codes of LDAP and the client's own failures |
| [modify_op](modify_op.md) | `types.h` | how a modification changes an attribute |
| [scope](scope.md) | `types.h` | how far a search looks under its base |
| [security](security.md) | `types.h` | TLS from the first byte, StartTLS, or none |

## See also

- [encoding::asn1](../../encoding/asn1/README.md): the BER of the messages
- [tls](../tls/README.md)
- [Benchmarks](../benchmarks.md)
- RFC 4511, RFC 4513, RFC 4515, RFC 2696, RFC 4532; `tests/net/ldap/` (OpenLDAP's slapd and its command line
  clients against the client and the test server)
