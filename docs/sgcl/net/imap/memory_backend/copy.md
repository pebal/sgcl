[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::copy

```cpp
expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to) const;
```

Copies messages into another mailbox, their flags and dates kept, each given the next UID there and one new
mod-sequence; UIDs of no message are passed over.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `from` | the mailbox of the messages |
| `uids` | the messages |
| `to` | the mailbox to copy to |

## Return value

What they became there, in the order of the UIDs; or the error: `errc::nonexistent`, `errc::over_quota`.

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
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n", {net::imap::flag::seen});
    mail.create("alice", "Archive", "");
    auto copied = mail.copy("alice", "INBOX", {1}, "Archive");
    println("UID {} {}", (*copied)[0].uid, (*copied)[0].flags);
}
```

Output:

```text
UID 1 ["\\Seen"]
```

## See also

- [client::copy](../client/copy.md)
- [memory_backend](README.md)
