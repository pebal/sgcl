[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::copy

```cpp
expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to) const;
```

Copies messages into another folder by hard links (a copy of the file where a link cannot be made), their flags and
dates kept, given UIDs there.

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
    net::imap::maildir_backend mail("mail");   // a directory of the program's
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
- [maildir_backend](README.md)
