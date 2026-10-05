[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [maildir_backend](README.md)

# sgcl::net::imap::maildir_backend::add_user

```cpp
expected<void, io::error> add_user(const string& user, const string& password = string()) const;
```

Makes the user's Maildir (`<root>/<user>/` with `cur/`, `new/` and `tmp/`) and keeps the password
[authenticate](authenticate.md) checks, in the handle (an empty one: none).

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user's name, a directory: no `/`, no leading `.` |
| `password` | the password; none by default |

## Return value

Nothing; or the error: `errc::cannot` for a name that is no directory, the system's for one that cannot be made.

## Complexity

That of the directories made.

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
    println("{}", mail.authenticate("alice", "secret"));
    println("{}", mail.add_user("../escape").error().code() == net::imap::errc::cannot);
}
```

Output:

```text
true
true
```

## See also

- [authenticate](authenticate.md)
- [maildir_backend](README.md)
