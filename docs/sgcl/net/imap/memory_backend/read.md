[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::read

```cpp
expected<string, io::error> read(const string& user, const string& name, uint32_t uid) const;
```

Returns the bytes of a message.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the mailbox |
| `uid` | the message |

## Return value

The message; or the error, `errc::expunged` for no message of the UID, `errc::nonexistent` for no such mailbox.

## Complexity

Constant: the message is shared, not copied.

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
    mail.append("alice", "INBOX", "Subject: x\r\n\r\nthe body\r\n");
    print("{}", *mail.read("alice", "INBOX", 1));
    println("{}", mail.read("alice", "INBOX", 2).error().code() == net::imap::errc::expunged);
}
```

Output:

```text
Subject: x

the body
true
```

## See also

- [append](append.md)
- [memory_backend](README.md)
