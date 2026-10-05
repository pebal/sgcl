[sgcl](../../README.md) › [net](../README.md) › [imap](README.md)

# sgcl::net::imap::flag_update

```cpp
#include "sgcl/net/imap/types.h"   // or "sgcl/net/imap.h"

namespace sgcl::net::imap {
    struct flag_update {
        uint32_t uid = 0;
        vector<string> flags;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

The new flags of a message, as the server asks a [backend](backend/README.md)'s `store` to keep them: the whole set,
not a change of it.

## Member objects

| Member | Description |
|---|---|
| `uid` | the message |
| `flags` | its flags from now on |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    net::imap::flag_update u{1, {net::imap::flag::seen, "$Done"}};
    mail.store("alice", "INBOX", {u});
    println("{}", mail.open("alice", "INBOX")->messages[0].flags);
}
```

Output:

```text
["\\Seen", "$Done"]
```

## See also

- [memory_backend::store](memory_backend/store.md)
- [sgcl::net::imap](README.md)
