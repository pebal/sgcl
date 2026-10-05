[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::subscribe

```cpp
expected<void, io::error> subscribe(const string& user, const string& name, bool on) const;
```

Subscribes the user to a name (or unsubscribes, `on` false); the name need not be a mailbox.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the name |
| `on` | subscribe or unsubscribe |

## Return value

Nothing, or the error.

## Complexity

Linear in the number of subscriptions.

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
    mail.subscribe("alice", "Lists/C++", true);
    auto boxes = mail.mailboxes("alice");
    for (const net::imap::list_entry& e : *boxes) {
        println("{} {}", e.name, e.attributes);
    }
}
```

Output:

```text
INBOX []
Lists/C++ ["\\NonExistent", "\\Subscribed"]
```

## See also

- [mailboxes](mailboxes.md)
- [memory_backend](README.md)
