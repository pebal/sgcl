[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::maildir_backend

```cpp
#include "sgcl/net/imap/maildir.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class maildir_backend;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

Mail in Maildir directories, durable, without a database: the format of qmail, Courier and Dovecot, which other
programs read and write too. A user's INBOX is the Maildir `<root>/<user>/` (with `cur/`, `new/` and `tmp/`), a folder
`a/b` is Maildir++'s `<root>/<user>/.a.b/`, a message a file of its own. A handle of one word, its copies the same
state, safe from many threads and beside other processes using the same directories: another server, a delivery agent
writing into `new/`.

## Rules

- **Files.** A message is delivered through `tmp/` (written whole and flushed to the disk) and renamed into `cur/`,
  so a reader never sees half of it and no lock is needed for it. Its flags are in its name after `:2,` — `D` draft,
  `F` flagged, `R` answered, `S` seen, `T` deleted, and keywords as the letters `a` to `z`, mapped in the folder's
  `sgcl-keywords` (at most 26 a folder: `errc::limit` past them). Its size is in its name (Courier's `,S=`; Dovecot's
  `,W=` read where it is).
- **What IMAP adds** is in each folder's `sgcl-uidlist`: UIDVALIDITY, the next UID, the highest mod-sequence, and per
  message its UID, mod-sequence and flags as last seen. It is rewritten whole through a temporary file and a rename,
  under an `flock` of `sgcl-uidlist.lock`, so that two processes keep it consistent. One that does not read is kept
  as `sgcl-uidlist.broken` and rebuilt, with a new UIDVALIDITY.
- **Files from outside.** A file in `new/` (an MTA's delivery, even with LF line ends, read as CRLF) is moved into
  `cur/` and gets the next UID when the folder is next read; a file gone is an expunge; a flag changed by a rename
  gets a new mod-sequence. The server finds these on NOOP and while a session idles.
- **Names.** Folder names are modified UTF-7 on the disk, a `.` inside a name written `&AC4-`; a name whose directory
  would pass 255 bytes is `errc::cannot`. User names are directories: no `/`, no leading `.`.
- **Passwords** given to [add_user](add_user.md) are kept by the handle, not on the disk; a server's
  `check_password` is the way for a real service. [set_quota](set_quota.md)'s limits are the handle's too.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](maildir_backend.md) | the users' Maildirs under a root |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another store |
| [add_user](add_user.md) | a user's Maildir made, a password kept |
| [path](path.md) | the directory of a mailbox |
| [set_quota](set_quota.md) | limits of a user's storage and messages |

#### The backend's

| Function | Description |
|---|---|
| [append](append.md) | a message delivered |
| [authenticate](authenticate.md) | whether a password is a user's |
| [copy](copy.md) | messages copied by hard links |
| [create](create.md) | a folder made |
| [expunge](expunge.md) | messages removed |
| [mailboxes](mailboxes.md) | a user's folders |
| [open](open.md) | a folder read: UIDs, flags, mod-sequences, dates, sizes |
| [quota](quota.md) | a user's usage and limits |
| [read](read.md) | a message's bytes |
| [remove](remove.md) | a folder removed |
| [rename](rename.md) | a folder renamed |
| [revision](revision.md) | a number that moves with every change of the files |
| [store](store.md) | new flags kept, by renames |
| [subscribe](subscribe.md) | a subscription made or removed |
| [uid_validity](uid_validity.md) | a folder's UIDVALIDITY |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::maildir_backend mail("mail");   // a directory of the program's
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: kept\r\n\r\non disk\r\n", {net::imap::flag::seen});
    mail.create("alice", "Archive/2026", "");
    auto boxes = mail.mailboxes("alice");
    for (const net::imap::list_entry& e : *boxes) {
        println("{} in {}", e.name, mail.path("alice", e.name));
    }
    println("{}", *mail.read("alice", "INBOX", 1));
}
```

Output:

```text
INBOX in mail/alice
Archive/2026 in mail/alice/.Archive.2026
Subject: kept

on disk

```

An [SMTP server](../../smtp/server/README.md) delivering into it, the mail an IMAP server of the same root then
serves (the append writes and flushes the file on the handler's worker):

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/imap.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::imap::maildir_backend mail("mail");
    mail.add_user("bob");
    net::smtp::server mx;
    mx.handle([mail](net::smtp::message m) {
        mail.append("bob", "INBOX", string(m.bytes()));
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(mx.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    net::smtp::send(url, encoding::email("alice@example.com", "bob@example.org", "Hello", "Hi, Bob."));
    mx.close();
    serving.wait();
    auto inbox = mail.open("bob", "INBOX");
    auto text = mail.read("bob", "INBOX", inbox->messages[0].uid);
    println("{} | {}", inbox->messages.size(), encoding::email::parse(*text)->subject());
}
```

Output:

```text
1 | Hello
```

## See also

- [backend](../backend/README.md), [memory_backend](../memory_backend/README.md)
- [server](../server/README.md), [smtp::server](../../smtp/server/README.md)
- `tests/net/imap/maildir.cpp` (the files, what other programs do to them, two handles on one directory)
