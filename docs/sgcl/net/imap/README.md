[sgcl](../../README.md) › [net](../README.md) › imap

# sgcl::net::imap

```cpp
#include "sgcl/net/imap.h"   // namespace sgcl::net::imap
```

IMAP, both sides: a [client](client/README.md) of IMAP4rev2 (RFC 9051) that speaks to IMAP4rev1 servers too, and a
[server](server/README.md) over a [backend](backend/README.md) that keeps the mail — [memory_backend](memory_backend/README.md)
in memory, [maildir_backend](maildir_backend/README.md) in Maildir directories, or a type of the program's own with
the same methods. Go has no IMAP in its standard library, Python has imaplib (a client of raw responses); this module
is what a mail program and a mail service need from the protocol: typed messages, envelopes and MIME structures,
messages parsed by [encoding::email](../../encoding/email/README.md),
searches built as values, flags, UIDs and mod-sequences, IDLE, and the extensions a modern pair of peers expects:
LITERAL+, IDLE, NAMESPACE, UIDPLUS, UNSELECT, MOVE, CONDSTORE and QRESYNC, ESEARCH, SEARCHRES, SPECIAL-USE,
LIST-EXTENDED and LIST-STATUS, ENABLE, ID, SASL-IR, AUTH=PLAIN, LOGIN, XOAUTH2 and OAUTHBEARER, STARTTLS and TLS from
the first byte, UTF8=ACCEPT, COMPRESS=DEFLATE, QUOTA, BINARY, SORT, THREAD, WITHIN, MULTIAPPEND and APPENDLIMIT.

The idea it rests on is that a session is a handle and a message is named by its UID. A client is one word, the
connection and its state behind it: commands from several tasks go out pipelined and each gets its own answer, and
what the server pushes (a new message, an expunge, flags another session changed) reaches the program through the
options' `on_update` and through [idle](client/idle.md). The server keeps a hub per mailbox open in any session:
every change goes through the backend and then to the other sessions that have the mailbox selected, which announce
it at their next command, or at once while they idle, each in its own numbering of the messages. Errors are values,
an `expected<T, io::error>` whose code is the server's answer read into [errc](errc.md) (`nonexistent` for a
`NO [NONEXISTENT]`), a socket's or TLS's.

## The rules

1. Every call that talks to the server has two forms, as in the rest of net: `session.fetch(uids)` blocks the
   calling thread, the exchange running on the scheduler (so never from a worker), and `co_await
   session.async_fetch(uids)` in a task. The server's `serve()` blocks the thread; in a task, `co_await
   srv.async_serve(...)`.
2. A [client](client/README.md), a [server](server/README.md) and the backends are handles of one word, copies the same
   session, server or store, safe from many tasks and threads. They hold a `tracked_ptr`, as the values do that hold a
   string or a vector ([message](message/README.md), [envelope](envelope.md), [sequence_set](sequence_set/README.md)).
3. The client names messages by their UIDs (UID FETCH, UID STORE, UID SEARCH, UID COPY, UID MOVE, UID EXPUNGE): a
   UID stays while sequence numbers move with every expunge. [command](client/command.md) sends what the class has
   no method for.
4. Mailbox names are UTF-8 on both sides. The client enables IMAP4rev2 or UTF8=ACCEPT when the server offers them and
   writes modified UTF-7 (RFC 3501 §5.1.3) otherwise; the server reads and writes either, as its client enabled.
   The hierarchy delimiter of the server is `/`; INBOX is INBOX in any case.
5. TLS: a client requires STARTTLS on a plain port unless told otherwise ([security](security.md)), and speaks TLS
   from the first byte for `imaps://` and port 993. A server with a [tls config](../tls/config.md) offers STARTTLS
   and refuses LOGIN and AUTHENTICATE before it (LOGINDISABLED); what a client sends in the clear behind STARTTLS is
   dropped, never read as protected. `serve_tls` is TLS from the first byte.
6. The codes are [errc](errc.md)'s, in the category `"imap"` ([category](category.md)): the server's refusals by their
   response codes, a malformed response, a failed login, a missing STARTTLS or capability; and the server answers a
   backend's error by the response code its errc stands for. A refused managed allocation ends the program, as
   everywhere in the library.
