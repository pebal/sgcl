[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::mailboxes

```cpp
expected<vector<list_entry>, io::error> mailboxes(const string& user) const;
```

Returns the user's folders: INBOX and every `.Name` directory with a `cur/`, their special use (`sgcl-special-use`)
and `\Subscribed` (the user's `subscriptions` file) in their attributes, the names subscribed without a folder with
`\NonExistent`. A user's Maildir is made when it is missing.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |

## Return value

The [entries](../list_entry/README.md), in no order; or the error.

## Complexity

Linear in the entries of the user's directory.

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
INBOX []
Sent ["\\Sent", "\\Subscribed"]
```

## See also

- [create](create.md), [subscribe](subscribe.md)
- [list_entry](../list_entry/README.md)
- [maildir_backend](README.md)
