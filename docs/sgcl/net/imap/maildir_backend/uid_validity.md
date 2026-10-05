[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::uid_validity

```cpp
expected<uint32_t, io::error> uid_validity(const string& user, const string& name) const;
```

Returns a folder's UIDVALIDITY, the folder read when its files changed.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

UIDVALIDITY; or the error, `errc::nonexistent`.

## Complexity

Constant when the folder did not change.

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
    println("{}", *mail.uid_validity("alice", "INBOX") == mail.open("alice", "INBOX")->uid_validity);
}
```

Output:

```text
true
```

## See also

- [open](open.md)
- [maildir_backend](README.md)
