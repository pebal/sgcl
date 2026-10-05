[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::mailbox_contents

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct mailbox_contents {
        uint32_t uid_validity = 1;
        uint32_t uid_next = 1;
        uint64_t highest_modseq = 1;
        vector<stored_message> messages;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A mailbox as a [backend](backend/README.md)'s `open` gives it: UIDVALIDITY, the next UID, the highest mod-sequence and
the [messages](stored_message.md) in ascending order of UID.

## Member objects

| Member | Description |
|---|---|
| `uid_validity` | UIDVALIDITY |
| `uid_next` | the UID the next message gets |
| `highest_modseq` | the highest mod-sequence |
| `messages` | the messages, ascending UIDs |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    net::imap::mailbox_contents c = mail.open("alice", "INBOX");
    println("{} message, next UID {}", c.messages.size(), c.uid_next);
}
```

Output:

```text
1 message, next UID 2
```

## See also

- [stored_message](stored_message.md)
- [memory_backend](memory_backend/README.md)
- [sgcl::net::imap](README.md)
