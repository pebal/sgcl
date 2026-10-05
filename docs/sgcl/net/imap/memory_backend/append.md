[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::append

```cpp
expected<stored_message, io::error> append(const string& user, const string& name, const string& message,
                                           const vector<string>& flags = {},
                                           const time::datetime& date = time::datetime::from_unix(std::time(nullptr),
                                                                                                 time::zone::utc())) const;
```

Adds a message with its flags and internal date, given the next UID and a new mod-sequence: the delivery of a message,
which a program makes itself (an SMTP server's handler) and a server makes for APPEND. A server with the mailbox open
tells its sessions at once, an idling one included.

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
- [memory_backend](README.md)
