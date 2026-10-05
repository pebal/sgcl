[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::stored_message

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct stored_message {
        uint32_t uid = 0;
        vector<string> flags;
        uint64_t modseq = 1;
        time::datetime internal_date;
        uint64_t size = 0;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

A message as a [backend](backend/README.md) keeps it: its UID, flags, mod-sequence, when it was received and its size.
A backend's `open` gives them in a [mailbox_contents](mailbox_contents.md), its `append` and `copy` give those of the
messages they made; the server keeps them while a session has the mailbox selected.

## Member objects

| Member | Description |
|---|---|
| `uid` | the UID |
| `flags` | the flags: the system flags first, then the keywords |
| `modseq` | the mod-sequence of its last change |
| `internal_date` | when it was received |
| `size` | its octets |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    net::imap::stored_message m = mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    println("UID {}, {} bytes, modseq {}", m.uid, m.size, m.modseq);
}
```

Output:

```text
UID 1, 17 bytes, modseq 2
```

## See also

- [mailbox_contents](mailbox_contents.md)
- [memory_backend](memory_backend/README.md)
- [sgcl::net::imap](README.md)
