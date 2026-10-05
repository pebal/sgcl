[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::path

```cpp
string path(const string& user, const string& mailbox) const;
```

Returns the directory of a mailbox: the user's Maildir for INBOX, `.` and the folder's name in modified UTF-7 under it
for the rest, a `.` of the name written `&AC4-`.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `mailbox` | the mailbox |

## Return value

The path, whether the directory exists or not.

## Complexity

Linear in the length of the name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::maildir_backend mail("mail");
    println("{}", mail.path("alice", "INBOX"));
    println("{}", mail.path("alice", "Mr. Smith/Zażółć"));
}
```

Output:

```text
mail/alice
mail/alice/.Mr&AC4- Smith.Za&AXwA8wFCAQc-
```

## See also

- [maildir_backend](README.md)
