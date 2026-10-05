[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::memory_backend

```cpp
#include "sgcl/net/imap/backend.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class memory_backend;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Mail in memory: users, their mailboxes and messages on the managed heap, gone with the process. For tests and for a
server of a program's own data; the default [backend](../backend/README.md) of a [server](../server/README.md). A
handle of one word, its copies the same mail, safe from many threads. A user is made by [add_user](add_user.md) or by
the first call that names it, with an INBOX; a user without a password logs in only through a server's
`check_password`.

Its methods are the [backend](../backend/README.md)'s, so a program may call them itself: [append](append.md) is the
delivery of a message (an SMTP server's handler's), which wakes the server's idling sessions at once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](memory_backend.md) | constructs an empty store |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another store |
| [add_user](add_user.md) | a user with a password and an INBOX |
| [set_quota](set_quota.md) | limits of a user's storage and messages |

#### The backend's

| Function | Description |
|---|---|
| [append](append.md) | a message added: the delivery |
| [authenticate](authenticate.md) | whether a password is a user's |
| [copy](copy.md) | messages copied into another mailbox |
| [create](create.md) | a mailbox made |
| [expunge](expunge.md) | messages removed |
| [mailboxes](mailboxes.md) | a user's mailboxes |
| [open](open.md) | a mailbox's UIDs, flags, mod-sequences, dates and sizes |
| [quota](quota.md) | a user's usage and limits |
| [read](read.md) | a message's bytes |
| [remove](remove.md) | a mailbox removed |
| [rename](rename.md) | a mailbox renamed |
| [revision](revision.md) | a number that moves with every change |
| [store](store.md) | new flags kept |
| [subscribe](subscribe.md) | a subscription made or removed |
| [uid_validity](uid_validity.md) | a mailbox's UIDVALIDITY |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: one\r\n\r\nx\r\n");
    mail.append("alice", "INBOX", "Subject: two\r\n\r\ny\r\n", {net::imap::flag::seen});
    net::imap::mailbox_contents inbox = mail.open("alice", "INBOX");
    for (const net::imap::stored_message& m : inbox.messages) {
        println("{} {} {}", m.uid, m.size, m.flags);
    }
}
```

Output:

```text
1 19 []
2 19 ["\\Seen"]
```

## See also

- [backend](../backend/README.md), [maildir_backend](../maildir_backend/README.md)
- [server](../server/README.md)
