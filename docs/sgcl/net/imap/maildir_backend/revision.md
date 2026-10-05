[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::revision

```cpp
uint64_t revision(const string& user, const string& name) const;
```

Returns a number that moves with every change of the folder's files (the times of `cur/`, `new/` and the list): the
server reads a folder again only when it moved, which is how another process's delivery is found.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

The number; 0 for no such mailbox.

## Complexity

Constant: three `stat` calls.

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
    uint64_t before = mail.revision("alice", "INBOX");
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    println("{}", mail.revision("alice", "INBOX") != before);
}
```

Output:

```text
true
```

## See also

- [open](open.md)
- [maildir_backend](README.md)
