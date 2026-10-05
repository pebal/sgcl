[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::expunge

```cpp
expected<uint64_t, io::error> expunge(const string& user, const string& name, const vector<uint32_t>& uids) const;
```

Removes the messages of the UIDs.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |
| `uids` | the messages |

## Return value

The mod-sequence of the removal; or the error, `errc::nonexistent`.

## Complexity

Linear in the number of messages.

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
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    mail.expunge("alice", "INBOX", {1});
    println("{}", mail.open("alice", "INBOX")->messages.size());
}
```

Output:

```text
0
```

## See also

- [store](store.md)
- [memory_backend](README.md)
