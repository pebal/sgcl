[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::revision

```cpp
uint64_t revision(const string& user, const string& name) const;
```

Returns a number that moves with every change of the mailbox: the server reads a mailbox again only when it moved.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

The number; 0 for no such mailbox.

## Complexity

Constant on average.

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
- [memory_backend](README.md)
