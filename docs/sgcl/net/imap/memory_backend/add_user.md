[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::add_user

```cpp
void add_user(const string& user, const string& password) const;
```

Makes a user with an INBOX and the password [authenticate](authenticate.md) checks (a server's LOGIN, when it has no
`check_password`); a user already there gets the password.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user's name |
| `password` | the password |

## Return value

None.

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
    println("{} {}", mail.authenticate("alice", "secret"), mail.authenticate("alice", "guess"));
    println("{}", (*mail.mailboxes("alice"))[0].name);
}
```

Output:

```text
true false
INBOX
```

## See also

- [authenticate](authenticate.md)
- [memory_backend](README.md)
