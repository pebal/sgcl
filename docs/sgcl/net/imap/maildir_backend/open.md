[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::open

```cpp
expected<mailbox_contents, io::error> open(const string& user, const string& name) const;
```

Returns a folder, read again when its files changed: files of `new/` moved into `cur/` and given UIDs, files gone
dropped, flags changed by renames given mod-sequences, the list written back when anything differed. A user's INBOX is
made when it is missing.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

The [contents](../mailbox_contents.md); or the error, `errc::nonexistent`.

## Complexity

Linear in the number of messages; the files listed when the folder changed.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::maildir_backend mail("mail");   // a directory of the program's
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n", {net::imap::flag::flagged});
    net::imap::mailbox_contents c = mail.open("alice", "INBOX");
    println("next UID {}, {} {}", c.uid_next, c.messages[0].uid, c.messages[0].flags);
}
```

Output:

```text
next UID 2, 1 ["\\Flagged"]
```

## See also

- [read](read.md)
- [mailbox_contents](../mailbox_contents.md)
- [maildir_backend](README.md)
