[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md)

# sgcl::net::imap::backend

```cpp
#include "sgcl/net/imap/backend.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    class backend {
    public:
        backend();
        template<class B>
        backend(B b);
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

The store of a [server](../server/README.md), its field `backend`: a [memory_backend](../memory_backend/README.md), a
[maildir_backend](../maildir_backend/README.md), or a type of the program's own with the methods below, taken by value
— a database, a store of another service. A handle of one word, its copies the same store. The server asks it for the
users' mailboxes and messages and does the rest of IMAP over them: sequence numbers, the notifications of other
sessions, SEARCH, SORT and THREAD, the sections and structures of FETCH.

## Rules

- **The methods.** Every method takes the user first; names are UTF-8 with `/` the delimiter, INBOX always written
  so. Messages are kept in ascending order of UID, UIDs given by the backend (UIDNEXT never going back), a
  mod-sequence given to every change (one per call, the mailbox's highest growing with each). Errors are
  [errc](../errc.md)'s, answered by the server with their response codes. The optional ones are found by the
  constructor; a type without them gets the server's way.

| Method | Description |
|---|---|
| `bool authenticate(const string& user, const string& password)` | optional: whether the password is the user's, for a server without `check_password` |
| `expected<vector<list_entry>, io::error> mailboxes(const string& user)` | the user's mailboxes, their special use and `\Subscribed` in their attributes, names subscribed without a mailbox as `\NonExistent` |
| `expected<void, io::error> create(const string& user, const string& name, const string& special_use)` | a mailbox made; `errc::already_exists`, `errc::cannot` |
| `expected<void, io::error> remove(const string& user, const string& name)` | a mailbox removed, those under it kept; `errc::nonexistent` |
| `expected<void, io::error> rename(const string& user, const string& from, const string& to)` | a mailbox and those under it renamed; INBOX's messages into a new mailbox |
| `expected<void, io::error> subscribe(const string& user, const string& name, bool on)` | a subscription made or removed |
| `expected<mailbox_contents, io::error> open(const string& user, const string& name)` | the mailbox: UIDVALIDITY, the next UID, the highest mod-sequence, its messages |
| `uint64_t revision(const string& user, const string& name)` | optional: a number that moves with every change, so that the server reads a mailbox again only when it did |
| `expected<uint32_t, io::error> uid_validity(const string& user, const string& name)` | optional: UIDVALIDITY alone (open() otherwise) |
| `expected<string, io::error> read(const string& user, const string& name, uint32_t uid)` | a message's bytes, CRLF line ends; `errc::expunged` |
| `expected<stored_message, io::error> append(const string& user, const string& name, const string& message, const vector<string>& flags, const time::datetime& date)` | a message added: its UID and mod-sequence from the backend |
| `expected<uint64_t, io::error> store(const string& user, const string& name, const vector<flag_update>& changes)` | new flags kept: the mod-sequence given them |
| `expected<uint64_t, io::error> expunge(const string& user, const string& name, const vector<uint32_t>& uids)` | messages removed: the mod-sequence of the removal |
| `expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to)` | optional: messages copied, flags and dates kept (read and append otherwise) |
| `expected<imap::quota, io::error> quota(const string& user)` | optional: the user's usage and limits (QUOTA offered when there is one) |

- **Threads.** The methods are called from the sessions' tasks, several at once: a backend is safe from many threads.
  They are called on the workers and do not wait for the network (a local disk's calls are made where they are).
- **Changes from outside.** A backend's `revision` lets the server find changes made around it (another process's
  delivery): read again on NOOP and while a session idles, every `poll_interval`. The module's backends also tell a
  server of a change made through their handle at once.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](backend.md) | a memory_backend of its own, or the store given |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another backend |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::server srv;
    srv.backend = net::imap::memory_backend();
    net::imap::backend same = srv.backend;
    println("ok");
}
```

Output:

```text
ok
```

## See also

- [memory_backend](../memory_backend/README.md), [maildir_backend](../maildir_backend/README.md)
- [server](../server/README.md)
- [stored_message](../stored_message.md), [mailbox_contents](../mailbox_contents.md), [flag_update](../flag_update.md)
