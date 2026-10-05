[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::mailboxes

```cpp
expected<vector<list_entry>, io::error> mailboxes(const string& user) const;
```

Returns the user's mailboxes: their special use and `\Subscribed` in their attributes, and the names subscribed
without a mailbox with `\NonExistent`. LIST is the server's work over them (patterns, parents, children).

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |

## Return value

The [entries](../list_entry/README.md), in no order; or the error.

## Complexity

Linear in the number of mailboxes.

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
    mail.create("alice", "Sent", net::imap::special_use::sent);
    mail.subscribe("alice", "Sent", true);
    auto boxes = mail.mailboxes("alice");
    for (const net::imap::list_entry& e : *boxes) {
        println("{} {}", e.name, e.attributes);
    }
}
```

Output:

```text
Sent ["\\Sent", "\\Subscribed"]
INBOX []
```

## See also

- [create](create.md), [subscribe](subscribe.md)
- [list_entry](../list_entry/README.md)
- [memory_backend](README.md)
