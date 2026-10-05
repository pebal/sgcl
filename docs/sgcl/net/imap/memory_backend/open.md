[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::open

```cpp
expected<mailbox_contents, io::error> open(const string& user, const string& name) const;
```

Returns a mailbox: its UIDVALIDITY, next UID, highest mod-sequence and the data of its messages, as a server opens it
for a session's SELECT.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

The [contents](../mailbox_contents.md); or the error, `errc::nonexistent`.

## Complexity

Linear in the number of messages.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
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
- [memory_backend](README.md)
