[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::rename

```cpp
expected<void, io::error> rename(const string& user, const string& from, const string& to) const;
```

Renames a folder and those under it by renaming their directories, each with a new UIDVALIDITY. INBOX's messages are
moved into a new folder of the name, INBOX staying, empty (RFC 9051 §6.3.6).

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `from` | the mailbox |
| `to` | its new name |

## Return value

Nothing; or the error: `errc::nonexistent`, `errc::already_exists`, `errc::cannot`.

## Complexity

Linear in the entries of the user's directory.

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
    mail.append("alice", "INBOX", "Subject: old\r\n\r\nx\r\n");
    mail.rename("alice", "INBOX", "Saved");
    println("{} {}", mail.open("alice", "INBOX")->messages.size(), mail.open("alice", "Saved")->messages.size());
}
```

Output:

```text
0 1
```

## See also

- [create](create.md)
- [maildir_backend](README.md)
