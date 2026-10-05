[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::append

```cpp
expected<stored_message, io::error> append(const string& user, const string& name, const string& message,
                                           const vector<string>& flags = {},
                                           const time::datetime& date = time::datetime::from_unix(std::time(nullptr),
                                                                                                 time::zone::utc())) const;
```

Delivers a message: written into `tmp/` (its line ends made CRLF), flushed to the disk, its time set to the internal
date, renamed into `cur/` with its flags in its name, given the next UID in the list. Safe beside other deliveries and
servers on the same directories; a server with the folder open tells its sessions at once.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |
| `message` | the message |
| `flags` | its flags |
| `date` | its internal date; now by default |

## Return value

What the message became ([stored_message](../stored_message.md)); or the error: `errc::nonexistent`,
`errc::over_quota`, `errc::limit` past the UIDs.

## Complexity

Linear in the message's size and in the folder's messages (the list rewritten).

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
    net::imap::stored_message m = mail.append("alice", "INBOX", "Subject: hi\r\n\r\nx\r\n", {net::imap::flag::draft});
    println("UID {}, {} bytes, {}", m.uid, m.size, m.flags);
}
```

Output:

```text
UID 1, 18 bytes, ["\\Draft"]
```

## See also

- [read](read.md)
- [client::append](../client/append.md): over IMAP
- [maildir_backend](README.md)