7. Not in this version: CATENATE (RFC 4469; APPEND of parts of other messages, which needs IMAP URLs), ACL, METADATA,
   NOTIFY, OBJECTID, and THREAD's REFS: rare in clients, each a protocol of its own. A Maildir folder holds at most
   26 keywords (letters a to z in its file names, as Dovecot's Maildir does).

## Functions

| Function | Header | Description |
|---|---|---|
| [category](category.md) | `error.h` | the error category of `errc`, named `"imap"` |
| [make_error_code](make_error_code.md) | `error.h` | an `errc` as an `error_code` |

## Classes

| Class | Header | Description |
|---|---|---|
| [address](address/README.md) | `types.h` | an address of an envelope: the name, the mailbox, the host |
| [backend](backend/README.md) | `backend.h` | the store of a server: a memory_backend, a maildir_backend or a program's own type |
| [body_structure](body_structure/README.md) | `types.h` | the MIME structure of a message: types, parameters, sizes, the parts |
| [client](client/README.md) | `client.h` | an IMAP client connection: connect, log in, the commands as methods |
| [client::options](client-options.md) | `client.h` | how a client connects and logs in |
| [copy_result](copy_result.md) | `types.h` | the UIDs messages got in the mailbox they were copied or moved to |
| [criteria](criteria/README.md) | `criteria.h` | what a search looks for: keys combined with &&, \|\| and ! |
| [envelope](envelope.md) | `types.h` | the envelope of a message: date, subject, addresses, ids |
| [fetch_options](fetch_options.md) | `client.h` | what a fetch asks for: flags, envelope, structure, sections |
| [flag_update](flag_update.md) | `types.h` | the new flags of a message, as a server asks its backend to store them |
| [list_entry](list_entry/README.md) | `types.h` | a mailbox as LIST gives it: name, delimiter, attributes, status |
| [list_options](list_options.md) | `client.h` | what a list asks for: subscribed, special-use, statuses |
| [mailbox_contents](mailbox_contents.md) | `types.h` | a mailbox as a backend opens it |
| [maildir_backend](maildir_backend/README.md) | `maildir.h` | mail in Maildir directories, durable, safe beside other processes |
| [memory_backend](memory_backend/README.md) | `backend.h` | mail in memory: users, mailboxes, messages |
| [message](message/README.md) | `types.h` | a message as FETCH gives it |
| [namespace_entry](namespace_entry.md) | `types.h` | one namespace: its prefix and delimiter |
| [namespaces](namespaces.md) | `types.h` | the namespaces of a server: personal, other users', shared |
| [quota](quota.md) | `types.h` | a quota root's storage and messages, used and limit |
| [select_options](select_options.md) | `client.h` | read only, and QRESYNC's state known from before |
| [selected](selected.md) | `types.h` | what SELECT said of the mailbox opened |
| [sequence_set](sequence_set/README.md) | `types.h` | a set of message numbers or UIDs: "1:4,7,10:*" |
| [server](server/README.md) | `server.h` | an IMAP server over a backend |
| [status](status.md) | `types.h` | what STATUS tells of a mailbox |
| [stored_message](stored_message.md) | `types.h` | a message as a backend keeps it: UID, flags, mod-sequence, date, size |
| [thread](thread.md) | `types.h` | a thread of THREAD: a message and the threads under it |
| [update](update.md) | `types.h` | what the server pushed of the selected mailbox |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the module's error codes |
| [mechanism](mechanism.md) | `client.h` | the SASL mechanism of a client's login |
| [order](order.md) | `client.h` | what SORT orders by |
| [security](security.md) | `client.h` | how a client's connection is protected: TLS, STARTTLS, none |
| [store_mode](store_mode.md) | `client.h` | how STORE changes flags: replace, add, remove |
| [threading](threading.md) | `client.h` | the algorithm of THREAD |
| [update::kind](update-kind.md) | `types.h` | what an update is: a new message, an expunge, flags changed |

## Objects and types

| Name | Header | Description |
|---|---|---|
| `flag::seen`, `flag::answered`, `flag::flagged`, `flag::deleted`, `flag::draft`, `flag::recent` | `types.h` | the system flags as character arrays, `"\\Seen"` and the rest: `session.add_flags(uid, {net::imap::flag::seen})` |
| `last` | `types.h` | `inline constexpr uint32_t last = 0;`: `*` in a [sequence_set](sequence_set/README.md), the last message or the highest UID |
| `special_use::all`, `archive`, `drafts`, `flagged`, `junk`, `sent`, `trash` | `types.h` | the attributes of special-use mailboxes (RFC 6154), `"\\Sent"` and the rest, as a list entry holds them and [create](client/create.md) takes one |

## See also

- [net](../README.md): the connections and listeners under the client and the server
- [tls](../tls/README.md): STARTTLS and `imaps://`
- [compress](../../compress/README.md): the DEFLATE under COMPRESS=DEFLATE
- [encoding::email](../../encoding/email/README.md): a message parsed and built; [message](message/README.md)'s
  `email()` and [smtp](../smtp/README.md)'s server and client use it
- [Benchmarks](benchmarks.md): the client and the server against a minimal Go client and Python's imaplib
