[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::remove

```cpp
expected<void, io::error> remove(const string& user, const string& name) const;
```

Removes a folder: its directory renamed out of sight first, then deleted with its files; the folders under it stay.
INBOX is `errc::cannot`.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

Nothing; or the error, `errc::nonexistent`.

## Complexity

Linear in the folder's files.

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
    mail.create("alice", "Old", "");
    mail.remove("alice", "Old");
    println("{}", mail.open("alice", "Old").error().code() == net::imap::errc::nonexistent);
}
```

Output:

```text
true
```

## See also

- [create](create.md)
- [maildir_backend](README.md)
