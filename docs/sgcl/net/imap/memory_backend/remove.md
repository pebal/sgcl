[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::remove

```cpp
expected<void, io::error> remove(const string& user, const string& name) const;
```

Removes a mailbox and its messages; those under it stay. INBOX is `errc::cannot`.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

Nothing; or the error, `errc::nonexistent`.

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
- [memory_backend](README.md)
