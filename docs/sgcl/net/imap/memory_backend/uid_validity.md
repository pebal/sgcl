[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::uid_validity

```cpp
expected<uint32_t, io::error> uid_validity(const string& user, const string& name) const;
```

Returns a mailbox's UIDVALIDITY, as [open](open.md) gives it, without its messages.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |

## Return value

UIDVALIDITY; or the error, `errc::nonexistent`.

## Complexity

Constant on average.

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
    println("{}", *mail.uid_validity("alice", "INBOX") == mail.open("alice", "INBOX")->uid_validity);
}
```

Output:

```text
true
```

## See also

- [open](open.md)
- [memory_backend](README.md)
