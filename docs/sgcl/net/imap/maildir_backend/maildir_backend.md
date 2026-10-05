[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::maildir_backend

```cpp
explicit maildir_backend(const string& root);               // (1)
maildir_backend(const maildir_backend& other) = default;    // (2)
```

1. The users' Maildirs under `root`, made when it is missing; nothing else is read until a user's mail is asked
   for.
2. The same state as `other`. A move is the copy.

## Parameters

| Parameter | Description |
|---|---|
| `root` | the directory of the users' Maildirs |
| `other` | another handle |

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
    net::imap::maildir_backend mail("mail");
    mail.add_user("alice", "secret");
    println("{}", mail.path("alice", "INBOX"));
}
```

Output:

```text
mail/alice
```

## See also

- [add_user](add_user.md)
- [maildir_backend](README.md)
