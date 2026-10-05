[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::store

```cpp
expected<uint64_t, io::error> store(const string& user, const string& name, const vector<flag_update>& changes) const;
```

Keeps new flags of messages by renaming their files (new keywords added to `sgcl-keywords`), all given one new
mod-sequence in the list; a file gone meanwhile is passed over.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |
| `changes` | the messages and their [flags](../flag_update.md) |

## Return value

The mod-sequence given; or the error: `errc::nonexistent`; `errc::limit` past 26 keywords (Maildir).

## Complexity

Linear in the number of changes and messages.

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
    mail.append("alice", "INBOX", "Subject: x\r\n\r\ny\r\n");
    mail.store("alice", "INBOX", {{1, {net::imap::flag::seen, "$Work"}}});
    println("{}", mail.open("alice", "INBOX")->messages[0].flags);
}
```

Output:

```text
["\\Seen", "$Work"]
```

## See also

- [flag_update](../flag_update.md)
- [open](open.md)
- [maildir_backend](README.md)
