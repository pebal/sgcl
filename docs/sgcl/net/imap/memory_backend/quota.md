[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::quota

```cpp
expected<imap::quota, io::error> quota(const string& user) const;
```

Returns what the user's mail takes against the limits [set_quota](set_quota.md) set, root `""`: what QUOTA's
GETQUOTAROOT answers.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |

## Return value

The [quota](../quota.md), or the error.

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
    mail.set_quota("alice", 100, 10);
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    net::imap::quota q = mail.quota("alice");
    println("{} of {} messages, {} of {} KiB", q.messages_used, q.messages_limit, q.storage_used, q.storage_limit);
}
```

Output:

```text
1 of 10 messages, 1 of 100 KiB
```

## See also

- [set_quota](set_quota.md)
- [client::quota](../client/quota.md)
- [memory_backend](README.md)
