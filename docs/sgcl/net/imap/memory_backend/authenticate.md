[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::authenticate

```cpp
bool authenticate(const string& user, const string& password) const;
```

Returns whether the password is the user's, as [add_user](add_user.md) set it: what a server without `check_password`
asks of LOGIN and AUTHENTICATE.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `password` | the password |

## Return value

`true` when it is; `false` for a user without a password or none at all.

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
    println("{} {}", mail.authenticate("alice", "secret"), mail.authenticate("bob", ""));
}
```

Output:

```text
true false
```

## See also

- [add_user](add_user.md)
- [server](../server/README.md)'s `check_password`
- [memory_backend](README.md)
