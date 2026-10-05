[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::set_quota

```cpp
void set_quota(const string& user, uint64_t storage_kib, uint64_t messages) const;
```

Sets limits of a user's mail: the storage in KiB and the number of messages, 0 for none. An append or a copy past one
is `errc::over_quota`, the server's NO [OVERQUOTA], and [quota](quota.md) gives them (QUOTA, RFC 9208).

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `storage_kib` | the storage limit, KiB |
| `messages` | the limit of messages |

## Return value

None.

## Complexity

Constant.

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
    mail.set_quota("alice", 0, 1);
    mail.append("alice", "INBOX", "Subject: first\r\n\r\nx\r\n");
    auto second = mail.append("alice", "INBOX", "Subject: second\r\n\r\ny\r\n");
    println("{}", second.error().code() == net::imap::errc::over_quota);
}
```

Output:

```text
true
```

## See also

- [quota](quota.md)
- [memory_backend](README.md)
